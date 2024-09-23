// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_INPUT_DELEGATE_TAB_SESSION_SOURCE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_INPUT_DELEGATE_TAB_SESSION_SOURCE_H_

#include "base/memory/raw_ptr.h"
#include "components/segmentation_platform/embedder/tab_fetcher.h"
#include "components/segmentation_platform/public/input_delegate.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"

namespace segmentation_platform::processing {

// Input delegate that provides data about a synced tab, handles
// CustomInput::FILL_TAB_METRICS.
class TabSessionSource : public InputDelegate {
 public:
  // Input index: Time since last time the tab was modified.
  static constexpr int kInputTimeSinceModifiedSec = 0;
  // Input index: Time since the last navigation started.
  static constexpr int kInputTimeSinceLastNavSec = 1;
  // Input index: Time since the first navigation on the tab started.
  static constexpr int kInputTimeSinceFirstNavSec = 2;
  // Input index: The type of last transition: `ui::PageTransition`.
  static constexpr int kInputLastTransitionType = 3;
  // Input index: The number navigations with a password field in the tab.
  // Removed 06/2024.
  static constexpr int kInputPasswordFieldCount = 4;
  // Input index: The tab's rank when sorted by modification time across all
  // other tabs in the same session.
  static constexpr int kInputTabRankInSession = 5;
  // Input index: The tab's rank when sorted by modification time across all
  // other sessions.
  static constexpr int kInputSessionRank = 6;
  static constexpr int kInputLocalTabTimeSinceModified = 7;
  static constexpr int kNumInputs = 8;

  TabSessionSource(sync_sessions::SessionSyncService* session_sync_service,
                   TabFetcher* tab_fetcher);
  ~TabSessionSource() override;

  TabSessionSource(const TabSessionSource&) = delete;
  TabSessionSource& operator=(const TabSessionSource&) = delete;

  // InputDelegate impl:
  void Process(const proto::CustomInput& input,
               FeatureProcessorState& feature_processor_state,
               ProcessedCallback callback) override;

  // Returns a bucketized value, with bucket in exponent of 2. The value is on original units
  // with reduced accuracy. `max_buckets` has a limit  of 64 given value is 64 bits.
  static float BucketizeExp(int64_t value, int max_buckets);

  // Returns a bucketized value, with linear buckets of size 1. The value is on original units
  // with reduced accuracy.
  static float BucketizeLinear(int64_t value, int max_buckets);

 protected:
  // Adds info about local tabs, to be implemented by the chrome layer which
  // knows about local tabs.
  virtual void AddLocalTabInfo(const TabFetcher::Tab& tab,
                               FeatureProcessorState& feature_processor_state,
                               Tensor& inputs);

 private:
  void AddTabInfo(const sessions::SessionTab* session_tab, Tensor& inputs);
  void AddTabRanks(const std::string& session_tag,
                   const sessions::SessionTab* session_tab,
                   Tensor& inputs);

  const raw_ptr<sync_sessions::SessionSyncService> session_sync_service_;
  const raw_ptr<TabFetcher, DanglingUntriaged> tab_fetcher_;
};

}  // namespace segmentation_platform::processing

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_INPUT_DELEGATE_TAB_SESSION_SOURCE_H_
