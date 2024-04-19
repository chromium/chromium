// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_LINK_MANAGER_H_
#define COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_LINK_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <optional>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_handle.h"
#include "third_party/blink/public/mojom/prerender/prerender.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

FORWARD_DECLARE_TEST(WebViewTest, NoPrerenderer);

namespace prerender {

class NoStatePrefetchManager;

// NoStatePrefetchLinkManager implements the API on Link elements for all
// documents being rendered in this chrome instance.  It receives messages from
// the renderer indicating addition, cancelation and abandonment of link
// elements, and controls the NoStatePrefetchManager accordingly.
class NoStatePrefetchLinkManager : public KeyedService,
                                   public NoStatePrefetchHandle::Observer {
 public:
  explicit NoStatePrefetchLinkManager(NoStatePrefetchManager* manager);

  NoStatePrefetchLinkManager(const NoStatePrefetchLinkManager&) = delete;
  NoStatePrefetchLinkManager& operator=(const NoStatePrefetchLinkManager&) =
      delete;

  ~NoStatePrefetchLinkManager() override;

  // Called when a <link rel=prerender ...> element has been inserted into the
  // document. Returns the link trigger id that is used for canceling or
  // abandoning prefetch. Returns std::nullopt if the prefetch was not started.
  virtual std::optional<int> OnStartLinkTrigger(
      int launcher_render_process_id,
      int launcher_render_view_id,
      int launcher_render_frame_id,
      blink::mojom::PrerenderAttributesPtr attributes,
      const url::Origin& initiator_origin);

  // Called when a <link rel=prerender ...> element has been explicitly removed
  // from a document.
  virtual void OnCancelLinkTrigger(int link_trigger_id);

  // Called when a renderer launching <link rel=prerender ...> has navigated
  // away from the launching page, the launching renderer process has crashed,
  // or perhaps the renderer process was fast-closed when the last render view
  // in it was closed.
  virtual void OnAbandonLinkTrigger(int link_trigger_id);

 private:
  friend class PrerenderBrowserTest;
  friend class NoStatePrefetchTest;
  // WebViewTest.NoPrerenderer needs to access the private IsEmpty() method.
  FRIEND_TEST_ALL_PREFIXES(::WebViewTest, NoPrerenderer);

  // Used to store state about a <link rel=prerender ...> that triggers
  // NoStatePrefetch.
  struct LinkTrigger {
    LinkTrigger(int launcher_render_process_id,
                int launcher_render_view_id,
                blink::mojom::PrerenderAttributesPtr attributes,
                const url::Origin& initiator_origin,
                base::TimeTicks creation_time,
                NoStatePrefetchContents* deferred_launcher);
    ~LinkTrigger();

    LinkTrigger(const LinkTrigger& other) = delete;
    LinkTrigger& operator=(const LinkTrigger& other) = delete;

    // Parameters from NoStatePrefetchLinkManager::OnStartLinkTrigger():
    const int launcher_render_process_id;
    const int launcher_render_view_id;
    const GURL url;
    const blink::mojom::PrerenderTriggerType trigger_type;
    const content::Referrer referrer;
    const url::Origin initiator_origin;
    const gfx::Size size;

    // The time at which this trigger was added to NoStatePrefetchLinkManager.
    const base::TimeTicks creation_time;

    // If non-null, this trigger was launched by an unswapped prefetcher,
    // |deferred_launcher|. When |deferred_launcher| is swapped in, the field is
    // set to null.
    raw_ptr<const NoStatePrefetchContents> deferred_launcher;

    // Initially null, |handle| is set once we start this trigger. It is owned
    // by this struct, and must be deleted before destructing this struct.
    std::unique_ptr<NoStatePrefetchHandle> handle;

    // True if this trigger has been abandoned by its launcher.
    bool has_been_abandoned;

    // The unique ID of this trigger. Used for canceling or abandoning
    // prefetching.
    const int link_trigger_id;
  };

  bool IsEmpty() const;

  bool TriggerIsRunningForTesting(LinkTrigger* link_trigger) const;

  // Returns a count of currently running triggers.
  size_t CountRunningTriggers() const;

  // Start any triggers that can be started, respecting concurrency limits for
  // the system and per launcher.
  void StartLinkTriggers();

  LinkTrigger* FindByNoStatePrefetchHandle(
      NoStatePrefetchHandle* no_state_prefetch_handle);
  LinkTrigger* FindByLinkTriggerId(int link_trigger_id);

  // Removes |trigger| from the the prerender link manager. Deletes the
  // NoStatePrefetchHandle as needed.
  void RemoveLinkTrigger(LinkTrigger* trigger);

  // Cancels |trigger| and removes |trigger| from the prerender link
  // manager.
  void CancelLinkTrigger(LinkTrigger* trigger);

  // From KeyedService:
  void Shutdown() override;

  // From NoStatePrefetchHandle::Observer:
  void OnPrefetchStop(NoStatePrefetchHandle* no_state_prefetch_handle) override;

  bool has_shutdown_;

  const raw_ptr<NoStatePrefetchManager> manager_;

  // All triggers known to this NoStatePrefetchLinkManager. Insertions are
  // always made at the back, so the oldest trigger is at the front, and the
  // youngest at the back. Using std::unique_ptr<> here as LinkTrigger is not
  // copyable.
  std::list<std::unique_ptr<LinkTrigger>> triggers_;
};

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_LINK_MANAGER_H_
