// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/browser/no_state_prefetch_link_manager.h"

#include <functional>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_handle.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/common/referrer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

// TODO(crbug.com/40520585): Use a dedicated build flag for GuestView.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_FUCHSIA)
#include "components/guest_view/browser/guest_view_base.h"  // nogncheck
#endif

using base::TimeTicks;
using content::RenderViewHost;
using content::SessionStorageNamespace;

namespace prerender {

namespace {

int GetNextLinkTriggerId() {
  static int next_id = 1;
  return next_id++;
}

}  // namespace

NoStatePrefetchLinkManager::LinkTrigger::LinkTrigger(
    int launcher_render_process_id,
    int launcher_render_view_id,
    blink::mojom::PrerenderAttributesPtr attributes,
    const url::Origin& initiator_origin,
    base::TimeTicks creation_time,
    NoStatePrefetchContents* deferred_launcher)
    : launcher_render_process_id(launcher_render_process_id),
      launcher_render_view_id(launcher_render_view_id),
      url(attributes->url),
      trigger_type(attributes->trigger_type),
      referrer(content::Referrer(*attributes->referrer)),
      initiator_origin(initiator_origin),
      size(attributes->view_size),
      creation_time(creation_time),
      deferred_launcher(deferred_launcher),
      has_been_abandoned(false),
      link_trigger_id(GetNextLinkTriggerId()) {}

NoStatePrefetchLinkManager::LinkTrigger::~LinkTrigger() {
  DCHECK_EQ(nullptr, handle.get())
      << "The NoStatePrefetchHandle should be destroyed before its Prerender.";
}

NoStatePrefetchLinkManager::NoStatePrefetchLinkManager(
    NoStatePrefetchManager* manager)
    : has_shutdown_(false), manager_(manager) {}

NoStatePrefetchLinkManager::~NoStatePrefetchLinkManager() {
  for (auto& trigger : triggers_) {
    if (trigger->handle) {
      DCHECK(!trigger->handle->IsPrefetching())
          << "All running prefetchers should stop at the same time as the "
          << "NoStatePrefetchManager.";
      trigger->handle.reset();
    }
  }
}

std::optional<int> NoStatePrefetchLinkManager::OnStartLinkTrigger(
    int launcher_render_process_id,
    int launcher_render_view_id,
    int launcher_render_frame_id,
    blink::mojom::PrerenderAttributesPtr attributes,
    const url::Origin& initiator_origin) {
// TODO(crbug.com/40520585): Use a dedicated build flag for GuestView.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_FUCHSIA)
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      launcher_render_process_id, launcher_render_frame_id);
  // Guests inside <webview> do not support cross-process navigation and so we
  // do not allow guests to prerender content.
  if (guest_view::GuestViewBase::IsGuest(rfh))
    return std::nullopt;
#endif

  // Check if the launcher is itself an unswapped prerender.
  NoStatePrefetchContents* no_state_prefetch_contents =
      manager_->GetNoStatePrefetchContentsForRoute(launcher_render_process_id,
                                                   launcher_render_view_id);
  if (no_state_prefetch_contents &&
      no_state_prefetch_contents->final_status() != FINAL_STATUS_UNKNOWN) {
    // The launcher is a prerender about to be destroyed asynchronously, but
    // its AddLinkRelPrerender message raced with shutdown. Ignore it.
    DCHECK_NE(FINAL_STATUS_USED, no_state_prefetch_contents->final_status());
    return std::nullopt;
  }

  auto trigger = std::make_unique<LinkTrigger>(
      launcher_render_process_id, launcher_render_view_id,
      std::move(attributes), initiator_origin, manager_->GetCurrentTimeTicks(),
      no_state_prefetch_contents);

  // Stash pointer used only for comparison later.
  const LinkTrigger* trigger_ptr = trigger.get();

  triggers_.push_back(std::move(trigger));

  if (!no_state_prefetch_contents)
    StartLinkTriggers();

