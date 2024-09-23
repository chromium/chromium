// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/presentation_request_notification_producer.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/media_router/common/providers/cast/cast_media_source.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace {

base::WeakPtr<media_router::WebContentsPresentationManager>
GetActiveWebContentsPresentationManager() {
  auto* browser = chrome::FindLastActive();
  if (!browser) {
    return nullptr;
  }
  auto* tab_strip = browser->tab_strip_model();
  if (!tab_strip) {
    return nullptr;
  }
  auto* web_contents = tab_strip->GetActiveWebContents();
  if (!web_contents) {
    return nullptr;
  }
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
        base::RepeatingCallback<bool(content::WebContents*)>
            has_active_notifications_callback,
        const base::UnguessableToken& source_id)
    : observer_receiver_(this),
      has_active_notifications_callback_(
          std::move(has_active_notifications_callback)),
      source_id_(source_id) {}

PresentationRequestNotificationProducer::
    ~PresentationRequestNotificationProducer() = default;

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

void PresentationRequestNotificationProducer::BindProvider(
    mojo::PendingRemote<global_media_controls::mojom::DevicePickerProvider>
        pending_provider) {
  provider_.Bind(std::move(pending_provider));
  provider_->AddObserver(observer_receiver_.BindNewPipeAndPassRemote());
  provider_.set_disconnect_handler(base::BindOnce(
      [](PresentationRequestNotificationProducer* self) {
        self->item_.reset();
      },
      base::Unretained(this)));
}

void PresentationRequestNotificationProducer::SetTestPresentationManager(
    base::WeakPtr<media_router::WebContentsPresentationManager>
        presentation_manager) {
  test_presentation_manager_ = presentation_manager;
}

void PresentationRequestNotificationProducer::OnMediaUIOpened() {
  is_dialog_open_ = true;
  presentation_manager_ = test_presentation_manager_
                              ? test_presentation_manager_
                              : GetActiveWebContentsPresentationManager();
  // It's possible the presentation manager was deleted since the call to
  // this method was scheduled.
  if (!presentation_manager_) {
    return;
  }
  presentation_manager_->AddObserver(this);

  // Handle any request that was created while we weren't watching, first
  // making sure the dialog hasn't been closed since the we found out it was
  // opening. This is the normal way notifications are created for a default
  // presentation request.
  if (presentation_manager_->HasDefaultPresentationRequest()) {
    OnDefaultPresentationChanged(
        &presentation_manager_->GetDefaultPresentationRequest());
  }
}

void PresentationRequestNotificationProducer::OnMediaUIClosed() {
  is_dialog_open_ = false;
  DeleteItemForPresentationRequest("Dialog closed.");
  if (presentation_manager_) {
    presentation_manager_->RemoveObserver(this);
  }
  presentation_manager_ = nullptr;
}

void PresentationRequestNotificationProducer::OnMediaUIUpdated() {
  ShowOrHideItem();
}

void PresentationRequestNotificationProducer::OnPickerDismissed() {
  DeleteItemForPresentationRequest("Dialog closed.");
}

void PresentationRequestNotificationProducer::OnPresentationsChanged(
    bool has_presentation) {
  // If there is a presentation, there would already be an item associated with
  // that, so `this` doesn't have to provide another item.
  if (has_presentation && provider_.is_bound()) {
#if !BUILDFLAG(IS_CHROMEOS)
    provider_->HideMediaUI();
#endif
    provider_->HideItem();
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
  item_.emplace(request, std::move(context), provider_);
  if (provider_.is_bound()) {
    provider_->CreateItem(source_id_);
  }
  ShowOrHideItem();
}

void PresentationRequestNotificationProducer::DeleteItemForPresentationRequest(
    const std::string& message) {
  if (!item_) {
    return;
  }
  if (item_->context()) {
    item_->context()->InvokeErrorCallback(blink::mojom::PresentationError(
        blink::mojom::PresentationErrorType::PRESENTATION_REQUEST_CANCELLED,
        message));
  }
  item_.reset();
  presentation_request_observer_.reset();
  if (provider_.is_bound()) {
    provider_->DeleteItem();
  }
}

void PresentationRequestNotificationProducer::ShowOrHideItem() {
  if (!item_ || !provider_.is_bound()) {
    should_show_ = false;
    return;
  }
  // If the dialog is open and the item is currently hidden, do not show it.
  // Otherwise, the item might be shown when the user has dismissed a media
  // session notification or a cast notification from the same WebContents.
  if (!should_show_ && is_dialog_open_) {
    return;
  }
  auto* web_contents = GetWebContentsFromPresentationRequest(item_->request());
  bool new_visibility =
      web_contents && !has_active_notifications_callback_.Run(web_contents);
  if (should_show_ == new_visibility) {
    return;
  }
  should_show_ = new_visibility;
  if (should_show_) {
    provider_->ShowItem();
  } else {
    provider_->HideItem();
  }
}
