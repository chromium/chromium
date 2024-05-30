// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_calendar_fetcher.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/birch/refresh_token_waiter.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/calendar/calendar_api_requests.h"
#include "google_apis/common/auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_constants.h"

namespace ash {
namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("birch_calendar_provider", R"(
       semantics {
         sender: "Post-login glanceables"
         description:
            "Fetch a Chrome OS user's Google Calendar event list in order to "
            "display their events. Events appear in suggestion chip buttons "
            "for activities the user might want to perform after login or "
            "from overview mode (e.g. view an upcoming calendar event)."
          trigger:
              "User logs in to device or enters overview mode."
          data:
            "The request is authenticated with an OAuth2 access token "
            "identifying the Google account."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "jamescook@google.com"
            }
            contacts {
              email: "chromeos-launcher@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
          }
          last_reviewed: "2024-05-30"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature can be enabled/disabled by the user in the "
            "suggestion chip button context menu."
          chrome_policy {
            CalendarIntegrationEnabled {
              CalendarIntegrationEnabled: false
            }
            ContextualGoogleIntegrationsEnabled {
              ContextualGoogleIntegrationsEnabled: false
            }
          }
        })");

}  // namespace

BirchCalendarFetcher::BirchCalendarFetcher(Profile* profile)
    : profile_(profile),
      refresh_token_waiter_(std::make_unique<RefreshTokenWaiter>(profile_)) {
  std::vector<std::string> scopes;
  scopes.push_back(GaiaConstants::kCalendarReadOnlyOAuth2Scope);
  url_loader_factory_ = profile_->GetURLLoaderFactory();
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  sender_ = std::make_unique<google_apis::RequestSender>(
      std::make_unique<google_apis::AuthService>(
          identity_manager,
          identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
          url_loader_factory_, scopes),
      url_loader_factory_,
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})
          .get(),
      /*custom_user_agent=*/std::string(), kTrafficAnnotation);
}

BirchCalendarFetcher::~BirchCalendarFetcher() {}

void BirchCalendarFetcher::Shutdown() {
  sender_.reset();
  refresh_token_waiter_.reset();
}

void BirchCalendarFetcher::GetCalendarEvents(
    base::Time start_time,
    base::Time end_time,
    google_apis::calendar::CalendarEventListCallback callback) {
  CHECK_LT(start_time, end_time);

  if (!sender_ || !refresh_token_waiter_) {
    // This class is in shutdown, don't fetch.
    return;
  }

  start_time_ = start_time;
  end_time_ = end_time;
  callback_ = std::move(callback);

  // Ensure refresh tokens are loaded before starting the request. Unretained is
  // safe because of the shutdown check for `refresh_token_waiter_` above.
  refresh_token_waiter_->Wait(base::BindOnce(
      &BirchCalendarFetcher::StartRequest, base::Unretained(this)));
}

void BirchCalendarFetcher::StartRequest() {
  sender_->StartRequestWithAuthRetry(
      std::make_unique<google_apis::calendar::CalendarApiEventsRequest>(
          sender_.get(), url_generator_, std::move(callback_), start_time_,
          end_time_, /*include_attachments=*/true));
}

void BirchCalendarFetcher::SetSenderForTest(
    std::unique_ptr<google_apis::RequestSender> sender) {
  sender_ = std::move(sender);
}

void BirchCalendarFetcher::SetBaseUrlForTest(const std::string& base_url) {
  url_generator_.SetBaseUrlForTesting(base_url);  // IN-TEST
}

}  // namespace ash
