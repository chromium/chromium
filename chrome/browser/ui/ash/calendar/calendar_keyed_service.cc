// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/calendar/calendar_keyed_service.h"

#include <string>
#include <vector>

#include "ash/calendar/calendar_controller.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/common/auth_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

using google_apis::RequestSender;
using google_apis::calendar::CalendarApiEventsRequest;
using google_apis::calendar::CalendarEventListCallback;

namespace ash {
namespace {

// OAuth2 scopes.
// TODO(jiamingc@): move this to google_apis/calendar/.
const char kCalendarReadScope[] =
    "https://www.googleapis.com/auth/calendar.readonly";

constexpr net::NetworkTrafficAnnotationTag kCalendarTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("calendar_get_events", R"(
        semantics {
          sender: "Calendar Keyed Service"
          description:
            "Fetch calender events."
          trigger:
              "Chrome OS system tray calendar view is opened by the user."
          data:
            "The request is authenticated with an OAuth2 access token "
            "identifying the Google account."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in settings."
          chrome_policy {
              CalendarIntegrationEnabled {
                CalendarIntegrationEnabled: true
              }
          }
        })");

}  // namespace

CalendarKeyedService::CalendarKeyedService(Profile* profile,
                                           const AccountId& account_id)
    : profile_(profile), account_id_(account_id), calendar_client_(profile) {
  identity_manager_ = IdentityManagerFactory::GetForProfile(profile_);
  // Instance check for tests.
  if (Shell::HasInstance()) {
    Shell::Get()->calendar_controller()->RegisterClientForUser(
        account_id_, &calendar_client_);
  }
  Initialize();
}

CalendarKeyedService::~CalendarKeyedService() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void CalendarKeyedService::Initialize() {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::vector<std::string> scopes;
  scopes.push_back(kCalendarReadScope);
  url_loader_factory_ = profile_->GetURLLoaderFactory();
  sender_ = std::make_unique<RequestSender>(
      std::make_unique<google_apis::AuthService>(
          identity_manager_,
          identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
          url_loader_factory_, scopes),
      url_loader_factory_,
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})
          .get(),
      /*custom_user_agent=*/std::string(), kCalendarTrafficAnnotation);
}

void CalendarKeyedService::SetUrlForTesting(const std::string& url) {
  url_generator_.SetBaseUrlForTesting(url);  // IN-TEST
}

void CalendarKeyedService::Shutdown() {
  if (Shell::HasInstance()) {
    Shell::Get()->calendar_controller()->RegisterClientForUser(
        account_id_,
        /*client=*/nullptr);
  }
  // Reset `sender_` early to prevent a crash during destruction of
  // CalendarKeyedService. See https://crbug.com/1319563.
  sender_.reset();
}

base::OnceClosure CalendarKeyedService::GetEventList(
    CalendarEventListCallback callback,
    const base::Time& start_time,
    const base::Time& end_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(callback);
  DCHECK_LT(start_time, end_time);

  if (!sender_) {
    std::move(callback).Run(google_apis::OTHER_ERROR, nullptr);
    return base::DoNothing();
  }

  std::unique_ptr<CalendarApiEventsRequest> request =
      std::make_unique<CalendarApiEventsRequest>(sender_.get(), url_generator_,
                                                 std::move(callback),
                                                 start_time, end_time);

  return sender_->StartRequestWithAuthRetry(std::move(request));
}

}  // namespace ash
