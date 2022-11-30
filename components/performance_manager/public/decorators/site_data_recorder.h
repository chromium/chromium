// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_SITE_DATA_RECORDER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_SITE_DATA_RECORDER_H_

#include "base/sequence_checker.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace performance_manager {

class SiteDataReader;
class SiteDataWriter;
class SiteDataCache;

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

  SEQUENCE_CHECKER(sequence_checker_);
};

// Allows retrieving the SiteDataWriter and SiteDataReader associated with a
// PageNode.
class SiteDataRecorder::Data {
 public:
  Data();
  virtual ~Data();
  Data(const Data& other) = delete;
  Data& operator=(const Data&) = delete;

  virtual SiteDataWriter* writer() const = 0;
  virtual SiteDataReader* reader() const = 0;

  static const Data* FromPageNode(const PageNode* page_node);
  virtual void SetDataCacheForTesting(SiteDataCache* cache) = 0;
  static Data* GetForTesting(const PageNode* page_node);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_DECORATORS_SITE_DATA_RECORDER_H_
