// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/presentation_request_notification_producer.h"

#include <utility>

#include "base/unguessable_token.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/presentation/presentation_service_delegate_impl.h"
#include "components/media_router/common/providers/cast/cast_media_source.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace {

base::WeakPtr<media_router::WebContentsPresentationManager>
GetActiveWebContentsPresentationManager() {
  auto* browser = chrome::FindLastActive();
  if (!browser)
    return nullptr;
  auto* tab_strip = browser->tab_strip_model();
  if (!tab_strip)
    return nullptr;
  auto* web_contents = tab_strip->GetActiveWebContents();
  if (!web_contents)
    return nullptr;
  return media_router::WebContentsPresentationManager::Get(web_contents);
}

content::WebContents* GetWebContentsFromPresentationRequest(
    const content::PresentationRequest& request) {
  auto* rfh = content::RenderFrameHost::FromID(request.render_frame_host_id);
  DCHECK(rfh);
  return content::WebContents::FromRenderFrameHost(rfh);
}
}  // namespace

PresentationRequestNotificationProducer::
    PresentationRequestNotificationProducer(
        MediaNotificationService* notification_service)
    : notification_service_(notification_service),
      container_observer_set_(this) {
  notification_service_->AddObserver(this);
}

PresentationRequestNotificationProducer::
    ~PresentationRequestNotificationProducer() {
  notification_service_->RemoveObserver(this);
}

base::WeakPtr<media_message_center::MediaNotificationItem>
PresentationRequestNotificationProducer::GetNotificationItem(
    const std::string& id) {
  if (item_ && item_->id() == id) {
    return item_->GetWeakPtr();
  } else {
    return nullptr;
  }
}

std::set<std::string>
PresentationRequestNotificationProducer::GetActiveControllableNotificationIds()
    const {
  return (item_ && !should_hide_) ? std::set<std::string>({item_->id()})
                                  : std::set<std::string>();
}

void PresentationRequestNotificationProducer::OnItemShown(
    const std::string& id,
    MediaNotificationContainerImpl* container) {
  if (container) {
    container_observer_set_.Observe(id, container);
  }
}

void PresentationRequestNotificationProducer::OnContainerDismissed(
    const std::string& id) {
  auto item = GetNotificationItem(id);
  if (item) {
    item->Dismiss();
    DeleteItemForPresentationRequest("Dialog closed.");
  }
}

void PresentationRequestNotificationProducer::OnStartPresentationContextCreated(
    std::unique_ptr<media_router::StartPresentationContext> context) {
  DCHECK(context);
  const auto& request = context->presentation_request();
  CreateItemForPresentationRequest(request, std::move(context));
}

content::WebContents*
PresentationRequestNotificationProducer::GetWebContents() {
  return GetWebContentsFromPresentationRequest(item_->request());
}

base::WeakPtr<PresentationRequestNotificationItem>
PresentationRequestNotificationProducer::GetNotificationItem() {
  return item_ ? item_->GetWeakPtr() : nullptr;
}

void PresentationRequestNotificationProducer::OnNotificationListChanged() {
  ShowOrHideItem();
}
void PresentationRequestNotificationProducer::SetPresentationManagerForTesting(
    base::WeakPtr<media_router::WebContentsPresentationManager>
        presentation_manager) {
  presentation_manager_ = presentation_manager;
  presentation_manager_->AddObserver(this);
}

void PresentationRequestNotificationProducer::OnMediaDialogOpened() {
  // At the point where this method is called, MediaNotificationService is
  // in a state where it can't accept new notifications.  As a workaround,
  // we simply defer the handling of the event.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PresentationRequestNotificationProducer::AfterMediaDialogOpened,
          weak_factory_.GetWeakPtr(),
          GetActiveWebContentsPresentationManager()));
}

void PresentationRequestNotificationProducer::OnMediaDialogClosed() {
  // This event needs to be handled asynchronously the be absolutely certain
  // it's handled later than a prior call to OnMediaDialogOpened().
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PresentationRequestNotificationProducer::AfterMediaDialogClosed,
          weak_factory_.GetWeakPtr()));
}

void PresentationRequestNotificationProducer::AfterMediaDialogOpened(
    base::WeakPtr<media_router::WebContentsPresentationManager>
        presentation_manager) {
  presentation_manager_ = presentation_manager;
  // It's possible the presentation manager was deleted since the call to
  // this method was scheduled.
  if (!presentation_manager_)
    return;

  presentation_manager_->AddObserver(this);

  // Handle any request that was created while we weren't watching, first
  // making sure the dialog hasn't been closed since the we found out it was
  // opening. This is the normal way notifications are created for a default
  // presentation request.
  if (presentation_manager_->HasDefaultPresentationRequest() &&
      notification_service_->HasOpenDialog()) {
    OnDefaultPresentationChanged(
        &presentation_manager_->GetDefaultPresentationRequest());
  }
}

void PresentationRequestNotificationProducer::AfterMediaDialogClosed() {
  DeleteItemForPresentationRequest("Dialog closed.");
  if (presentation_manager_)
    presentation_manager_->RemoveObserver(this);
  presentation_manager_ = nullptr;
}

void PresentationRequestNotificationProducer::OnMediaRoutesChanged(
    const std::vector<media_router::MediaRoute>& routes) {
  if (!routes.empty()) {
    notification_service_->HideMediaDialog();
    item_->Dismiss();
  }
}

void PresentationRequestNotificationProducer::OnDefaultPresentationChanged(
    const content::PresentationRequest* presentation_request) {
  // NOTE: We only observe the presentation manager while the media control
  // dialog is open, so this method is only handling the unusual case where
  // the default presentation request is changed while the dialog is open.
  // In the even more unusual case where the dialog is already open with a
  // notification for a non-default request, we ignored changes in the
  // default request.
  if (!HasItemForNonDefaultRequest()) {
    DeleteItemForPresentationRequest("Default presentation changed.");
    if (presentation_request) {
      CreateItemForPresentationRequest(*presentation_request, nullptr);
    }
  }
}

void PresentationRequestNotificationProducer::CreateItemForPresentationRequest(
    const content::PresentationRequest& request,
    std::unique_ptr<media_router::StartPresentationContext> context) {
  // This may replace an existing item, which is the right thing to do if
  // we've reached this point.
  item_.emplace(notification_service_, request, std::move(context));

  ShowOrHideItem();
}

void PresentationRequestNotificationProducer::DeleteItemForPresentationRequest(
    const std::string& message) {
  if (item_) {
    if (item_->context()) {
      item_->context()->InvokeErrorCallback(blink::mojom::PresentationError(
          blink::mojom::PresentationErrorType::PRESENTATION_REQUEST_CANCELLED,
          message));
    }
    const auto id{item_->id()};
    item_.reset();
    notification_service_->RemoveItem(id);
  }
}

void PresentationRequestNotificationProducer::ShowOrHideItem() {
  if (!item_) {
    should_hide_ = true;
    return;
  }

  bool new_visibility =
      notification_service_->HasActiveNotificationsForWebContents(
          GetWebContentsFromPresentationRequest(item_->request()));
  if (should_hide_ == new_visibility)
    return;

  should_hide_ = new_visibility;
  if (should_hide_) {
    notification_service_->HideNotification(item_->id());
  } else {
    notification_service_->ShowNotification(item_->id());
  }
}
