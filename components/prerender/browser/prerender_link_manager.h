// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRERENDER_BROWSER_PRERENDER_LINK_MANAGER_H_
#define COMPONENTS_PRERENDER_BROWSER_PRERENDER_LINK_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prerender/browser/prerender_handle.h"
#include "third_party/blink/public/mojom/prerender/prerender.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

FORWARD_DECLARE_TEST(WebViewTest, NoPrerenderer);

namespace prerender {

class PrerenderManager;

// PrerenderLinkManager implements the API on Link elements for all documents
// being rendered in this chrome instance.  It receives messages from the
// renderer indicating addition, cancelation and abandonment of link elements,
// and controls the PrerenderManager accordingly.
class PrerenderLinkManager : public KeyedService,
                             public PrerenderHandle::Observer {
 public:
  explicit PrerenderLinkManager(PrerenderManager* manager);
  ~PrerenderLinkManager() override;

  // Called when a <link rel=prerender ...> element has been inserted into the
  // document. Returns the prerender id that is used for canceling or abandoning
  // prerendering. Returns base::nullopt if the prerender was not started.
  base::Optional<int> OnStartPrerender(
      int launcher_render_process_id,
      int launcher_render_view_id,
      blink::mojom::PrerenderAttributesPtr attributes,
      const url::Origin& initiator_origin,
      mojo::PendingRemote<blink::mojom::PrerenderProcessorClient>
          processor_client);

  // Called when a <link rel=prerender ...> element has been explicitly removed
  // from a document.
  void OnCancelPrerender(int prerender_id);

  // Called when a renderer launching <link rel=prerender ...> has navigated
  // away from the launching page, the launching renderer process has crashed,
  // or perhaps the renderer process was fast-closed when the last render view
  // in it was closed.
  void OnAbandonPrerender(int prerender_id);

 private:
  friend class PrerenderBrowserTest;
  friend class PrerenderTest;
  // WebViewTest.NoPrerenderer needs to access the private IsEmpty() method.
  FRIEND_TEST_ALL_PREFIXES(::WebViewTest, NoPrerenderer);

  class PendingPrerenderManager;

  // Used to store state about a requested prerender.
  struct LinkPrerender {
    LinkPrerender(int launcher_render_process_id,
                  int launcher_render_view_id,
                  blink::mojom::PrerenderAttributesPtr attributes,
                  const url::Origin& initiator_origin,
                  mojo::PendingRemote<blink::mojom::PrerenderProcessorClient>
                      processor_client,
                  base::TimeTicks creation_time,
                  PrerenderContents* deferred_launcher);
    ~LinkPrerender();

    LinkPrerender(const LinkPrerender& other) = delete;
    LinkPrerender& operator=(const LinkPrerender& other) = delete;

    // Parameters from PrerenderLinkManager::OnStartPrerender():
    const int launcher_render_process_id;
    const int launcher_render_view_id;
    const GURL url;
    const blink::mojom::PrerenderRelType rel_type;
    const content::Referrer referrer;
    const url::Origin initiator_origin;
    const gfx::Size size;

    // Notification interface back to the requestor of this prerender.
    mojo::Remote<blink::mojom::PrerenderProcessorClient>
        remote_processor_client;

    // The time at which this Prerender was added to PrerenderLinkManager.
    const base::TimeTicks creation_time;

    // If non-null, this link prerender was launched by an unswapped prerender,
    // |deferred_launcher|. When |deferred_launcher| is swapped in, the field is
    // set to null.
    const PrerenderContents* deferred_launcher;

    // Initially null, |handle| is set once we start this prerender. It is owned
    // by this struct, and must be deleted before destructing this struct.
    std::unique_ptr<PrerenderHandle> handle;

    // True if this prerender has been abandoned by its launcher.
    bool has_been_abandoned;

    // The unique ID of this prerender. Used for canceling or abandoning
    // prerendering.
    const int prerender_id;
  };

  bool IsEmpty() const;

  bool PrerenderIsRunningForTesting(LinkPrerender* link_prerender) const;

  // Returns a count of currently running prerenders.
  size_t CountRunningPrerenders() const;

  // Start any prerenders that can be started, respecting concurrency limits for
  // the system and per launcher.
  void StartPrerenders();

  LinkPrerender* FindByPrerenderHandle(PrerenderHandle* prerender_handle);
  LinkPrerender* FindByPrerenderId(int prerender_id);

  // Removes |prerender| from the the prerender link manager. Deletes the
  // PrerenderHandle as needed.
  void RemovePrerender(LinkPrerender* prerender);

  // Cancels |prerender| and removes |prerender| from the prerender link
  // manager.
  void CancelPrerender(LinkPrerender* prerender);

  // From KeyedService:
  void Shutdown() override;

  // From PrerenderHandle::Observer:
  void OnPrerenderStart(PrerenderHandle* prerender_handle) override;
  void OnPrerenderStopLoading(PrerenderHandle* prerender_handle) override;
  void OnPrerenderDomContentLoaded(PrerenderHandle* prerender_handle) override;
  void OnPrerenderStop(PrerenderHandle* prerender_handle) override;
  void OnPrerenderNetworkBytesChanged(
      PrerenderHandle* prerender_handle) override;

  bool has_shutdown_;

  PrerenderManager* const manager_;

  // All prerenders known to this PrerenderLinkManager. Insertions are always
  // made at the back, so the oldest prerender is at the front, and the youngest
  // at the back. Using std::unique_ptr<> here as LinkPrerender is not copyable.
  std::list<std::unique_ptr<LinkPrerender>> prerenders_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderLinkManager);
};

}  // namespace prerender

#endif  // COMPONENTS_PRERENDER_BROWSER_PRERENDER_LINK_MANAGER_H_
