// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_SITE_DATA_NODE_DATA_H_
#define COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_SITE_DATA_NODE_DATA_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/performance_manager/graph/node_inline_data.h"
#include "components/performance_manager/public/decorators/site_data_recorder.h"

class GURL;

namespace performance_manager {

class PageNodeImpl;
class SiteDataCache;
class SiteDataReader;
class SiteDataWriter;
class SiteDataRecorder;

// NodeAttachedData used to adorn every page node with a SiteDataWriter.
class SiteDataNodeData : public SiteDataRecorder::Data,
                         public SparseNodeInlineData<SiteDataNodeData> {
 public:
  SiteDataNodeData(const PageNodeImpl* page_node,
                   SiteDataRecorder* site_data_recorder);

  SiteDataNodeData(const SiteDataNodeData&) = delete;
  SiteDataNodeData& operator=(const SiteDataNodeData&) = delete;

  ~SiteDataNodeData() override;

  // Set the SiteDataCache that should be used to create the writer.
  void set_data_cache(SiteDataCache* data_cache) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(data_cache);
    data_cache_ = data_cache;
  }

  // Functions called whenever one of the tracked properties changes.
  void OnMainFrameUrlChanged(const GURL& url, bool page_is_visible);
  void OnIsLoadedIdleChanged(bool is_loaded_idle);
  void OnIsVisibleChanged(bool is_visible);
  void OnIsAudibleChanged(bool audible);
  void OnTitleUpdated();
  void OnFaviconUpdated();

  void Reset();

  SiteDataWriter* writer() const override;

  SiteDataReader* reader() const override;

 private:
  // Convenience alias.
  using FeatureType = SiteDataRecorderHeuristics::FeatureType;

  void SetDataCacheForTesting(SiteDataCache* cache) override;

  // Indicates if a feature usage event should be recorded or ignored.
  bool ShouldRecordFeatureUsageEvent(FeatureType feature_type);

  // Records a feature usage event if necessary.
  void MaybeNotifyBackgroundFeatureUsage(void (SiteDataWriter::*method)(),
                                         FeatureType feature_type);

  // The SiteDataCache used to serve writers for the PageNode owned by this
  // object.
  raw_ptr<SiteDataCache> data_cache_ GUARDED_BY_CONTEXT(sequence_checker_) =
      nullptr;

  // The PageNode that owns this object.
  raw_ptr<const PageNodeImpl> page_node_ GUARDED_BY_CONTEXT(sequence_checker_);

  raw_ptr<SiteDataRecorder> site_data_recorder_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The time at which this tab switched to LoadingState::kLoadedIdle, null if
  // this tab is not currently in that state.
  // SiteDataRecorderHeuristics::IsLoadedIdle() should always be used to check
  // for LoadingState::kLoadedIdle so that if the heuristic is overridden in
  // tests, this variable is kept in sync with the test definition of "loaded
  // and idle".
  base::TimeTicks loaded_idle_time_ GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<SiteDataWriter> writer_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<SiteDataReader> reader_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_SITE_DATA_NODE_DATA_H_
