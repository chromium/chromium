// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_ZERO_STATE_SYNC_CONTROLLER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_ZERO_STATE_SYNC_CONTROLLER_H_

#include "base/callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace app_list {

class SearchControllerImplNew;

// Manages the synchronization logic of zero-state searches. Owned by the
// SearchControllerImplNew.
//
// ZeroStateSyncController tracks the return of all zero-state-blocking
// providers. It will trigger result publication by the search controller on
// fulfilment of either of these conditions:
//
// 1) All zero-state blocking providers have returned, or
// 2) The zero-state timeout period has elapsed.
class ZeroStateSyncController {
 public:
  using PublishCallback = base::RepeatingCallback<void()>;

  explicit ZeroStateSyncController(SearchControllerImplNew* search_controller);
  ~ZeroStateSyncController();

  ZeroStateSyncController(const ZeroStateSyncController&) = delete;
  ZeroStateSyncController& operator=(const ZeroStateSyncController&) = delete;

  void AddProvider(SearchProvider* provider);
  void Start(base::TimeDelta timeout, base::OnceClosure on_zero_state_done);
  void Stop();
  void UpdateResults(const SearchProvider* provider);

 private:
  void OnZeroStateTimedOut();

  // How many search providers should block zero-state until they return
  // results.
  int total_blockers_ = 0;

  // How many zero-state blocking providers have returned for this search.
  int returned_blockers_ = 0;

  // A timer to trigger a Publish (by the search controller). Triggered at the
  // end of the timeout period passed to Start.
  base::OneShotTimer timeout_;

  // The callback to indicate zero-state should be published. It is reset after
  // calling, and has_value is used as a flag for whether zero-state has
  // published.
  absl::optional<base::OnceClosure> on_done_;

  SearchControllerImplNew* search_controller_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_ZERO_STATE_SYNC_CONTROLLER_H_
