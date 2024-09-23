// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/guest_view_message_handler.h"

#include <memory>

#include "components/guest_view/browser/bad_message.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

using content::BrowserThread;
using content::RenderFrameHost;

namespace guest_view {

class ViewHandle : public mojom::ViewHandle {
 public:
  ViewHandle(int view_instance_id,
             base::WeakPtr<GuestViewManager> guest_view_manager,
             int render_process_id)
      : view_instance_id_(view_instance_id),
        guest_view_manager_(guest_view_manager),
        render_process_id_(render_process_id) {}

  ~ViewHandle() override {
    if (guest_view_manager_) {
      guest_view_manager_->ViewGarbageCollected(render_process_id_,
                                                view_instance_id_);
    }
  }

 private:
  const int view_instance_id_;
  base::WeakPtr<GuestViewManager> guest_view_manager_;
  const int render_process_id_;
};

GuestViewMessageHandler::GuestViewMessageHandler(
    const content::GlobalRenderFrameHostId& frame_id)
    : frame_id_(frame_id) {}

GuestViewMessageHandler::~GuestViewMessageHandler() = default;

GuestViewManager* GuestViewMessageHandler::GetOrCreateGuestViewManager() {
  auto* browser_context = GetBrowserContext();
  auto* manager = GuestViewManager::FromBrowserContext(browser_context);
  if (!manager) {
    manager = GuestViewManager::CreateWithDelegate(
        browser_context, CreateGuestViewManagerDelegate());
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
GuestViewMessageHandler::CreateGuestViewManagerDelegate() const {
  return std::make_unique<GuestViewManagerDelegate>();
}

content::BrowserContext* GuestViewMessageHandler::GetBrowserContext() const {
  auto* rph = content::RenderProcessHost::FromID(render_process_id());
  return rph ? rph->GetBrowserContext() : nullptr;
}

void GuestViewMessageHandler::ViewCreated(
    int view_instance_id,
    const std::string& view_type,
    mojo::PendingReceiver<mojom::ViewHandle> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!GetBrowserContext()) {
    return;
  }
  auto* guest_view_manager = GetOrCreateGuestViewManager();
  guest_view_manager->ViewCreated(render_process_id(), view_instance_id,
                                  view_type);
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ViewHandle>(view_instance_id,
                                   guest_view_manager->AsWeakPtr(),
                                   render_process_id()),
      std::move(receiver));
}

void GuestViewMessageHandler::AttachToEmbedderFrame(
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
  auto* outer_contents_frame = RenderFrameHost::FromID(frame_id_);

  const bool changed_owner_web_contents =
      owner_web_contents !=
      content::WebContents::FromRenderFrameHost(outer_contents_frame);

  if (changed_owner_web_contents) {
    guest->MaybeRecreateGuestContents(outer_contents_frame);
  }

  // Update the guest manager about the attachment.
  // This sets up the embedder and guest pairing information inside
  // the manager.
  manager->AttachGuest(render_process_id(), element_instance_id,
                       guest_instance_id, params);

  guest->AttachToOuterWebContentsFrame(
      std::move(owned_guest), outer_contents_frame, element_instance_id,
      false /* is_full_page_plugin */, std::move(callback));
}

}  // namespace guest_view