  // Check if the trigger we added is still at the end of the list. It
  // may have been discarded by StartLinkTriggers().
  if (!triggers_.empty() && triggers_.back().get() == trigger_ptr)
    return trigger_ptr->link_trigger_id;
  return std::nullopt;
}

void NoStatePrefetchLinkManager::OnCancelLinkTrigger(int link_trigger_id) {
  LinkTrigger* trigger = FindByLinkTriggerId(link_trigger_id);
  if (!trigger)
    return;
  CancelLinkTrigger(trigger);
  StartLinkTriggers();
}

void NoStatePrefetchLinkManager::OnAbandonLinkTrigger(int link_trigger_id) {
  LinkTrigger* trigger = FindByLinkTriggerId(link_trigger_id);
  if (!trigger)
    return;

  if (!trigger->handle) {
    RemoveLinkTrigger(trigger);
    return;
  }

  trigger->has_been_abandoned = true;
  trigger->handle->OnNavigateAway();
  DCHECK(trigger->handle);

  // If the prefetcher is not running, remove it from the list so it does not
  // leak. If it is running, it will send a cancel event when it stops which
  // will remove it.
  if (!trigger->handle->IsPrefetching())
    RemoveLinkTrigger(trigger);
}

bool NoStatePrefetchLinkManager::IsEmpty() const {
  return triggers_.empty();
}

bool NoStatePrefetchLinkManager::TriggerIsRunningForTesting(
    LinkTrigger* trigger) const {
  return trigger->handle.get() != nullptr;
}

size_t NoStatePrefetchLinkManager::CountRunningTriggers() const {
  return base::ranges::count_if(
      triggers_, [](const std::unique_ptr<LinkTrigger>& trigger) {
        return trigger->handle && trigger->handle->IsPrefetching();
      });
}

void NoStatePrefetchLinkManager::StartLinkTriggers() {
  if (has_shutdown_)
    return;

  size_t total_started_trigger_count = 0;
  std::list<LinkTrigger*> abandoned_triggers;
  std::list<std::list<std::unique_ptr<LinkTrigger>>::iterator> pending_triggers;
  std::multiset<std::pair<int, int>> running_launcher_and_render_view_routes;

  // Scan the list, counting how many prefetches have handles (and so were added
  // to the NoStatePrefetchManager). The count is done for the system as a
  // whole, and also per launcher.
  for (auto it = triggers_.begin(); it != triggers_.end(); ++it) {
    std::unique_ptr<LinkTrigger>& trigger = *it;
    // Skip triggers launched by a trigger.
    if (trigger->deferred_launcher)
      continue;
    if (!trigger->handle) {
      pending_triggers.push_back(it);
    } else {
      ++total_started_trigger_count;
      if (trigger->has_been_abandoned) {
        abandoned_triggers.push_back(trigger.get());
      } else {
        // We do not count abandoned prefetches towards their launcher, since it
        // has already navigated on to another page.
        std::pair<int, int> launcher_and_render_view_route(
            trigger->launcher_render_process_id,
            trigger->launcher_render_view_id);
        running_launcher_and_render_view_routes.insert(
            launcher_and_render_view_route);
        DCHECK_GE(manager_->config().max_link_concurrency_per_launcher,
                  running_launcher_and_render_view_routes.count(
                      launcher_and_render_view_route));
      }
    }
  }
  DCHECK_LE(abandoned_triggers.size(), total_started_trigger_count);
  DCHECK_GE(manager_->config().max_link_concurrency,
            total_started_trigger_count);
  DCHECK_LE(CountRunningTriggers(), total_started_trigger_count);

  TimeTicks now = manager_->GetCurrentTimeTicks();

  // Scan the pending triggers, starting triggers as we can.
  for (const std::list<std::unique_ptr<LinkTrigger>>::iterator& it :
       pending_triggers) {
    LinkTrigger* pending_trigger = it->get();

    base::TimeDelta trigger_age = now - pending_trigger->creation_time;
    if (trigger_age >= manager_->config().max_wait_to_launch) {
      // This trigger waited too long in the queue before launching.
      triggers_.erase(it);
      continue;
    }

    std::pair<int, int> launcher_and_render_view_route(
        pending_trigger->launcher_render_process_id,
        pending_trigger->launcher_render_view_id);
    if (manager_->config().max_link_concurrency_per_launcher <=
        running_launcher_and_render_view_routes.count(
            launcher_and_render_view_route)) {
      // This trigger's launcher is already at its limit.
      continue;
    }

    if (total_started_trigger_count >=
            manager_->config().max_link_concurrency ||
        total_started_trigger_count >= triggers_.size()) {
      // The system is already at its prerender concurrency limit. Try removing
      // an abandoned trigger, if one exists, to make room.
      if (abandoned_triggers.empty())
        return;

      CancelLinkTrigger(abandoned_triggers.front());
      --total_started_trigger_count;
      abandoned_triggers.pop_front();
    }

    std::unique_ptr<NoStatePrefetchHandle> handle =
        manager_->StartPrefetchingFromLinkRelPrerender(
            pending_trigger->launcher_render_process_id,
            pending_trigger->launcher_render_view_id, pending_trigger->url,
            pending_trigger->trigger_type, pending_trigger->referrer,
            pending_trigger->initiator_origin, pending_trigger->size);
    if (!handle) {
      // This trigger couldn't be launched, it's gone.
      triggers_.erase(it);
      continue;
    }

    if (handle->IsPrefetching()) {
      // We have successfully started a new prefetcher.
      pending_trigger->handle = std::move(handle);
      ++total_started_trigger_count;
      pending_trigger->handle->SetObserver(this);
      running_launcher_and_render_view_routes.insert(
          launcher_and_render_view_route);
    } else {
      triggers_.erase(it);
    }
  }
}

