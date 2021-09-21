// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_notification_service.h"

#include <memory>

#include "base/callback_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_provider_impl.h"
#include "chrome/browser/ui/global_media_controls/media_session_notification_item.h"
#include "chrome/browser/ui/global_media_controls/media_session_notification_producer.h"
#include "chrome/browser/ui/media_router/media_router_ui.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/global_media_controls/public/media_dialog_delegate.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/global_media_controls/public/media_item_producer.h"
#include "components/global_media_controls/public/media_item_ui.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/media_session_service.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace {
void CancelRequest(
    std::unique_ptr<media_router::StartPresentationContext> context,
    const std::string& message) {
  context->InvokeErrorCallback(blink::mojom::PresentationError(
      blink::mojom::PresentationErrorType::PRESENTATION_REQUEST_CANCELLED,
      message));
}
}  // namespace

MediaNotificationService::MediaNotificationService(
    Profile* profile,
    bool show_from_all_profiles) {
  item_manager_ = global_media_controls::MediaItemManager::Create();

  media_session_notification_producer_ =
      std::make_unique<MediaSessionNotificationProducer>(
          item_manager_.get(), profile, show_from_all_profiles);
  item_manager_->AddItemProducer(media_session_notification_producer_.get());

  if (media_router::MediaRouterEnabled(profile)) {
    if (base::FeatureList::IsEnabled(media::kGlobalMediaControlsForCast)) {
      // base::Unretained() is safe here because cast_notification_producer_ is
      // deleted before item_manager_.
      cast_notification_producer_ =
          std::make_unique<CastMediaNotificationProducer>(
              profile, item_manager_.get(),
              base::BindRepeating(
                  &global_media_controls::MediaItemManager::OnItemsChanged,
                  base::Unretained(item_manager_.get())));
      item_manager_->AddItemProducer(cast_notification_producer_.get());
    }
    if (media_router::GlobalMediaControlsCastStartStopEnabled()) {
      presentation_request_notification_producer_ =
          std::make_unique<PresentationRequestNotificationProducer>(this);
      item_manager_->AddItemProducer(
          presentation_request_notification_producer_.get());
    }
  }
}

MediaNotificationService::~MediaNotificationService() {
  item_manager_->RemoveItemProducer(media_session_notification_producer_.get());
}

void MediaNotificationService::Shutdown() {
  // |cast_notification_producer_| and
  // |presentation_request_notification_producer_| depend on MediaRouter,
  // which is another keyed service.
  if (cast_notification_producer_)
    item_manager_->RemoveItemProducer(cast_notification_producer_.get());

  if (presentation_request_notification_producer_) {
    item_manager_->RemoveItemProducer(
        presentation_request_notification_producer_.get());
  }

  cast_notification_producer_.reset();
  presentation_request_notification_producer_.reset();
}

void MediaNotificationService::SetDialogDelegateForWebContents(
    global_media_controls::MediaDialogDelegate* delegate,
    content::WebContents* contents) {
  DCHECK(delegate);
  DCHECK(contents);

  // When the dialog is opened by a PresentationRequest, there should be only
  // one notification, in the following priority order:
  // 1. A cast session associated with |contents|.
  // 2. A local media session associated with |contents|.
  // 3. A supplemental notification populated using the PresentationRequest.
  std::string item_id;

  // Find the cast notification item associated with |contents|.
  auto routes = media_router::WebContentsPresentationManager::Get(contents)
                    ->GetMediaRoutes();
  if (!routes.empty()) {
    // It is possible for a sender page to connect to two routes. For the
    // sake of the Zenith dialog, only one notification is needed.
    item_id = routes.begin()->media_route_id();
  } else if (media_session_notification_producer_
                 ->HasActiveControllableSessionForWebContents(contents)) {
    item_id = media_session_notification_producer_
                  ->GetActiveControllableSessionForWebContents(contents);
  } else {
    auto presentation_item =
        presentation_request_notification_producer_->GetNotificationItem();
    item_id = presentation_item->id();
    DCHECK(presentation_request_notification_producer_->GetWebContents() ==
           contents);
  }

  item_manager_->SetDialogDelegateForId(delegate, item_id);
}

bool MediaNotificationService::HasActiveNotificationsForWebContents(
    content::WebContents* web_contents) const {
  bool has_media_session =
      media_session_notification_producer_ &&
      media_session_notification_producer_
          ->HasActiveControllableSessionForWebContents(web_contents);
  return HasCastNotificationsForWebContents(web_contents) || has_media_session;
}

bool MediaNotificationService::HasLocalCastNotifications() const {
  return cast_notification_producer_
             ? cast_notification_producer_->HasLocalMediaRoute()
             : false;
}

base::CallbackListSubscription
MediaNotificationService::RegisterAudioOutputDeviceDescriptionsCallback(
    MediaNotificationDeviceProvider::GetOutputDevicesCallback callback) {
  return media_session_notification_producer_
      ->RegisterAudioOutputDeviceDescriptionsCallback(std::move(callback));
}

base::CallbackListSubscription
MediaNotificationService::RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
    const std::string& id,
    base::RepeatingCallback<void(bool)> callback) {
  return media_session_notification_producer_
      ->RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
          id, std::move(callback));
}

void MediaNotificationService::OnStartPresentationContextCreated(
    std::unique_ptr<media_router::StartPresentationContext> context) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(
      content::RenderFrameHost::FromID(
          context->presentation_request().render_frame_host_id));
  if (!web_contents) {
    CancelRequest(std::move(context), "The web page is closed.");
    return;
  }

  // If there exists a cast notification associated with |web_contents|,
  // delete |context| because users should not start a new presentation at
  // this time.
  if (HasCastNotificationsForWebContents(web_contents)) {
    CancelRequest(std::move(context), "A presentation has already started.");
  } else if (media_session_notification_producer_
                 ->HasActiveControllableSessionForWebContents(web_contents)) {
    // If there exists a media session notification associated with
    // |web_contents|, pass |context| to |media_session_notification_producer_|.
    media_session_notification_producer_->OnStartPresentationContextCreated(
        std::move(context));
  } else if (presentation_request_notification_producer_) {
    // If there do not exist active notifications, pass |context| to
    // |presentation_request_notification_producer_| to create a dummy
    // notification.
    presentation_request_notification_producer_
        ->OnStartPresentationContextCreated(std::move(context));
  } else {
    CancelRequest(std::move(context), "Unable to start presentation.");
  }
}

std::unique_ptr<media_router::CastDialogController>
MediaNotificationService::CreateCastDialogControllerForSession(
    const std::string& id) {
  return media_session_notification_producer_
      ->CreateCastDialogControllerForSession(id);
}

std::unique_ptr<media_router::CastDialogController>
MediaNotificationService::CreateCastDialogControllerForPresentationRequest() {
  auto* web_contents =
      presentation_request_notification_producer_->GetWebContents();
  if (!web_contents)
    return nullptr;

  auto ui = std::make_unique<media_router::MediaRouterUI>(web_contents);
  if (!presentation_request_notification_producer_->GetNotificationItem()
           ->is_default_presentation_request()) {
    ui->InitWithStartPresentationContext(
        presentation_request_notification_producer_->GetNotificationItem()
            ->PassContext());
  } else {
    ui->InitWithDefaultMediaSource();
  }
  return ui;
}

bool MediaNotificationService::HasCastNotificationsForWebContents(
    content::WebContents* web_contents) const {
  return !media_router::WebContentsPresentationManager::Get(web_contents)
              ->GetMediaRoutes()
              .empty();
}
