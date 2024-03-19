// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_CALENDAR_PROVIDER_H_
#define CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_CALENDAR_PROVIDER_H_

#include <memory>

#include "ash/birch/birch_data_provider.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "google_apis/common/api_error_codes.h"

class Profile;

namespace ash {

class BirchCalendarFetcher;

// Manages fetching calendar events for the birch feature. Fetched events are
// sent to the `BirchModel` to be stored.
class BirchCalendarProvider : public BirchDataProvider {
 public:
  explicit BirchCalendarProvider(Profile* profile);
  BirchCalendarProvider(const BirchCalendarProvider&) = delete;
  BirchCalendarProvider& operator=(const BirchCalendarProvider&) = delete;
  ~BirchCalendarProvider() override;

  void Initialize();
  void Shutdown();

  // BirchDataProvider:
  void RequestBirchDataFetch() override;

  void SetFetcherForTest(std::unique_ptr<BirchCalendarFetcher> fetcher);

 private:
  // Callback for network response with events.
  void OnEventsFetched(
      google_apis::ApiErrorCode error,
      std::unique_ptr<google_apis::calendar::EventList> events);

  raw_ptr<Profile> profile_;
  std::unique_ptr<BirchCalendarFetcher> fetcher_;
  bool is_fetching_ = false;
  base::WeakPtrFactory<BirchCalendarProvider> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_CALENDAR_PROVIDER_H_
