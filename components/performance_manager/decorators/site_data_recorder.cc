// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/site_data_recorder.h"

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/performance_manager/graph/node_attached_data_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/persistence/site_data/site_data_cache.h"
#include "components/performance_manager/persistence/site_data/site_data_cache_factory.h"
#include "components/performance_manager/persistence/site_data/site_data_writer.h"
#include "components/performance_manager/public/persistence/site_data/site_data_reader.h"

namespace performance_manager {

// Provides SiteData machinery access to some internals of a PageNodeImpl.
class SiteDataAccess {
 public:
  static std::unique_ptr<NodeAttachedData>* GetUniquePtrStorage(
      PageNodeImpl* page_node) {
    return &page_node->GetSiteData(base::PassKey<SiteDataAccess>());
  }
};

namespace {

// The period of time after loading during which we ignore title/favicon
// change events. It's possible for some site that are loaded in background to
// use some of these features without this being an attempt to communicate
// with the user (e.g. the page is just really finishing to load).
constexpr base::TimeDelta kTitleOrFaviconChangePostLoadGracePeriod =
    base::Seconds(20);

// The period of time during which audio usage gets ignored after a page gets
// backgrounded. It's necessary because there might be a delay between a media
// request gets initiated and the time the audio actually starts.
constexpr base::TimeDelta kFeatureUsagePostBackgroundGracePeriod =
    base::Seconds(10);

SiteDataRecorder* g_site_data_recorder = nullptr;

TabVisibility GetPageNodeVisibility(const PageNode* page_node) {
  return page_node->IsVisible() ? TabVisibility::kForeground
                                : TabVisibility::kBackground;
}

// Returns the global heuristics implementation held in the SiteDataRecorder.
const SiteDataRecorderHeuristics& RecorderHeuristics() {
  CHECK(g_site_data_recorder);
  return g_site_data_recorder->heuristics_impl();
}

// Default implementation of SiteDataRecorderHeuristics that's used in
// production.
class DefaultHeuristics final : public SiteDataRecorderHeuristics {
 public:
  DefaultHeuristics() = default;
  ~DefaultHeuristics() final = default;

  DefaultHeuristics(const DefaultHeuristics& other) = delete;
  DefaultHeuristics& operator=(const DefaultHeuristics&) = delete;

  bool IsLoadedIdle(PageNode::LoadingState loading_state) const final {
    return DefaultIsLoadedIdle(loading_state);
  }

  bool IsInBackground(const PageNode* page_node) const final {
    return DefaultIsInBackground(page_node);
  }

  bool IsOutsideLoadingGracePeriod(
      const PageNode* page_node,
      FeatureType feature_type,
      base::TimeDelta time_since_load) const final {
    return DefaultIsOutsideLoadingGracePeriod(page_node, feature_type,
                                              time_since_load);
  }

  bool IsOutsideBackgroundingGracePeriod(
      const PageNode* page_node,
      FeatureType feature_type,
      base::TimeDelta time_since_backgrounding) const final {
    return DefaultIsOutsideBackgroundingGracePeriod(page_node, feature_type,
                                                    time_since_backgrounding);
  }
};

// NodeAttachedData used to adorn every page node with a SiteDataWriter.
class SiteDataNodeData : public NodeAttachedDataImpl<SiteDataNodeData>,
                         public SiteDataRecorder::Data {
 public:
  struct Traits : public NodeAttachedDataOwnedByNodeType<PageNodeImpl> {};

  explicit SiteDataNodeData(const PageNodeImpl* page_node)
      : page_node_(page_node) {}

  SiteDataNodeData(const SiteDataNodeData&) = delete;
  SiteDataNodeData& operator=(const SiteDataNodeData&) = delete;

  ~SiteDataNodeData() override = default;

  // NodeAttachedData:
  static std::unique_ptr<NodeAttachedData>* GetUniquePtrStorage(
      PageNodeImpl* page_node) {
    return SiteDataAccess::GetUniquePtrStorage(page_node);
  }

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

  SiteDataWriter* writer() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return writer_.get();
  }

  SiteDataReader* reader() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return reader_.get();
  }

 private:
  // Convenience alias.
  using FeatureType = SiteDataRecorderHeuristics::FeatureType;

