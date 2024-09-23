// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_SITE_DATA_RECORDER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_SITE_DATA_RECORDER_H_

#include <memory>

#include "base/sequence_checker.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace performance_manager {

class SiteDataReader;
class SiteDataWriter;
class SiteDataCache;

// Policy class implementing heuristics that are checked by SiteDataRecorder.
// This can be overridden in tests to change the definition of the heuristics,
// for example to ignore timeouts. The default production versions of the
// heuristics are available in static methods.
class SiteDataRecorderHeuristics {
 public:
  // The features tracked by the SiteDataRecorder class.
  enum class FeatureType {
    kTitleChange,
    kFaviconChange,
    kAudioUsage,
  };

  SiteDataRecorderHeuristics() = default;
  virtual ~SiteDataRecorderHeuristics() = default;

  SiteDataRecorderHeuristics(const SiteDataRecorderHeuristics& other) = delete;
  SiteDataRecorderHeuristics& operator=(const SiteDataRecorderHeuristics&) =
      delete;

  // If any of these return false, the feature usage will not be recorded.

  // Returns whether the page has reached LoadingState::kLoadedIdle.
  virtual bool IsLoadedIdle(PageNode::LoadingState loading_state) const = 0;

  // Returns whether the page is in the background.
  virtual bool IsInBackground(const PageNode* page_node) const = 0;

  // Returns whether a grace period (on the order of tens of seconds) has passed
  // since IsLoadedIdle() returned true. Feature changes during that time are
  // ignored since they may be part of delayed work from the initial page load.
  // This will not be called while IsLoadedIdle() returns false.
  virtual bool IsOutsideLoadingGracePeriod(
      const PageNode* page_node,
      FeatureType feature_type,
      base::TimeDelta time_since_load) const = 0;

  // Returns whether a grace period (on the order of tens of seconds) has passed
  // since the page was put in the background. Feature changes during that time
  // are ignored since they are often false positives. This will not be called
  // while IsInBackground() returns false.
  virtual bool IsOutsideBackgroundingGracePeriod(
      const PageNode* page_node,
      FeatureType feature_type,
      base::TimeDelta time_since_backgrounding) const = 0;

 protected:
  // Default implementations of the heuristics. Used in production. These are
  // protected so they can be called from subclasses that override some
  // heuristics and use the defaults for others.

  static bool DefaultIsLoadedIdle(PageNode::LoadingState loading_state);
  static bool DefaultIsInBackground(const PageNode* page_node);
  static bool DefaultIsOutsideLoadingGracePeriod(
      const PageNode* page_node,
      FeatureType feature_type,
      base::TimeDelta time_since_load);
  static bool DefaultIsOutsideBackgroundingGracePeriod(
      const PageNode* page_node,
      FeatureType feature_type,
      base::TimeDelta time_since_backgrounding);
};

// The SiteDataRecorder decorator is responsible for adorning PageNodes with a
// SiteDataReader and a SiteDataWriter and for forwarding the event of interest
// to this writer.
class SiteDataRecorder : public GraphOwned,
                         public PageNode::ObserverDefaultImpl {
 public:
  class Data;

  SiteDataRecorder();
  ~SiteDataRecorder() override;

  SiteDataRecorder(const SiteDataRecorder& other) = delete;
  SiteDataRecorder& operator=(const SiteDataRecorder&) = delete;

  // Returns the current heuristic implementation. This will use the default
  // production heuristics unless overridden using
  // SetHeuristicsImplementationForTesting().
  const SiteDataRecorderHeuristics& heuristics_impl() const {
    return *heuristics_impl_;
  }

  // Overrides the default heuristics implementation for testing. If
  // `heuristics` is null, the default implementation is restored.
  static void SetHeuristicsImplementationForTesting(
      std::unique_ptr<SiteDataRecorderHeuristics> heuristics);

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // PageNode::ObserverDefaultImpl:
  void OnPageNodeAdded(const PageNode* page_node) override;
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;
  void OnMainFrameUrlChanged(const PageNode* page_node) override;
  void OnLoadingStateChanged(const PageNode* page_node,
                             PageNode::LoadingState previous_state) override;
  void OnIsVisibleChanged(const PageNode* page_node) override;
  void OnIsAudibleChanged(const PageNode* page_node) override;
  void OnTitleUpdated(const PageNode* page_node) override;
  void OnFaviconUpdated(const PageNode* page_node) override;

 private:
  // (Un)registers the various node observer flavors of this object with the
  // graph.
  void RegisterObservers(Graph* graph);
  void UnregisterObservers(Graph* graph);

  // Set the site data cache that should be used by |page_node| to create its
  // site data writer.
  void SetPageNodeDataCache(const PageNode* page_node);

  std::unique_ptr<SiteDataRecorderHeuristics> heuristics_impl_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// Allows retrieving the SiteDataWriter and SiteDataReader associated with a
// PageNode.
class SiteDataRecorder::Data {
 public:
  Data() = default;
  virtual ~Data() = default;

  Data(const Data& other) = delete;
  Data& operator=(const Data&) = delete;

  virtual SiteDataWriter* writer() const = 0;
  virtual SiteDataReader* reader() const = 0;

  virtual void SetDataCacheForTesting(SiteDataCache* cache) = 0;

  static const Data& FromPageNode(const PageNode* page_node);
  static Data& GetForTesting(const PageNode* page_node);

  // Convenience accessor.
  static SiteDataReader* GetReaderForPageNode(const PageNode* page_node);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_SITE_DATA_RECORDER_H_
