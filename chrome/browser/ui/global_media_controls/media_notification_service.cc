// Copyright 2019 The Chromium Authors
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
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_provider_impl.h"
#include "chrome/browser/ui/media_router/media_router_ui.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/global_media_controls/public/media_dialog_delegate.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/global_media_controls/public/media_item_producer.h"
#include "components/global_media_controls/public/media_item_ui.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/media_session_service.h"
#include "media/base/media_switches.h"
#include "media/remoting/device_capability_checker.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace {

// The maximum number of actions we will record to UKM for a specific source.
constexpr int kMaxActionsRecordedToUKM = 100;

void CancelRequest(
    std::unique_ptr<media_router::StartPresentationContext> context,
    const std::string& message) {
  context->InvokeErrorCallback(blink::mojom::PresentationError(
      blink::mojom::PresentationErrorType::PRESENTATION_REQUEST_CANCELLED,
      message));
}

// Here we check to see if the WebContents is focused. Note that we can't just
// use |WebContentsObserver::OnWebContentsFocused()| and
// |WebContentsObserver::OnWebContentsLostFocus()| because focusing the
// MediaDialogView causes the WebContents to "lose focus", so we'd never be
// focused.
bool IsWebContentsFocused(content::WebContents* web_contents) {
  DCHECK(web_contents);
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return false;

  // If the given WebContents is not in the focused window, then it's not
  // focused. Note that we know a Browser is focused because otherwise the user
  // could not interact with the MediaDialogView.
  if (BrowserList::GetInstance()->GetLastActive() != browser)
    return false;

  return browser->tab_strip_model()->GetActiveWebContents() == web_contents;
}

}  // namespace

MediaNotificationService::MediaNotificationService(
    Profile* profile,
    bool show_from_all_profiles) {
  item_manager_ = global_media_controls::MediaItemManager::Create();

  absl::optional<base::UnguessableToken> source_id;
  if (!show_from_all_profiles) {
    source_id = content::MediaSession::GetSourceId(profile);
  }

  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote;
  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote;

  // Connect to receive audio focus events.
  content::GetMediaSessionService().BindAudioFocusManager(
      audio_focus_remote.BindNewPipeAndPassReceiver());

  // Connect to the controller manager so we can create media controllers for
  // media sessions.
  content::GetMediaSessionService().BindMediaControllerManager(
      controller_manager_remote.BindNewPipeAndPassReceiver());

  media_session_item_producer_ =
      std::make_unique<global_media_controls::MediaSessionItemProducer>(
          std::move(audio_focus_remote), std::move(controller_manager_remote),
          item_manager_.get(), source_id);

  media_session_item_producer_->AddObserver(this);
  item_manager_->AddItemProducer(media_session_item_producer_.get());

  if (!media_router::MediaRouterEnabled(profile)) {
    return;
  }
  // base::Unretained() is safe here because cast_notification_producer_ is
  // deleted before item_manager_.
  cast_notification_producer_ = std::make_unique<CastMediaNotificationProducer>(
      profile, item_manager_.get());
  item_manager_->AddItemProducer(cast_notification_producer_.get());

  if (media_router::GlobalMediaControlsCastStartStopEnabled(profile)) {
    presentation_request_notification_producer_ =
        std::make_unique<PresentationRequestNotificationProducer>(this);
    item_manager_->AddItemProducer(
        presentation_request_notification_producer_.get());
  }
}

MediaNotificationService::~MediaNotificationService() {
  media_session_item_producer_->RemoveObserver(this);
  item_manager_->RemoveItemProducer(media_session_item_producer_.get());
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

void MediaNotificationService::OnAudioSinkChosen(const std::string& item_id,
                                                 const std::string& sink_id) {
  media_session_item_producer_->SetAudioSinkId(item_id, sink_id);
}

base::CallbackListSubscription
MediaNotificationService::RegisterAudioOutputDeviceDescriptionsCallback(
    MediaNotificationDeviceProvider::GetOutputDevicesCallback callback) {
  if (!device_provider_)
    device_provider_ = std::make_unique<MediaNotificationDeviceProviderImpl>(
        content::CreateAudioSystemForAudioService());
  return device_provider_->RegisterOutputDeviceDescriptionsCallback(
      std::move(callback));
}

base::CallbackListSubscription
MediaNotificationService::RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
    const std::string& id,
    base::RepeatingCallback<void(bool)> callback) {
  return media_session_item_producer_
      ->RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
          id, std::move(callback));
}

bool MediaNotificationService::OnMediaRemotingRequested(
    const std::string& item_id) {
  auto item = media_session_item_producer_->GetMediaItem(item_id);
  return item ? item->RequestMediaRemoting() : false;
}

