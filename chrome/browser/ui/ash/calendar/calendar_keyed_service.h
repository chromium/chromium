// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CALENDAR_CALENDAR_KEYED_SERVICE_H_
#define CHROME_BROWSER_UI_ASH_CALENDAR_CALENDAR_KEYED_SERVICE_H_

#include <memory>

#include "ash/shell.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/ui/ash/calendar/calendar_client_impl.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service.h"
#include "google_apis/calendar/calendar_api_requests.h"
#include "google_apis/calendar/calendar_api_url_generator.h"
#include "google_apis/common/auth_service_interface.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/core_account_id.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {
class Shell;
}  // namespace ash

namespace google_apis {

class RequestSender;

namespace calendar {
class CalendarApiUrlGenerator;
}  // namespace calendar
}  // namespace google_apis

namespace signin {
class IdentityManager;
}

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {

// Browser context keyed service that manages the calendar api service
// per-profile.
class CalendarKeyedService : public KeyedService {
 public:
  CalendarKeyedService(Profile* profile, const AccountId& account_id);
  CalendarKeyedService(const CalendarKeyedService& other) = delete;
  CalendarKeyedService& operator=(const CalendarKeyedService& other) = delete;
  ~CalendarKeyedService() override;

  void Initialize();

  // Fetches calendar events based on the current client's account.
  //
  // `callback` will be called when response or google_apis's ERROR (if the call
  // is not successful) is returned. `google_apis::OTHER_ERROR` will be passed
  // in the `callback` if the current client has no valid canlendar service,
  // e.g. a guest user.
  //
  // `end_time` must be greater than `start_time`.
  //
  // The returned `base::OnceClosure` callback can cancel the api call and get
  // the `google_apis::CANCEL` error before the response is back.
  base::OnceClosure GetEventList(
      google_apis::calendar::CalendarEventListCallback callback,
      const base::Time& start_time,
      const base::Time& end_time);

  CalendarClient* client() { return &calendar_client_; }

  void set_sender_for_testing(
      std::unique_ptr<google_apis::RequestSender> test_sender) {
    sender_ = std::move(test_sender);
  }

  // The `url` will be set as the `url_generator_`'s base url.
  void SetUrlForTesting(const std::string& url);

 private:
  // KeyedService:
  void Shutdown() override;

  // The class is expected to run on UI thread.
  base::ThreadChecker thread_checker_;
  Profile* const profile_;
  const AccountId account_id_;
  CalendarClientImpl calendar_client_;
  signin::IdentityManager* identity_manager_;
  CoreAccountId core_account_id_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<google_apis::RequestSender> sender_;
  google_apis::calendar::CalendarApiUrlGenerator url_generator_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_CALENDAR_CALENDAR_KEYED_SERVICE_H_