  void SetDataCacheForTesting(SiteDataCache* cache) override {
    set_data_cache(cache);
  }

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
  raw_ptr<const PageNodeImpl> page_node_ GUARDED_BY_CONTEXT(sequence_checker_) =
      nullptr;

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

void SiteDataNodeData::OnMainFrameUrlChanged(const GURL& url,
                                             bool page_is_visible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  url::Origin origin = url::Origin::Create(url);

  if (writer_ && origin == writer_->Origin())
    return;

  // If the origin has changed then the writer should be invalidated.
  Reset();

  if (!url.SchemeIsHTTPOrHTTPS())
    return;

  writer_ = data_cache_->GetWriterForOrigin(origin);
  reader_ = data_cache_->GetReaderForOrigin(origin);

  // The writer is assumed not to be LoadingState::kLoadedIdle at this point.
  // Make adjustments if it is LoadingState::kLoadedIdle.
  if (RecorderHeuristics().IsLoadedIdle(page_node_->loading_state())) {
    OnIsLoadedIdleChanged(true);
  }

  DCHECK_EQ(RecorderHeuristics().IsLoadedIdle(page_node_->loading_state()),
            !loaded_idle_time_.is_null());
}

void SiteDataNodeData::OnIsLoadedIdleChanged(bool is_loaded_idle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!writer_)
    return;

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
  if (!writer_)
    return;
  if (is_visible) {
    writer_->NotifySiteForegrounded(
        RecorderHeuristics().IsLoadedIdle(page_node_->loading_state()));
  } else {
    writer_->NotifySiteBackgrounded(
        RecorderHeuristics().IsLoadedIdle(page_node_->loading_state()));
  }
}

void SiteDataNodeData::OnIsAudibleChanged(bool audible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!audible)
    return;

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
      RecorderHeuristics().IsLoadedIdle(page_node_->loading_state())) {
    writer_->NotifySiteUnloaded(GetPageNodeVisibility(page_node_));
    loaded_idle_time_ = base::TimeTicks();
  }
  writer_.reset();
  reader_.reset();
}

bool SiteDataNodeData::ShouldRecordFeatureUsageEvent(FeatureType feature_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The feature usage should be ignored if there's no writer for this page.
  if (!writer_) {
    return false;
  }

  const SiteDataRecorderHeuristics& heuristics = RecorderHeuristics();
  if (!heuristics.IsLoadedIdle(page_node_->loading_state())) {
    return false;
  }
  CHECK(!loaded_idle_time_.is_null());
  return heuristics.IsOutsideLoadingGracePeriod(
             page_node_, feature_type,
             base::TimeTicks::Now() - loaded_idle_time_) &&
         heuristics.IsInBackground(page_node_) &&
         heuristics.IsOutsideBackgroundingGracePeriod(
             page_node_, feature_type,
             page_node_->TimeSinceLastVisibilityChange());
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

SiteDataNodeData* GetSiteDataNodeDataFromPageNode(const PageNode* page_node) {
  auto* page_node_impl = PageNodeImpl::FromNode(page_node);
  DCHECK(page_node_impl);
  auto* data = SiteDataNodeData::Get(page_node_impl);
  DCHECK(data);
  return data;
}

}  // namespace

SiteDataRecorder::SiteDataRecorder()
    : heuristics_impl_(std::make_unique<DefaultHeuristics>()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SiteDataRecorder::~SiteDataRecorder() = default;

void SiteDataRecorder::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!g_site_data_recorder);
  g_site_data_recorder = this;
  RegisterObservers(graph);
}

void SiteDataRecorder::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UnregisterObservers(graph);
  CHECK_EQ(g_site_data_recorder, this);
  g_site_data_recorder = nullptr;
}

void SiteDataRecorder::OnPageNodeAdded(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetPageNodeDataCache(page_node);
}

void SiteDataRecorder::OnBeforePageNodeRemoved(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* data = GetSiteDataNodeDataFromPageNode(page_node);
  data->Reset();
}

void SiteDataRecorder::OnMainFrameUrlChanged(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* data = GetSiteDataNodeDataFromPageNode(page_node);
  data->OnMainFrameUrlChanged(page_node->GetMainFrameUrl(),
                              page_node->IsVisible());
}

