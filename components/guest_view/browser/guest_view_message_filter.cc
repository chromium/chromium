// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/guest_view_message_filter.h"

#include <memory>

#include "base/memory/singleton.h"
#include "components/guest_view/browser/bad_message.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/common/guest_view_messages.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "ipc/ipc_message_macros.h"

using content::BrowserContext;
using content::BrowserThread;
using content::RenderFrameHost;
using content::WebContents;

namespace guest_view {

namespace {

class ShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static ShutdownNotifierFactory* GetInstance() {
    return base::Singleton<ShutdownNotifierFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ShutdownNotifierFactory>;

  ShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "GuestViewMessageFilter") {}
  ~ShutdownNotifierFactory() override = default;
};

}  // namespace

GuestViewMessageFilter::GuestViewMessageFilter(
    const uint32_t* message_classes_to_filter,
    size_t num_message_classes_to_filter,
    int render_process_id,
    BrowserContext* context)
    : BrowserMessageFilter(message_classes_to_filter,
                           num_message_classes_to_filter),
      render_process_id_(render_process_id),
      browser_context_(context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_context_shutdown_subscription_ =
      ShutdownNotifierFactory::GetInstance()->Get(context)->Subscribe(
          base::BindRepeating(&GuestViewMessageFilter::OnBrowserContextShutdown,
                              base::Unretained(this)));
}

GuestViewMessageFilter::~GuestViewMessageFilter() {
  // The filter is destroyed on the UI thread as
  // |browser_context_shutdown_subscription_| was created there.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void GuestViewMessageFilter::EnsureShutdownNotifierFactoryBuilt() {
  ShutdownNotifierFactory::GetInstance();
}

void GuestViewMessageFilter::OnBrowserContextShutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_context_ = nullptr;
  browser_context_shutdown_subscription_.reset();
}

GuestViewManager* GuestViewMessageFilter::GetOrCreateGuestViewManager() {
  DCHECK(browser_context_);
  auto* manager = GuestViewManager::FromBrowserContext(browser_context_);
  if (!manager) {
    manager = GuestViewManager::CreateWithDelegate(
        browser_context_, std::make_unique<GuestViewManagerDelegate>());
  }
  return manager;
}

GuestViewManager* GuestViewMessageFilter::GetGuestViewManagerOrKill() {
  DCHECK(browser_context_);
  auto* manager = GuestViewManager::FromBrowserContext(browser_context_);
  if (!manager) {
    bad_message::ReceivedBadMessage(
        this, bad_message::GVMF_UNEXPECTED_MESSAGE_BEFORE_GVM_CREATION);
  }
  return manager;
}

void GuestViewMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  if (IPC_MESSAGE_CLASS(message) == GuestViewMsgStart)
    *thread = BrowserThread::UI;
}

void GuestViewMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnUIThread::Destruct(this);
}

bool GuestViewMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GuestViewMessageFilter, message)
    IPC_MESSAGE_HANDLER(GuestViewHostMsg_AttachGuest, OnAttachGuest)
    IPC_MESSAGE_HANDLER(GuestViewHostMsg_AttachToEmbedderFrame,
                        OnAttachToEmbedderFrame)
    IPC_MESSAGE_HANDLER(GuestViewHostMsg_ViewCreated, OnViewCreated)
    IPC_MESSAGE_HANDLER(GuestViewHostMsg_ViewGarbageCollected,
                        OnViewGarbageCollected)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GuestViewMessageFilter::OnViewCreated(int view_instance_id,
                                           const std::string& view_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;
  GetOrCreateGuestViewManager()->ViewCreated(render_process_id_,
                                             view_instance_id, view_type);
}

void GuestViewMessageFilter::OnViewGarbageCollected(int view_instance_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;
  GetOrCreateGuestViewManager()->ViewGarbageCollected(render_process_id_,
                                                      view_instance_id);
}

void GuestViewMessageFilter::OnAttachGuest(
    int element_instance_id,
    int guest_instance_id,
    const base::DictionaryValue& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!browser_context_)
    return;

  // We should have a GuestViewManager at this point. If we don't then the
  // embedder is misbehaving.
  auto* manager = GetGuestViewManagerOrKill();
  if (!manager)
    return;

  manager->AttachGuest(render_process_id_,
                       element_instance_id,
                       guest_instance_id,
                       params);
}

void GuestViewMessageFilter::OnAttachToEmbedderFrame(
    int embedder_local_render_frame_id,
    int element_instance_id,
    int guest_instance_id,
    const base::DictionaryValue& params) {
  if (!browser_context_)
    return;

  // We should have a GuestViewManager at this point. If we don't then the
  // embedder is misbehaving.
  auto* manager = GetGuestViewManagerOrKill();
  if (!manager)
    return;

  content::WebContents* guest_web_contents =
      manager->GetGuestByInstanceIDSafely(guest_instance_id,
                                          render_process_id_);
  if (!guest_web_contents)
    return;

  auto* guest = GuestViewBase::FromWebContents(guest_web_contents);
  content::WebContents* owner_web_contents = guest->owner_web_contents();
  DCHECK(owner_web_contents);
  auto* embedder_frame = RenderFrameHost::FromID(
      render_process_id_, embedder_local_render_frame_id);

  // Update the guest manager about the attachment.
  // This sets up the embedder and guest pairing information inside
  // the manager.
  manager->AttachGuest(render_process_id_, element_instance_id,
                       guest_instance_id, params);

  guest->AttachToOuterWebContentsFrame(embedder_frame, element_instance_id,
                                       false /* is_full_page_plugin */);
}

}  // namespace guest_view
