// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prerender/browser/prerender_link_manager.h"

#include <functional>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "components/prerender/browser/prerender_contents.h"
#include "components/prerender/browser/prerender_handle.h"
#include "components/prerender/browser/prerender_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/common/referrer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

// TODO(crbug.com/722453): Use a dedicated build flag for GuestView.
#if !defined(OS_ANDROID) && !defined(OS_IOS) && !defined(OS_FUCHSIA)
#include "components/guest_view/browser/guest_view_base.h"  // nogncheck
#endif

using base::TimeDelta;
using base::TimeTicks;
using content::RenderViewHost;
using content::SessionStorageNamespace;

namespace prerender {

namespace {

int GetNextPrerenderId() {
  static int next_id = 1;
  return next_id++;
}

}  // namespace

PrerenderLinkManager::LinkPrerender::LinkPrerender(
    int launcher_render_process_id,
    int launcher_render_view_id,
    blink::mojom::PrerenderAttributesPtr attributes,
    const url::Origin& initiator_origin,
    mojo::PendingRemote<blink::mojom::PrerenderProcessorClient>
        processor_client,
    base::TimeTicks creation_time,
    PrerenderContents* deferred_launcher)
    : launcher_render_process_id(launcher_render_process_id),
      launcher_render_view_id(launcher_render_view_id),
      url(attributes->url),
      rel_type(attributes->rel_type),
      referrer(content::Referrer(*attributes->referrer)),
      initiator_origin(initiator_origin),
      size(attributes->view_size),
      remote_processor_client(std::move(processor_client)),
      creation_time(creation_time),
      deferred_launcher(deferred_launcher),
      has_been_abandoned(false),
      prerender_id(GetNextPrerenderId()) {}

PrerenderLinkManager::LinkPrerender::~LinkPrerender() {
  DCHECK_EQ(nullptr, handle.get())
      << "The PrerenderHandle should be destroyed before its Prerender.";
}

PrerenderLinkManager::PrerenderLinkManager(PrerenderManager* manager)
    : has_shutdown_(false), manager_(manager) {}

PrerenderLinkManager::~PrerenderLinkManager() {
  for (auto& prerender : prerenders_) {
    if (prerender->handle) {
      DCHECK(!prerender->handle->IsPrerendering())
          << "All running prerenders should stop at the same time as the "
          << "PrerenderManager.";
      prerender->handle.reset();
    }
  }
}

base::Optional<int> PrerenderLinkManager::OnStartPrerender(
    int launcher_render_process_id,
    int launcher_render_view_id,
    blink::mojom::PrerenderAttributesPtr attributes,
    const url::Origin& initiator_origin,
    mojo::PendingRemote<blink::mojom::PrerenderProcessorClient>
        processor_client) {
// TODO(crbug.com/722453): Use a dedicated build flag for GuestView.
#if !defined(OS_ANDROID) && !defined(OS_IOS) && !defined(OS_FUCHSIA)
  content::RenderViewHost* rvh = content::RenderViewHost::FromID(
      launcher_render_process_id, launcher_render_view_id);
  content::WebContents* web_contents =
      rvh ? content::WebContents::FromRenderViewHost(rvh) : nullptr;
  // Guests inside <webview> do not support cross-process navigation and so we
  // do not allow guests to prerender content.
  if (guest_view::GuestViewBase::IsGuest(web_contents))
    return base::nullopt;
#endif

  // Check if the launcher is itself an unswapped prerender.
  PrerenderContents* prerender_contents =
      manager_->GetPrerenderContentsForRoute(launcher_render_process_id,
                                             launcher_render_view_id);
  if (prerender_contents &&
      prerender_contents->final_status() != FINAL_STATUS_UNKNOWN) {
    // The launcher is a prerender about to be destroyed asynchronously, but
    // its AddLinkRelPrerender message raced with shutdown. Ignore it.
    DCHECK_NE(FINAL_STATUS_USED, prerender_contents->final_status());
    return base::nullopt;
  }

  auto prerender = std::make_unique<LinkPrerender>(
      launcher_render_process_id, launcher_render_view_id,
      std::move(attributes), initiator_origin, std::move(processor_client),
      manager_->GetCurrentTimeTicks(), prerender_contents);

  // Observe disconnect of the client and treat as equivalent to explicit
  // abandonment. Similar to above, the raw pointer to |this| is safe because
  // |prerender| is owned by |this|.
  prerender->remote_processor_client.set_disconnect_handler(
      base::BindOnce(&PrerenderLinkManager::OnAbandonPrerender,
                     base::Unretained(this), prerender->prerender_id));

  // Stash pointer used only for comparison later.
  const LinkPrerender* prerender_ptr = prerender.get();

  prerenders_.push_back(std::move(prerender));

  if (!prerender_contents)
    StartPrerenders();

  // Check if the prerender we added is still at the end of the list. It
  // may have been discarded by StartPrerenders().
  if (!prerenders_.empty() && prerenders_.back().get() == prerender_ptr)
    return prerender_ptr->prerender_id;
  return base::nullopt;
}

void PrerenderLinkManager::OnCancelPrerender(int prerender_id) {
  LinkPrerender* prerender = FindByPrerenderId(prerender_id);
  if (!prerender)
    return;
  CancelPrerender(prerender);
  StartPrerenders();
}

void PrerenderLinkManager::OnAbandonPrerender(int prerender_id) {
  LinkPrerender* prerender = FindByPrerenderId(prerender_id);
  if (!prerender)
    return;

  if (!prerender->handle) {
    RemovePrerender(prerender);
    return;
  }

  prerender->has_been_abandoned = true;
  prerender->handle->OnNavigateAway();
  DCHECK(prerender->handle);

  // If the prerender is not running, remove it from the list so it does not
  // leak. If it is running, it will send a cancel event when it stops which
  // will remove it.
  if (!prerender->handle->IsPrerendering())
    RemovePrerender(prerender);
}

bool PrerenderLinkManager::IsEmpty() const {
  return prerenders_.empty();
}

bool PrerenderLinkManager::PrerenderIsRunningForTesting(
    LinkPrerender* prerender) const {
  return prerender->handle.get() != nullptr;
}

size_t PrerenderLinkManager::CountRunningPrerenders() const {
  return std::count_if(prerenders_.begin(), prerenders_.end(),
                       [](const std::unique_ptr<LinkPrerender>& prerender) {
                         return prerender->handle &&
                                prerender->handle->IsPrerendering();
                       });
}

void PrerenderLinkManager::StartPrerenders() {
  if (has_shutdown_)
    return;

  size_t total_started_prerender_count = 0;
  std::list<LinkPrerender*> abandoned_prerenders;
  std::list<std::list<std::unique_ptr<LinkPrerender>>::iterator>
      pending_prerenders;
  std::multiset<std::pair<int, int>> running_launcher_and_render_view_routes;

  // Scan the list, counting how many prerenders have handles (and so were added
  // to the PrerenderManager). The count is done for the system as a whole, and
  // also per launcher.
  for (auto it = prerenders_.begin(); it != prerenders_.end(); ++it) {
    std::unique_ptr<LinkPrerender>& prerender = *it;
    // Skip prerenders launched by a prerender.
    if (prerender->deferred_launcher)
      continue;
    if (!prerender->handle) {
      pending_prerenders.push_back(it);
    } else {
      ++total_started_prerender_count;
      if (prerender->has_been_abandoned) {
        abandoned_prerenders.push_back(prerender.get());
      } else {
        // We do not count abandoned prerenders towards their launcher, since it
        // has already navigated on to another page.
        std::pair<int, int> launcher_and_render_view_route(
            prerender->launcher_render_process_id,
            prerender->launcher_render_view_id);
        running_launcher_and_render_view_routes.insert(
            launcher_and_render_view_route);
        DCHECK_GE(manager_->config().max_link_concurrency_per_launcher,
                  running_launcher_and_render_view_routes.count(
                      launcher_and_render_view_route));
      }
    }
  }
  DCHECK_LE(abandoned_prerenders.size(), total_started_prerender_count);
  DCHECK_GE(manager_->config().max_link_concurrency,
            total_started_prerender_count);
  DCHECK_LE(CountRunningPrerenders(), total_started_prerender_count);

  TimeTicks now = manager_->GetCurrentTimeTicks();

  // Scan the pending prerenders, starting prerenders as we can.
  for (const std::list<std::unique_ptr<LinkPrerender>>::iterator& it :
       pending_prerenders) {
    LinkPrerender* pending_prerender = it->get();

    TimeDelta prerender_age = now - pending_prerender->creation_time;
    if (prerender_age >= manager_->config().max_wait_to_launch) {
      // This prerender waited too long in the queue before launching.
      prerenders_.erase(it);
      continue;
    }

    std::pair<int, int> launcher_and_render_view_route(
        pending_prerender->launcher_render_process_id,
        pending_prerender->launcher_render_view_id);
    if (manager_->config().max_link_concurrency_per_launcher <=
        running_launcher_and_render_view_routes.count(
            launcher_and_render_view_route)) {
      // This prerender's launcher is already at its limit.
      continue;
    }

    if (total_started_prerender_count >=
            manager_->config().max_link_concurrency ||
        total_started_prerender_count >= prerenders_.size()) {
      // The system is already at its prerender concurrency limit. Try removing
      // an abandoned prerender, if one exists, to make room.
      if (abandoned_prerenders.empty())
        return;

      CancelPrerender(abandoned_prerenders.front());
      --total_started_prerender_count;
      abandoned_prerenders.pop_front();
    }

    std::unique_ptr<PrerenderHandle> handle =
        manager_->AddPrerenderFromLinkRelPrerender(
            pending_prerender->launcher_render_process_id,
            pending_prerender->launcher_render_view_id, pending_prerender->url,
            pending_prerender->rel_type, pending_prerender->referrer,
            pending_prerender->initiator_origin, pending_prerender->size);
    if (!handle) {
      // This prerender couldn't be launched, it's gone.
      prerenders_.erase(it);
      continue;
    }

    if (handle->IsPrerendering()) {
      // We have successfully started a new prerender.
      pending_prerender->handle = std::move(handle);
      ++total_started_prerender_count;
      pending_prerender->handle->SetObserver(this);
      OnPrerenderStart(pending_prerender->handle.get());
      running_launcher_and_render_view_routes.insert(
          launcher_and_render_view_route);
    } else {
      pending_prerender->remote_processor_client->OnPrerenderStop();
      prerenders_.erase(it);
    }
  }
}

PrerenderLinkManager::LinkPrerender*
PrerenderLinkManager::FindByPrerenderHandle(PrerenderHandle* prerender_handle) {
  DCHECK(prerender_handle);
  for (auto& prerender : prerenders_) {
    if (prerender->handle.get() == prerender_handle)
      return prerender.get();
  }
  return nullptr;
}

PrerenderLinkManager::LinkPrerender* PrerenderLinkManager::FindByPrerenderId(
    int prerender_id) {
  for (auto& prerender : prerenders_) {
    if (prerender->prerender_id == prerender_id)
      return prerender.get();
  }
  return nullptr;
}

void PrerenderLinkManager::RemovePrerender(LinkPrerender* prerender) {
  for (auto it = prerenders_.begin(); it != prerenders_.end(); ++it) {
    LinkPrerender* current_prerender = it->get();
    if (current_prerender == prerender) {
      std::unique_ptr<PrerenderHandle> own_handle =
          std::move(prerender->handle);
      prerenders_.erase(it);
      return;
    }
  }
  NOTREACHED();
}

void PrerenderLinkManager::CancelPrerender(LinkPrerender* prerender) {
  for (auto it = prerenders_.begin(); it != prerenders_.end(); ++it) {
    LinkPrerender* current_prerender = it->get();
    if (current_prerender == prerender) {
      std::unique_ptr<PrerenderHandle> own_handle =
          std::move(prerender->handle);
      prerenders_.erase(it);
      if (own_handle)
        own_handle->OnCancel();
      return;
    }
  }
  NOTREACHED();
}

void PrerenderLinkManager::Shutdown() {
  has_shutdown_ = true;
}

// In practice, this is always called from
// PrerenderLinkManager::OnStartPrerender().
void PrerenderLinkManager::OnPrerenderStart(PrerenderHandle* prerender_handle) {
  LinkPrerender* prerender = FindByPrerenderHandle(prerender_handle);
  if (!prerender)
    return;

  prerender->remote_processor_client->OnPrerenderStart();
}

void PrerenderLinkManager::OnPrerenderStopLoading(
    PrerenderHandle* prerender_handle) {
  LinkPrerender* prerender = FindByPrerenderHandle(prerender_handle);
  if (!prerender)
    return;

  prerender->remote_processor_client->OnPrerenderStopLoading();
}

void PrerenderLinkManager::OnPrerenderDomContentLoaded(
    PrerenderHandle* prerender_handle) {
  LinkPrerender* prerender = FindByPrerenderHandle(prerender_handle);
  if (!prerender)
    return;

  prerender->remote_processor_client->OnPrerenderDomContentLoaded();
}

void PrerenderLinkManager::OnPrerenderStop(PrerenderHandle* prerender_handle) {
  LinkPrerender* prerender = FindByPrerenderHandle(prerender_handle);
  if (!prerender)
    return;

  prerender->remote_processor_client->OnPrerenderStop();

  RemovePrerender(prerender);
  StartPrerenders();
}

void PrerenderLinkManager::OnPrerenderNetworkBytesChanged(
    PrerenderHandle* prerender_handle) {}

}  // namespace prerender
