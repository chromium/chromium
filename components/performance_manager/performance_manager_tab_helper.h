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
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom-forward.h"

namespace performance_manager {

class FrameNodeImpl;
class PageNodeImpl;

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
  // Observer interface to be notified when a PerformanceManagerTabHelper is
  // being teared down.
  class DestructionObserver {
   public:
    virtual ~DestructionObserver() = default;
    virtual void OnPerformanceManagerTabHelperDestroying(
        content::WebContents*) = 0;
  };

  ~PerformanceManagerTabHelper() override;

  PageNodeImpl* page_node() { return page_node_.get(); }

  // Registers an observer that is notified when the PerformanceManagerTabHelper
  // is destroyed. Can only be set to non-nullptr if it was previously nullptr,
  // and vice-versa.
  void SetDestructionObserver(DestructionObserver* destruction_observer);

  // Must be invoked prior to detaching a PerformanceManagerTabHelper from its
  // WebContents.
  void TearDown();

  // WebContentsObserver overrides.
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void OnAudioStateChanged(bool audible) override;
  void OnFrameAudioStateChanged(content::RenderFrameHost* render_frame_host,
                                bool is_audible) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void TitleWasSet(content::NavigationEntry* entry) override;
  void InnerWebContentsAttached(content::WebContents* inner_web_contents,
                                content::RenderFrameHost* render_frame_host,
                                bool is_full_page) override;
  void InnerWebContentsDetached(
      content::WebContents* inner_web_contents) override;
  void WebContentsDestroyed() override;
  void DidUpdateFaviconURL(
      content::RenderFrameHost* render_frame_host,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) override;

  // WebContentsProxyImpl overrides.
  content::WebContents* GetWebContents() const override;
  int64_t LastNavigationId() const override;
  int64_t LastNewDocNavigationId() const override;

  void BindDocumentCoordinationUnit(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::DocumentCoordinationUnit> receiver);

  void SetUkmSourceIdForTesting(ukm::SourceId id) { ukm_source_id_ = id; }

  // Retrieves the frame node associated with |render_frame_host|. Returns
  // nullptr if none exist for that frame.
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
  friend class PerformanceManagerRegistryImpl;
  friend class WebContentsProxyImpl;

  explicit PerformanceManagerTabHelper(content::WebContents* web_contents);

  // Make CreateForWebContents private to restrict usage to
  // PerformanceManagerRegistry.
  using WebContentsUserData<PerformanceManagerTabHelper>::CreateForWebContents;

  void OnMainFrameNavigation(int64_t navigation_id, bool same_doc);

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
  // Similar to the above, but for the last non same-document navigation
  // associated with this WebContents. This is always for a navigation that is
  // older or equal to |last_navigation_id_|.
  int64_t last_new_doc_navigation_id_ = 0;

  // Maps from RenderFrameHost to the associated PM node.
  std::map<content::RenderFrameHost*, std::unique_ptr<FrameNodeImpl>> frames_;

  DestructionObserver* destruction_observer_ = nullptr;
  base::ObserverList<Observer, true, false> observers_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<PerformanceManagerTabHelper> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PerformanceManagerTabHelper);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_TAB_HELPER_H_
