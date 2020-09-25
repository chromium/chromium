// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/presentation_request_notification_provider.h"

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

}  // namespace

PresentationRequestNotificationProvider::
    PresentationRequestNotificationProvider(
        MediaNotificationService* notification_service)
    : notification_service_(notification_service) {
  notification_service_->AddObserver(this);
}

PresentationRequestNotificationProvider::
    ~PresentationRequestNotificationProvider() {
  notification_service_->RemoveObserver(this);
}

base::WeakPtr<media_message_center::MediaNotificationItem>
PresentationRequestNotificationProvider::GetNotificationItem(
    const std::string& id) {
  if (item_) {
    DCHECK_EQ(item_->id(), id);
    return item_->GetWeakPtr();
  }
  return nullptr;
}

void PresentationRequestNotificationProvider::OnStartPresentationContextCreated(
    std::unique_ptr<media_router::StartPresentationContext> context) {
  DCHECK(context);
  const auto& request = context->presentation_request();
  CreateItemForPresentationRequest(request, std::move(context));
}

void PresentationRequestNotificationProvider::OnNotificationListChanged() {}

void PresentationRequestNotificationProvider::OnMediaDialogOpened() {
  // At the point where this method is called, MediaNotificationService is in
  // a state where it can't accept new notifications.  As a workaround, we
  // simply defer the handling of the event.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PresentationRequestNotificationProvider::AfterMediaDialogOpened,
          base::Unretained(this), GetActiveWebContentsPresentationManager()));
}

void PresentationRequestNotificationProvider::OnMediaDialogClosed() {
  // This event needs to be handled asynchronously the be absolutely certain
  // it's handled later than a prior call to OnMediaDialogOpened().
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PresentationRequestNotificationProvider::AfterMediaDialogClosed,
          base::Unretained(this), GetActiveWebContentsPresentationManager()));
}

void PresentationRequestNotificationProvider::AfterMediaDialogOpened(
    base::WeakPtr<media_router::WebContentsPresentationManager>
        presentation_manager) {
  // It's possible the presentation manager was deleted since the call to this
  // method was scheduled.
  if (!presentation_manager)
    return;

  presentation_manager->AddObserver(this);

  // Handle any request that was created while we weren't watching, first making
  // sure the dialog hasn't been closed since the we found out it was opening.
  // This is the normal way notifications are created for a default presentation
  // request.
  if (presentation_manager->HasDefaultPresentationRequest() &&
      notification_service_->HasOpenDialog()) {
    CreateItemForPresentationRequest(
        presentation_manager->GetDefaultPresentationRequest(), nullptr);
  }
}

void PresentationRequestNotificationProvider::AfterMediaDialogClosed(
    base::WeakPtr<media_router::WebContentsPresentationManager>
        presentation_manager) {
  item_.reset();
  if (presentation_manager)
    presentation_manager->RemoveObserver(this);
}

void PresentationRequestNotificationProvider::OnDefaultPresentationChanged(
    const content::PresentationRequest* presentation_request) {
  // NOTE: We only observe the presentation manager while the media control
  // dialog is open, so this method is only handling the unusual case where the
  // default presentation request is changed while the dialog is open.  In the
  // even more unusual case where the dialog is already open with a notification
  // for a non-default request, we ignored changes in the default request.
  if (!HasItemForNonDefaultRequest()) {
    if (presentation_request) {
      CreateItemForPresentationRequest(*presentation_request, nullptr);
    } else {
      item_.reset();
    }
  }
}

void PresentationRequestNotificationProvider::CreateItemForPresentationRequest(
    const content::PresentationRequest& request,
    std::unique_ptr<media_router::StartPresentationContext> context) {
  // This may replace an existing item, which is the right thing to do if we've
  // reached this point.
  item_.emplace(notification_service_, request, std::move(context));
  notification_service_->ShowNotification(item_->id());
}
