// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/site_data_node_data.h"

#include "base/check_op.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/persistence/site_data/site_data_cache.h"
#include "components/performance_manager/persistence/site_data/site_data_writer.h"
#include "components/performance_manager/public/decorators/site_data_recorder.h"
#include "components/performance_manager/public/persistence/site_data/site_data_reader.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace performance_manager {

namespace {

TabVisibility GetPageNodeVisibility(const PageNode* page_node) {
  return page_node->IsVisible() ? TabVisibility::kForeground
                                : TabVisibility::kBackground;
}

}  // namespace

SiteDataNodeData::SiteDataNodeData(const PageNodeImpl* page_node,
                                   SiteDataRecorder* site_data_recorder)
    : page_node_(page_node), site_data_recorder_(site_data_recorder) {}

SiteDataNodeData::~SiteDataNodeData() = default;

void SiteDataNodeData::OnMainFrameUrlChanged(const GURL& url,
                                             bool page_is_visible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  url::Origin origin = url::Origin::Create(url);

  if (writer_ && origin == writer_->Origin()) {
    return;
  }

  // If the origin has changed then the writer should be invalidated.
  Reset();

  if (!url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  if (!data_cache_) {
    // There is no SiteDataCache if this PageNode is in a browser context that
    // doesn't enable keyed services (such as the system profile).
    return;
  }

  writer_ = data_cache_->GetWriterForOrigin(origin);
  reader_ = data_cache_->GetReaderForOrigin(origin);

  // The writer is assumed not to be LoadingState::kLoadedIdle at this point.
  // Make adjustments if it is LoadingState::kLoadedIdle.
  if (site_data_recorder_->heuristics_impl().IsLoadedIdle(
          page_node_->GetLoadingState())) {
    OnIsLoadedIdleChanged(true);
  }

  DCHECK_EQ(site_data_recorder_->heuristics_impl().IsLoadedIdle(
                page_node_->GetLoadingState()),
            !loaded_idle_time_.is_null());
}

void SiteDataNodeData::OnIsLoadedIdleChanged(bool is_loaded_idle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!writer_) {
    return;
  }

  // This should only be called when the loading state actually changes, or
  // when the writer is first created. In all cases `loaded_idle_time_` should
  // only be set if the site is already loaded, meaning `is_loaded_idle` is
  // now changing to false.
  CHECK_EQ(is_loaded_idle, loaded_idle_time_.is_null());
  if (is_loaded_idle) {
    writer_->NotifySiteLoaded(GetPageNodeVisibility(page_node_));
    loaded_idle_time_ = base::TimeTicks::Now();
  } else {
    writer_->NotifySiteUnloaded(GetPageNodeVisibility(page_node_));
    loaded_idle_time_ = base::TimeTicks();
  }
  CHECK_EQ(is_loaded_idle, !loaded_idle_time_.is_null());
}

void SiteDataNodeData::OnIsVisibleChanged(bool is_visible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!writer_) {
    return;
  }
  if (is_visible) {
    writer_->NotifySiteForegrounded(
        site_data_recorder_->heuristics_impl().IsLoadedIdle(
            page_node_->GetLoadingState()));
  } else {
    writer_->NotifySiteBackgrounded(
        site_data_recorder_->heuristics_impl().IsLoadedIdle(
            page_node_->GetLoadingState()));
  }
}

void SiteDataNodeData::OnIsAudibleChanged(bool audible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!audible) {
    return;
  }

  MaybeNotifyBackgroundFeatureUsage(
      &SiteDataWriter::NotifyUsesAudioInBackground, FeatureType::kAudioUsage);
}

void SiteDataNodeData::OnTitleUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeNotifyBackgroundFeatureUsage(
      &SiteDataWriter::NotifyUpdatesTitleInBackground,
      FeatureType::kTitleChange);
}

void SiteDataNodeData::OnFaviconUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeNotifyBackgroundFeatureUsage(
      &SiteDataWriter::NotifyUpdatesFaviconInBackground,
      FeatureType::kFaviconChange);
}

void SiteDataNodeData::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (writer_ && !loaded_idle_time_.is_null() &&
      site_data_recorder_->heuristics_impl().IsLoadedIdle(
          page_node_->GetLoadingState())) {
    writer_->NotifySiteUnloaded(GetPageNodeVisibility(page_node_));
    loaded_idle_time_ = base::TimeTicks();
  }
  writer_.reset();
  reader_.reset();
}

SiteDataWriter* SiteDataNodeData::writer() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return writer_.get();
}

SiteDataReader* SiteDataNodeData::reader() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return reader_.get();
}

void SiteDataNodeData::SetDataCacheForTesting(SiteDataCache* cache) {
  set_data_cache(cache);
}

bool SiteDataNodeData::ShouldRecordFeatureUsageEvent(FeatureType feature_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The feature usage should be ignored if there's no writer for this page.
  if (!writer_) {
    return false;
  }

  const SiteDataRecorderHeuristics& heuristics =
      site_data_recorder_->heuristics_impl();
  if (!heuristics.IsLoadedIdle(page_node_->GetLoadingState())) {
    return false;
  }
  CHECK(!loaded_idle_time_.is_null());
  return heuristics.IsOutsideLoadingGracePeriod(
             page_node_, feature_type,
             base::TimeTicks::Now() - loaded_idle_time_) &&
         heuristics.IsInBackground(page_node_) &&
         heuristics.IsOutsideBackgroundingGracePeriod(
             page_node_, feature_type,
             page_node_->GetTimeSinceLastVisibilityChange());
}

void SiteDataNodeData::MaybeNotifyBackgroundFeatureUsage(
    void (SiteDataWriter::*method)(),
    FeatureType feature_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ShouldRecordFeatureUsageEvent(feature_type)) {
    return;
  }

  (writer_.get()->*method)();
}

}  // namespace performance_manager