void MediaNotificationService::OnMediaSessionActionButtonPressed(
    const std::string& id,
    media_session::mojom::MediaSessionAction action) {
  auto* web_contents = content::MediaSession::GetWebContentsFromRequestId(id);
  if (!web_contents)
    return;

  base::UmaHistogramBoolean("Media.GlobalMediaControls.UserActionFocus",
                            IsWebContentsFocused(web_contents));

  ukm::UkmRecorder* recorder = ukm::UkmRecorder::Get();
  ukm::SourceId source_id =
      web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();

  if (++actions_recorded_to_ukm_[source_id] > kMaxActionsRecordedToUKM)
    return;

  ukm::builders::Media_GlobalMediaControls_ActionButtonPressed(source_id)
      .SetMediaSessionAction(static_cast<int64_t>(action))
      .Record(recorder);
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
  } else if (HasActiveControllableSessionForWebContents(contents)) {
    item_id = GetActiveControllableSessionForWebContents(contents);
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
      HasActiveControllableSessionForWebContents(web_contents);
  return HasCastNotificationsForWebContents(web_contents) || has_media_session;
}

bool MediaNotificationService::HasLocalCastNotifications() const {
  return cast_notification_producer_
             ? cast_notification_producer_->HasLocalMediaRoute()
             : false;
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
  } else if (HasActiveControllableSessionForWebContents(web_contents)) {
    // If there exists a media session notification associated with
    // |web_contents|, hold onto the context for later use.
    context_ = std::move(context);

    // When a media session item is associated with PresentationRequest, we
    // must show the origin associated with the request rather than that for
    // the top frame.
    std::string item_id =
        GetActiveControllableSessionForWebContents(web_contents);
    media_session_item_producer_->UpdateMediaItemSourceOrigin(
        item_id, context_->presentation_request().frame_origin);
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
  auto* web_contents = content::MediaSession::GetWebContentsFromRequestId(id);
  if (!web_contents)
    return nullptr;

  if (context_) {
    return media_router::MediaRouterUI::CreateWithStartPresentationContext(
        web_contents, std::move(context_));
  }

  // Initialize MediaRouterUI with Remote Playback Media Source if there is no
  // default PresentationRequest associated with `web_contents`.
  if (base::FeatureList::IsEnabled(media::kMediaRemotingWithoutFullscreen)) {
    base::WeakPtr<media_router::WebContentsPresentationManager>
        presentation_manager =
            media_router::WebContentsPresentationManager::Get(web_contents);
    if (!presentation_manager ||
        !presentation_manager->HasDefaultPresentationRequest()) {
      auto remote_playback_metadata =
          media_session_item_producer_->GetRemotePlaybackMetadataFromItem(id);
      if (remote_playback_metadata) {
        return media_router::MediaRouterUI::
            CreateWithMediaSessionRemotePlayback(
                web_contents,
                media::remoting::ParseVideoCodec(
                    remote_playback_metadata->video_codec),
                media::remoting::ParseAudioCodec(
                    remote_playback_metadata->audio_codec));
      }
    }
  }

  return media_router::MediaRouterUI::CreateWithDefaultMediaSource(
      web_contents);
}

std::unique_ptr<media_router::CastDialogController>
MediaNotificationService::CreateCastDialogControllerForPresentationRequest() {
  auto* web_contents =
      presentation_request_notification_producer_->GetWebContents();
  if (!web_contents)
    return nullptr;

  if (!presentation_request_notification_producer_->GetNotificationItem()
           ->is_default_presentation_request()) {
    return media_router::MediaRouterUI::CreateWithStartPresentationContext(
        web_contents,
        presentation_request_notification_producer_->GetNotificationItem()
            ->PassContext());
  }
  return media_router::MediaRouterUI::CreateWithDefaultMediaSource(
      web_contents);
}

void MediaNotificationService::set_device_provider_for_testing(
    std::unique_ptr<MediaNotificationDeviceProvider> device_provider) {
  device_provider_ = std::move(device_provider);
}

bool MediaNotificationService::HasCastNotificationsForWebContents(
    content::WebContents* web_contents) const {
  return !media_router::WebContentsPresentationManager::Get(web_contents)
              ->GetMediaRoutes()
              .empty();
}

bool MediaNotificationService::HasActiveControllableSessionForWebContents(
    content::WebContents* web_contents) const {
  DCHECK(web_contents);
  auto item_ids = media_session_item_producer_->GetActiveControllableItemIds();
  return base::ranges::any_of(item_ids, [web_contents](const auto& item_id) {
    return web_contents ==
           content::MediaSession::GetWebContentsFromRequestId(item_id);
  });
}

std::string
MediaNotificationService::GetActiveControllableSessionForWebContents(
    content::WebContents* web_contents) const {
  DCHECK(web_contents);
  for (const auto& item_id :
       media_session_item_producer_->GetActiveControllableItemIds()) {
    if (web_contents ==
        content::MediaSession::GetWebContentsFromRequestId(item_id)) {
      return item_id;
    }
  }
  return "";
}
