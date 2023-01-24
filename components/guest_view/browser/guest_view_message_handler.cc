// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/guest_view_message_handler.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "components/guest_view/browser/bad_message.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"

using content::BrowserThread;
using content::RenderFrameHost;

namespace guest_view {

GuestViewMessageHandler::GuestViewMessageHandler(int render_process_id)
    : render_process_id_(render_process_id) {}

GuestViewMessageHandler::~GuestViewMessageHandler() = default;

GuestViewManager* GuestViewMessageHandler::GetOrCreateGuestViewManager() {
  auto* browser_context = GetBrowserContext();
  auto* manager = GuestViewManager::FromBrowserContext(browser_context);
  if (!manager) {
    manager = GuestViewManager::CreateWithDelegate(
        browser_context, CreateGuestViewManagerDelegate(browser_context));
  }
  return manager;
}

GuestViewManager* GuestViewMessageHandler::GetGuestViewManagerOrKill() {
  auto* manager = GuestViewManager::FromBrowserContext(GetBrowserContext());
  if (!manager) {
    bad_message::ReceivedBadMessage(
        render_process_id(),
        bad_message::GVMF_UNEXPECTED_MESSAGE_BEFORE_GVM_CREATION);
  }
  return manager;
}

std::unique_ptr<GuestViewManagerDelegate>
GuestViewMessageHandler::CreateGuestViewManagerDelegate(
    content::BrowserContext* context) const {
  return std::make_unique<GuestViewManagerDelegate>();
}

content::BrowserContext* GuestViewMessageHandler::GetBrowserContext() const {
  auto* rph = content::RenderProcessHost::FromID(render_process_id());
  return rph ? rph->GetBrowserContext() : nullptr;
}

void GuestViewMessageHandler::ViewCreated(int view_instance_id,
                                          const std::string& view_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!GetBrowserContext())
    return;
  GetOrCreateGuestViewManager()->ViewCreated(render_process_id(),
                                             view_instance_id, view_type);
}

void GuestViewMessageHandler::ViewGarbageCollected(int view_instance_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!GetBrowserContext())
    return;
  GetOrCreateGuestViewManager()->ViewGarbageCollected(render_process_id(),
                                                      view_instance_id);
}

void GuestViewMessageHandler::AttachToEmbedderFrame(
    int embedder_local_render_frame_id,
    int element_instance_id,
    int guest_instance_id,
    base::Value::Dict params,
    AttachToEmbedderFrameCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!GetBrowserContext()) {
    std::move(callback).Run();
    return;
  }

  // We should have a GuestViewManager at this point. If we don't then the
  // embedder is misbehaving.
  auto* manager = GetGuestViewManagerOrKill();
  if (!manager) {
    // The renderer has been killed, and this event logged, by
    // `ReceivedBadMessage`, so we can just return.
    std::move(callback).Run();
    return;
  }

  GuestViewBase* guest = manager->GetGuestByInstanceIDSafely(
      guest_instance_id, render_process_id());
  if (!guest) {
    std::move(callback).Run();
    return;
  }

  std::unique_ptr<GuestViewBase> owned_guest =
      manager->TransferOwnership(guest);
  DCHECK_EQ(owned_guest.get(), guest);

  content::WebContents* owner_web_contents = guest->owner_web_contents();
  DCHECK(owner_web_contents);
  auto* embedder_frame = RenderFrameHost::FromID(
      render_process_id(), embedder_local_render_frame_id);

  const bool changed_owner_web_contents =
      owner_web_contents !=
      content::WebContents::FromRenderFrameHost(embedder_frame);
  base::UmaHistogramBoolean(
      "Extensions.GuestView.ChangeOwnerWebContentsOnAttach",
      changed_owner_web_contents);

  if (changed_owner_web_contents) {
    guest->MaybeRecreateGuestContents(
        content::WebContents::FromRenderFrameHost(embedder_frame));
  }

  // Update the guest manager about the attachment.
  // This sets up the embedder and guest pairing information inside
  // the manager.
  manager->AttachGuest(render_process_id(), element_instance_id,
                       guest_instance_id, params);

  guest->AttachToOuterWebContentsFrame(
      std::move(owned_guest), embedder_frame, element_instance_id,
      false /* is_full_page_plugin */, std::move(callback));
}

}  // namespace guest_view
