// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_calendar_fetcher.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/base/consent_level.h"
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
         sender: "Birch Calendar Provider"
         description:
            "Fetch a Chrome OS user's Google Calendar event list in order to "
            "display their events in the Birch UI."
          trigger:
              "Birch UI is shown."
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
          last_reviewed: "2024-02-15"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature is off by default."
          policy_exception_justification:
            "Policy is planned, but not yet implemented."
        })");

}  // namespace

BirchCalendarFetcher::BirchCalendarFetcher(Profile* profile)
    : profile_(profile),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile_)) {
  std::vector<std::string> scopes;
  scopes.push_back(GaiaConstants::kCalendarReadOnlyOAuth2Scope);
  url_loader_factory_ = profile_->GetURLLoaderFactory();
  sender_ = std::make_unique<google_apis::RequestSender>(
      std::make_unique<google_apis::AuthService>(
          identity_manager_,
          identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
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
}

void BirchCalendarFetcher::GetCalendarEvents(
    base::Time start_time,
    base::Time end_time,
    google_apis::calendar::CalendarEventListCallback callback) {
  CHECK_LT(start_time, end_time);

  if (!sender_) {
    // This class is in shutdown, don't fetch.
    return;
  }

  sender_->StartRequestWithAuthRetry(
      std::make_unique<google_apis::calendar::CalendarApiEventsRequest>(
          sender_.get(), url_generator_, std::move(callback), start_time,
          end_time));
}

void BirchCalendarFetcher::SetSenderForTest(
    std::unique_ptr<google_apis::RequestSender> sender) {
  sender_ = std::move(sender);
}

void BirchCalendarFetcher::SetBaseUrlForTest(const std::string& base_url) {
  url_generator_.SetBaseUrlForTesting(base_url);  // IN-TEST
}

}  // namespace ash
