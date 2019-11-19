// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_TAB_HELPER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_TAB_HELPER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom-forward.h"
#include "components/performance_manager/web_contents_proxy_impl.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace performance_manager {

class FrameNodeImpl;
class PageNodeImpl;
class PerformanceManagerImpl;

// This tab helper maintains a page node, and its associated tree of frame nodes
// in the performance manager graph. It also sources a smattering of attributes
// into the graph, including visibility, title, and favicon bits.
// In addition it handles forwarding interface requests from the render frame
// host to the frame graph entity.
class PerformanceManagerTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PerformanceManagerTabHelper>,
      public WebContentsProxyImpl {
 public:
  // Detaches all instances from their WebContents and destroys them.
  static void DetachAndDestroyAll();

  ~PerformanceManagerTabHelper() override;

  PageNodeImpl* page_node() { return page_node_.get(); }

  // WebContentsObserver overrides.
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void DidStartLoading() override;
  void DidStopLoading() override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void OnAudioStateChanged(bool audible) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void TitleWasSet(content::NavigationEntry* entry) override;
  void WebContentsDestroyed() override;
  void DidUpdateFaviconURL(
      const std::vector<content::FaviconURL>& candidates) override;

  // WebContentsProxyImpl overrides.
  content::WebContents* GetWebContents() const override;
  int64_t LastNavigationId() const override;

  void BindDocumentCoordinationUnit(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::DocumentCoordinationUnit> receiver);

  void SetUkmSourceIdForTesting(ukm::SourceId id) { ukm_source_id_ = id; }

  // Retrieves the frame node associated with |render_frame_host|.
  FrameNodeImpl* GetFrameNode(content::RenderFrameHost* render_frame_host);

  class Observer : public base::CheckedObserver {
   public:
    // Invoked when a frame node is about to be removed from the graph.
    virtual void OnBeforeFrameNodeRemoved(
        PerformanceManagerTabHelper* performance_manager,
        FrameNodeImpl* frame_node) = 0;
  };

  // Adds/removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class content::WebContentsUserData<PerformanceManagerTabHelper>;
  friend class WebContentsProxyImpl;

  explicit PerformanceManagerTabHelper(content::WebContents* web_contents);
  void TearDown();

  // Post a task to run in the performance manager sequence. The |node| will be
  // passed as unretained, and the closure will be created with BindOnce.
  template <typename Functor, typename NodeType, typename... Args>
  void PostToGraph(const base::Location& from_here,
                   Functor&& functor,
                   NodeType* node,
                   Args&&... args);

  void OnMainFrameNavigation(int64_t navigation_id);

  // The performance manager for this process, if any.
  PerformanceManagerImpl* const performance_manager_;
  std::unique_ptr<PageNodeImpl> page_node_;
  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;

  // Favicon and title are set when a page is loaded, we only want to send
  // signals to the page node about title and favicon update from the previous
  // title and favicon, thus we want to ignore the very first update since it is
  // always supposed to happen.
  bool first_time_favicon_set_ = false;
  bool first_time_title_set_ = false;

  // The last navigation ID that was committed to a main frame in this web
  // contents.
  int64_t last_navigation_id_ = 0;

  // Maps from RenderFrameHost to the associated PM node.
  std::map<content::RenderFrameHost*, std::unique_ptr<FrameNodeImpl>> frames_;

  base::ObserverList<Observer, true, false> observers_;

  // All instances are linked together in a doubly linked list to allow orderly
  // destruction at browser shutdown time.
  static PerformanceManagerTabHelper* first_;

  PerformanceManagerTabHelper* next_ = nullptr;
  PerformanceManagerTabHelper* prev_ = nullptr;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<PerformanceManagerTabHelper> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PerformanceManagerTabHelper);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_TAB_HELPER_H_