NoStatePrefetchLinkManager::LinkTrigger*
NoStatePrefetchLinkManager::FindByNoStatePrefetchHandle(
    NoStatePrefetchHandle* no_state_prefetch_handle) {
  DCHECK(no_state_prefetch_handle);
  for (auto& trigger : triggers_) {
    if (trigger->handle.get() == no_state_prefetch_handle)
      return trigger.get();
  }
  return nullptr;
}

NoStatePrefetchLinkManager::LinkTrigger*
NoStatePrefetchLinkManager::FindByLinkTriggerId(int link_trigger_id) {
  for (auto& trigger : triggers_) {
    if (trigger->link_trigger_id == link_trigger_id)
      return trigger.get();
  }
  return nullptr;
}

void NoStatePrefetchLinkManager::RemoveLinkTrigger(LinkTrigger* trigger) {
  for (auto it = triggers_.begin(); it != triggers_.end(); ++it) {
    LinkTrigger* current_trigger = it->get();
    if (current_trigger == trigger) {
      std::unique_ptr<NoStatePrefetchHandle> own_handle =
          std::move(trigger->handle);
      triggers_.erase(it);
      return;
    }
  }
  NOTREACHED_IN_MIGRATION();
}

void NoStatePrefetchLinkManager::CancelLinkTrigger(LinkTrigger* trigger) {
  for (auto it = triggers_.begin(); it != triggers_.end(); ++it) {
    LinkTrigger* current_trigger = it->get();
    if (current_trigger == trigger) {
      std::unique_ptr<NoStatePrefetchHandle> own_handle =
          std::move(trigger->handle);
      triggers_.erase(it);
      if (own_handle)
        own_handle->OnCancel();
      return;
    }
  }
  NOTREACHED_IN_MIGRATION();
}

void NoStatePrefetchLinkManager::Shutdown() {
  has_shutdown_ = true;
}

void NoStatePrefetchLinkManager::OnPrefetchStop(
    NoStatePrefetchHandle* no_state_prefetch_handle) {
  LinkTrigger* trigger = FindByNoStatePrefetchHandle(no_state_prefetch_handle);
  if (!trigger)
    return;
  RemoveLinkTrigger(trigger);
  StartLinkTriggers();
}

}  // namespace prerender