void SiteDataRecorder::OnLoadingStateChanged(
    const PageNode* page_node,
    PageNode::LoadingState previous_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* data = GetSiteDataNodeDataFromPageNode(page_node);
  const bool is_loaded_idle =
      heuristics_impl_->IsLoadedIdle(page_node->GetLoadingState());
  const bool was_loaded_idle = heuristics_impl_->IsLoadedIdle(previous_state);
  if (is_loaded_idle != was_loaded_idle) {
    data->OnIsLoadedIdleChanged(is_loaded_idle);
  }
}

void SiteDataRecorder::OnIsVisibleChanged(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* data = GetSiteDataNodeDataFromPageNode(page_node);
  data->OnIsVisibleChanged(page_node->IsVisible());
}

void SiteDataRecorder::OnIsAudibleChanged(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* data = GetSiteDataNodeDataFromPageNode(page_node);
  data->OnIsAudibleChanged(page_node->IsAudible());
}

void SiteDataRecorder::OnTitleUpdated(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* data = GetSiteDataNodeDataFromPageNode(page_node);
  data->OnTitleUpdated();
}

void SiteDataRecorder::OnFaviconUpdated(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* data = GetSiteDataNodeDataFromPageNode(page_node);
  data->OnFaviconUpdated();
}

// static
void SiteDataRecorder::SetHeuristicsImplementationForTesting(
    std::unique_ptr<SiteDataRecorderHeuristics> heuristics) {
  CHECK(g_site_data_recorder);
  g_site_data_recorder->heuristics_impl_ =
      heuristics ? std::move(heuristics)
                 : std::make_unique<DefaultHeuristics>();
}

void SiteDataRecorder::RegisterObservers(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->AddPageNodeObserver(this);
}

void SiteDataRecorder::UnregisterObservers(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->RemovePageNodeObserver(this);
}

void SiteDataRecorder::SetPageNodeDataCache(const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* page_node_impl = PageNodeImpl::FromNode(page_node);
  DCHECK(page_node_impl);
  DCHECK(!SiteDataNodeData::Get(page_node_impl));
  auto* data = SiteDataNodeData::GetOrCreate(page_node_impl);
  data->set_data_cache(
      SiteDataCacheFactory::GetInstance()->GetDataCacheForBrowserContext(
          page_node->GetBrowserContextID()));
}

// static
bool SiteDataRecorderHeuristics::DefaultIsLoadedIdle(
    PageNode::LoadingState loading_state) {
  switch (loading_state) {
    case PageNode::LoadingState::kLoadingNotStarted:
    case PageNode::LoadingState::kLoadedBusy:
    case PageNode::LoadingState::kLoading:
    case PageNode::LoadingState::kLoadingTimedOut:
      return false;
    case PageNode::LoadingState::kLoadedIdle:
      return true;
  }
  NOTREACHED_NORETURN();
}

// static
bool SiteDataRecorderHeuristics::DefaultIsInBackground(
    const PageNode* page_node) {
  return GetPageNodeVisibility(page_node) != TabVisibility::kForeground;
}

// static
bool SiteDataRecorderHeuristics::DefaultIsOutsideLoadingGracePeriod(
    const PageNode* page_node,
    FeatureType feature_type,
    base::TimeDelta time_since_load) {
  if (feature_type == FeatureType::kTitleChange ||
      feature_type == FeatureType::kFaviconChange) {
    return time_since_load >= kTitleOrFaviconChangePostLoadGracePeriod;
  }
  return true;
}

// static
bool SiteDataRecorderHeuristics::DefaultIsOutsideBackgroundingGracePeriod(
    const PageNode* page_node,
    FeatureType feature_type,
    base::TimeDelta time_since_backgrounding) {
  // Ignore events happening shortly after the tab being backgrounded, they're
  // usually false positives.
  return time_since_backgrounding >= kFeatureUsagePostBackgroundGracePeriod;
}

// static
const SiteDataRecorder::Data* SiteDataRecorder::Data::FromPageNode(
    const PageNode* page_node) {
  return SiteDataNodeData::Get(PageNodeImpl::FromNode(page_node));
}

// static
SiteDataRecorder::Data* SiteDataRecorder::Data::GetForTesting(
    const PageNode* page_node) {
  return GetSiteDataNodeDataFromPageNode(page_node);
}

// static
SiteDataReader* SiteDataRecorder::Data::GetReaderForPageNode(
    const PageNode* page_node) {
  const auto* site_data = FromPageNode(page_node);
  return site_data ? site_data->reader() : nullptr;
}

}  // namespace performance_manager
