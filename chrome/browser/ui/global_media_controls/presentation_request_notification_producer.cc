// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/presentation_request_notification_producer.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/media_router/common/providers/cast/cast_media_source.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

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
  return rfh ? content::WebContents::FromRenderFrameHost(rfh) : nullptr;
}
}  // namespace

class PresentationRequestNotificationProducer::
    PresentationRequestWebContentsObserver
    : public content::WebContentsObserver {
 public:
  PresentationRequestWebContentsObserver(
      content::WebContents* web_contents,
      PresentationRequestNotificationProducer* notification_producer)
      : content::WebContentsObserver(web_contents),
        notification_producer_(notification_producer) {
    DCHECK(notification_producer_);
  }

 private:
  void WebContentsDestroyed() override {
    notification_producer_->DeleteItemForPresentationRequest("Dialog closed.");
  }

  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override {
    notification_producer_->DeleteItemForPresentationRequest("Dialog closed.");
  }

  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override {
    notification_producer_->DeleteItemForPresentationRequest("Dialog closed.");
  }

  const raw_ptr<PresentationRequestNotificationProducer> notification_producer_;
};

PresentationRequestNotificationProducer::
    PresentationRequestNotificationProducer(
        MediaNotificationService* notification_service)
    : notification_service_(notification_service),
      item_manager_(notification_service_->media_item_manager()),
      item_ui_observer_set_(this) {
  item_manager_->AddObserver(this);
}

PresentationRequestNotificationProducer::
    ~PresentationRequestNotificationProducer() {
  item_manager_->RemoveObserver(this);
}

base::WeakPtr<media_message_center::MediaNotificationItem>
PresentationRequestNotificationProducer::GetMediaItem(const std::string& id) {
  if (item_ && item_->id() == id) {
    return item_->GetWeakPtr();
  } else {
    return nullptr;
  }
}

std::set<std::string>
PresentationRequestNotificationProducer::GetActiveControllableItemIds() const {
  return (item_ && !should_hide_) ? std::set<std::string>({item_->id()})
                                  : std::set<std::string>();
}

bool PresentationRequestNotificationProducer::HasFrozenItems() {
  return false;
}

void PresentationRequestNotificationProducer::OnItemShown(
    const std::string& id,
    global_media_controls::MediaItemUI* item_ui) {
  if (item_ui) {
    item_ui_observer_set_.Observe(id, item_ui);
  }
}

bool PresentationRequestNotificationProducer::IsItemActivelyPlaying(
    const std::string& id) {
  // TODO: This is a stub, since we currently only care about
  // MediaSessionNotificationProducer, but we probably should care about the
  // other ones.
  return false;
}

void PresentationRequestNotificationProducer::OnMediaItemUIDismissed(
    const std::string& id) {
  auto item = GetMediaItem(id);
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
  return item_ ? GetWebContentsFromPresentationRequest(item_->request())
               : nullptr;
}

base::WeakPtr<PresentationRequestNotificationItem>
PresentationRequestNotificationProducer::GetNotificationItem() {
  return item_ ? item_->GetWeakPtr() : nullptr;
}

void PresentationRequestNotificationProducer::OnItemListChanged() {
  ShowOrHideItem();
}
void PresentationRequestNotificationProducer::SetTestPresentationManager(
    base::WeakPtr<media_router::WebContentsPresentationManager>
        presentation_manager) {
  test_presentation_manager_ = presentation_manager;
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
          test_presentation_manager_
              ? test_presentation_manager_
              : GetActiveWebContentsPresentationManager()));
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
      item_manager_->HasOpenDialog()) {
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

void PresentationRequestNotificationProducer::OnPresentationsChanged(
    bool has_presentation) {
  if (has_presentation) {
    item_manager_->HideDialog();
    if (item_) {
      item_->Dismiss();
    }
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
  presentation_request_observer_ =
      std::make_unique<PresentationRequestWebContentsObserver>(
          GetWebContentsFromPresentationRequest(request), this);
  // This may replace an existing item, which is the right thing to do if
  // we've reached this point.
  item_.emplace(item_manager_, request, std::move(context));

  ShowOrHideItem();
}

void PresentationRequestNotificationProducer::DeleteItemForPresentationRequest(
    const std::string& message) {
  if (!item_)
    return;
  if (item_->context()) {
    item_->context()->InvokeErrorCallback(blink::mojom::PresentationError(
        blink::mojom::PresentationErrorType::PRESENTATION_REQUEST_CANCELLED,
        message));
  }
  const auto id{item_->id()};
  item_.reset();
  presentation_request_observer_.reset();
  item_manager_->HideItem(id);
}

void PresentationRequestNotificationProducer::ShowOrHideItem() {
  if (!item_) {
    should_hide_ = true;
    return;
  }

  // If the dialog is open and the item is currently hidden, do not show it.
  // Otherwise, the item might be shown when the user has dismissed a media
  // session notification or a cast notification from the same WebContents.
  if (should_hide_ && item_manager_->HasOpenDialog()) {
    return;
  }

  auto* web_contents = GetWebContentsFromPresentationRequest(item_->request());
  bool new_visibility =
      web_contents
          ? notification_service_->HasActiveNotificationsForWebContents(
                web_contents)
          : true;
  if (should_hide_ == new_visibility)
    return;

  should_hide_ = new_visibility;
  if (should_hide_) {
    item_manager_->HideItem(item_->id());
  } else {
    item_manager_->ShowItem(item_->id());
  }
}
