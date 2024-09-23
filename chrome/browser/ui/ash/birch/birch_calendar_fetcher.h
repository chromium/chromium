// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_CALENDAR_FETCHER_H_
#define CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_CALENDAR_FETCHER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_requests.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "google_apis/calendar/calendar_api_url_generator.h"
#include "google_apis/common/api_error_codes.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class Profile;

namespace google_apis {
class RequestSender;
}

namespace ash {

class RefreshTokenWaiter;

// Fetches calendar events using the Google Calendar public API.
class BirchCalendarFetcher {
 public:
  explicit BirchCalendarFetcher(Profile* profile);
  BirchCalendarFetcher(const BirchCalendarFetcher&) = delete;
  BirchCalendarFetcher& operator=(const BirchCalendarFetcher&) = delete;
  virtual ~BirchCalendarFetcher();

  void Shutdown();

  // Fetches calendar events for the primary account between `start_time` and
  // `end_time` and invokes `callback` with the events or an error. Virtual for
  // testing.
  virtual void GetCalendarEvents(
      base::Time start_time,
      base::Time end_time,
      google_apis::calendar::CalendarEventListCallback callback);

  void SetSenderForTest(std::unique_ptr<google_apis::RequestSender> sender);
  void SetBaseUrlForTest(const std::string& base_url);

 private:
  // Starts the network request.
  void StartRequest();

  const raw_ptr<Profile> profile_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<google_apis::RequestSender> sender_;
  google_apis::calendar::CalendarApiUrlGenerator url_generator_;

  // Waits for refresh tokens to be loaded. Tokens may not be loaded early in
  // startup.
  std::unique_ptr<RefreshTokenWaiter> refresh_token_waiter_;

  // The time range for the current fetch.
  base::Time start_time_;
  base::Time end_time_;

  // Callback for the current fetch.
  google_apis::calendar::CalendarEventListCallback callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_CALENDAR_FETCHER_H_
