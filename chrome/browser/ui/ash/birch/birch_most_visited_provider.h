// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_MOST_VISITED_PROVIDER_H_
#define CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_MOST_VISITED_PROVIDER_H_

#include "ash/birch/birch_data_provider.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/history/core/browser/history_types.h"
#include "url/gurl.h"

class Profile;

namespace history {
class HistoryService;
}

namespace ash {

// Managed the most frequently visited URLs for the birch feature. URLs are sent
// to 'BirchModel' to be stored.
class BirchMostVisitedProvider : public BirchDataProvider {
 public:
  explicit BirchMostVisitedProvider(Profile* profile);
  BirchMostVisitedProvider(const BirchMostVisitedProvider&) = delete;
  BirchMostVisitedProvider& operator=(const BirchMostVisitedProvider&) = delete;
  ~BirchMostVisitedProvider() override;

  // BirchDataProvider:
  void RequestBirchDataFetch() override;

  // Callback from history service with the list of most visited URLs.
  void OnGotMostVisitedURLs(history::MostVisitedURLList urls);

  void set_history_service_for_test(history::HistoryService* service) {
    history_service_ = service;
  }

 private:
  const raw_ptr<Profile> profile_;
  raw_ptr<history::HistoryService> history_service_;

  // Task tracker for history requests.
  base::CancelableTaskTracker cancelable_task_tracker_;

  base::WeakPtrFactory<BirchMostVisitedProvider> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_MOST_VISITED_PROVIDER_H_
