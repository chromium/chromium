// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_notification_service.h"

#include <memory>

#include "base/callback_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/unguessable_token.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/global_media_controls/cast_device_list_host.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_provider_impl.h"
#include "chrome/browser/ui/global_media_controls/presentation_request_notification_producer.h"
#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/media_router/media_router_ui.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/global_media_controls/public/media_dialog_delegate.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/global_media_controls/public/media_item_producer.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/media_session_service.h"
#include "media/base/media_switches.h"
#include "media/remoting/device_capability_checker.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/media_ui_ash.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/media_ui.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

namespace mojom {
using global_media_controls::mojom::DeviceListClient;
using global_media_controls::mojom::DeviceListHost;
}  // namespace mojom

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
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser) {
    return false;
  }
  // If the given WebContents is not in the focused window, then it's not
  // focused. Note that we know a Browser is focused because otherwise the user
  // could not interact with the MediaDialogView.
  if (BrowserList::GetInstance()->GetLastActive() != browser) {
    return false;
  }
  return browser->tab_strip_model()->GetActiveWebContents() == web_contents;
}

#if BUILDFLAG(IS_CHROMEOS)
crosapi::mojom::MediaUI* GetMediaUI() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (crosapi::CrosapiManager::IsInitialized()) {
    return crosapi::CrosapiManager::Get()->crosapi_ash()->media_ui_ash();
  }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  if (chromeos::LacrosService::Get()->IsAvailable<crosapi::mojom::MediaUI>()) {
    return chromeos::LacrosService::Get()
        ->GetRemote<crosapi::mojom::MediaUI>()
        .get();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return nullptr;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

bool ShouldInitializeWithRemotePlaybackSource(
    content::WebContents* web_contents,
    media_session::mojom::RemotePlaybackMetadataPtr remote_playback_metadata) {
  if (!base::FeatureList::IsEnabled(media::kMediaRemotingWithoutFullscreen)) {
    return false;
  }

  // Do not initialize MediaRouterUI with RemotePlayback media source when there
  // exists default presentation request.
  base::WeakPtr<media_router::WebContentsPresentationManager>
      presentation_manager =
          media_router::WebContentsPresentationManager::Get(web_contents);
  if (presentation_manager &&
      presentation_manager->HasDefaultPresentationRequest()) {
    return false;
  }

  if (!remote_playback_metadata) {
    return false;
  }

  if (media::remoting::ParseVideoCodec(remote_playback_metadata->video_codec) ==
          media::VideoCodec::kUnknown ||
      media::remoting::ParseAudioCodec(remote_playback_metadata->audio_codec) ==
          media::AudioCodec::kUnknown) {
    return false;
  }

  return true;
}
}  // namespace

MediaNotificationService::MediaNotificationService(Profile* profile,
                                                   bool show_from_all_profiles)
    : profile_(profile), receiver_(this) {
  item_manager_ = global_media_controls::MediaItemManager::Create();

  std::optional<base::UnguessableToken> source_id;
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
  // CastMediaNotificationProducer is owned by
  // CastMediaNotificationProducerKeyedService in Ash.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // base::Unretained() is safe here because `cast_notification_producer_` is
  // deleted before `item_manager_`.
  cast_notification_producer_ = std::make_unique<CastMediaNotificationProducer>(
      profile, item_manager_.get());
  item_manager_->AddItemProducer(cast_notification_producer_.get());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  if (media_router::GlobalMediaControlsCastStartStopEnabled(profile)) {
    presentation_request_notification_producer_ =
        std::make_unique<PresentationRequestNotificationProducer>(
            base::BindRepeating(
                &MediaNotificationService::HasActiveNotificationsForWebContents,
                base::Unretained(this)),
            content::MediaSession::GetSourceId(profile));
#if !BUILDFLAG(IS_CHROMEOS)
    supplemental_device_picker_producer_ =
        std::make_unique<SupplementalDevicePickerProducer>(item_manager_.get());
    item_manager_->AddItemProducer(supplemental_device_picker_producer_.get());
    // On Chrome OS, SetDevicePickerProvider() gets called by Ash via the
    // crosapi.
    SetDevicePickerProvider(supplemental_device_picker_producer_->PassRemote());
#endif  // !BUILDFLAG(IS_CHROMEOS)
  }

#if BUILDFLAG(IS_CHROMEOS)
  // On Lacros-enabled Chrome OS, MediaNotificationService instances exist on
  // both Ash and Lacros sides. The Ash-side instance manages Casting from
  // System Web Apps.
  if (GetMediaUI()) {
    GetMediaUI()->RegisterDeviceService(
        content::MediaSession::GetSourceId(profile),
        receiver_.BindNewPipeAndPassRemote());
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

#if BUILDFLAG(IS_CHROMEOS)
void MediaNotificationService::ShowDialogAsh(
    std::unique_ptr<media_router::StartPresentationContext> context) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(
      content::RenderFrameHost::FromID(
          context->presentation_request().render_frame_host_id));
  OnStartPresentationContextCreated(std::move(context));
  auto routes = media_router::WebContentsPresentationManager::Get(web_contents)
                    ->GetMediaRoutes();
  std::string item_id;
  // TODO(crbug.com/1462768): When `routes` is not empty, we'd ideally set
  // `item_id` to be the ID of a MediaRoute so that we'd only show the
  // corresponding notification item. However, MediaRoute IDs are not the same
  // between Lacros and Ash, so we resort to showing all the items by leaving
  // `item_id` empty.
  if (routes.empty()) {
    item_id = content::MediaSession::GetRequestIdFromWebContents(web_contents)
                  .ToString();
  }
  if (GetMediaUI()) {
    GetMediaUI()->ShowDevicePicker(item_id);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

MediaNotificationService::~MediaNotificationService() {
  media_session_item_producer_->RemoveObserver(this);
  item_manager_->RemoveItemProducer(media_session_item_producer_.get());
}

void MediaNotificationService::Shutdown() {
  shutdown_has_started_ = true;
  // `cast_notification_producer_`,
  // `presentation_request_notification_producer_` and `host_receivers_`
  // depend on MediaRouter, which is another keyed service. So they must be
  // destroyed here.
  if (cast_notification_producer_) {
    item_manager_->RemoveItemProducer(cast_notification_producer_.get());
  }
  cast_notification_producer_.reset();
  presentation_request_notification_producer_.reset();
  for (const auto& host : host_receivers_) {
    if (host.second) {
      host.second->Close();
    }
  }
}

void MediaNotificationService::OnAudioSinkChosen(const std::string& item_id,
                                                 const std::string& sink_id) {
  media_session_item_producer_->SetAudioSinkId(item_id, sink_id);
}

base::CallbackListSubscription
MediaNotificationService::RegisterAudioOutputDeviceDescriptionsCallback(
    MediaNotificationDeviceProvider::GetOutputDevicesCallback callback) {
  if (!device_provider_) {
    device_provider_ = std::make_unique<MediaNotificationDeviceProviderImpl>(
        content::CreateAudioSystemForAudioService());
  }
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

void MediaNotificationService::OnMediaRemotingRequested(
    const std::string& item_id) {
  auto item = media_session_item_producer_->GetMediaItem(item_id);
  if (!item) {
    return;
  }

  item->RequestMediaRemoting();
  auto* web_contents =
      content::MediaSession::GetWebContentsFromRequestId(item_id);
  if (web_contents && web_contents->GetLastCommittedURL().SchemeIsFile()) {
    feature_engagement::TrackerFactory::GetForBrowserContext(profile_)
        ->NotifyEvent("media_route_started_from_gmc");
  }
}

void MediaNotificationService::OnSinksDiscovered(const std::string& item_id) {
  auto item = media_session_item_producer_->GetMediaItem(item_id);
  auto* web_contents =
      content::MediaSession::GetWebContentsFromRequestId(item_id);

  if (web_contents) {
    should_show_cast_local_media_iph_ =
        web_contents->GetLastCommittedURL().SchemeIsFile();
  }
}

void MediaNotificationService::OnMediaSessionActionButtonPressed(
    const std::string& id,
    media_session::mojom::MediaSessionAction action) {
  auto* web_contents = content::MediaSession::GetWebContentsFromRequestId(id);
  if (!web_contents) {
    return;
  }
  base::UmaHistogramBoolean("Media.GlobalMediaControls.UserActionFocus",
                            IsWebContentsFocused(web_contents));

  ukm::UkmRecorder* recorder = ukm::UkmRecorder::Get();
  ukm::SourceId source_id =
      web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();

  if (++actions_recorded_to_ukm_[source_id] > kMaxActionsRecordedToUKM) {
    return;
  }
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
  // 1. A cast presentation session associated with `contents`.
  // 2. A local media session associated with `contents`. This media session
  // might potentially be associated with a Remote Playback route.
  // 3. A supplemental notification populated using the PresentationRequest.
  std::string item_id;

  // Find the cast presentation route associated with `contents`.
  // WebContentsPresentationManager manages all presentation routes including
  // Cast and Remote Playback presentations. For the sake of displaying media
  // routes in the GMC dialog, Cast presentation routes should be shown as Cast
  // notification items and Remote Playback presentation routes should be shown
  // as media session notification items.
  std::optional<std::string> cast_presentation_route_id;
  for (auto route : media_router::WebContentsPresentationManager::Get(contents)
                        ->GetMediaRoutes()) {
    if (route.media_source().IsCastPresentationUrl()) {
      cast_presentation_route_id = route.media_route_id();
      break;
    }
  }

  if (cast_presentation_route_id.has_value()) {
    // It is possible for a sender page to connect to two routes. For the
    // sake of the Zenith dialog, only one notification is needed.
    item_id = cast_presentation_route_id.value();
  } else if (HasActiveControllableSessionForWebContents(contents)) {
    item_id = GetActiveControllableSessionForWebContents(contents);
  } else {
    const SupplementalDevicePickerItem& supplemental_item =
        supplemental_device_picker_producer_->GetOrCreateNotificationItem(
            content::MediaSession::GetSourceId(profile_));
    item_id = supplemental_item.id();
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

  // If there exists a cast notification associated with `web_contents`, delete
  // `context` because users should not start a new presentation at this time.
  if (HasCastNotificationsForWebContents(web_contents)) {
    CancelRequest(std::move(context), "A presentation has already started.");
  } else if (HasActiveControllableSessionForWebContents(web_contents)) {
    // If there exists a media session notification and a tab mirroring session,
    // both, associated with `web_contents`, delete `context` because users
    // should not start a new presentation at this time.
    if (HasTabMirroringSessionForWebContents(web_contents)) {
      CancelRequest(std::move(context),
                    "A tab mirroring session has already started.");
      return;
    }

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

void MediaNotificationService::GetDeviceListHostForSession(
    const std::string& session_id,
    mojo::PendingReceiver<mojom::DeviceListHost> host_receiver,
    mojo::PendingRemote<mojom::DeviceListClient> client_remote) {
  std::optional<std::string> remoting_session_id;
  // `remoting_session_id` is used to construct the MediaRemotingCallback for
  // CastDeviceListHost to request Media Remoting for a MediaSession. This is
  // used for Media Remoting sessions started from the GMC dialog. However, when
  // the dialog is opened for RemotePlayback#prompt() (when `context_` is not
  // nullptr), the Remote Playback API on the blink side handles sending Media
  // Remoting request and there's no need for requesting Media Remoting from
  // MNS.
  if (context_ == nullptr) {
    remoting_session_id = session_id;
  }
  CreateCastDeviceListHost(CreateCastDialogControllerForSession(session_id),
                           std::move(host_receiver), std::move(client_remote),
                           remoting_session_id);
}

void MediaNotificationService::GetDeviceListHostForPresentation(
    mojo::PendingReceiver<mojom::DeviceListHost> host_receiver,
    mojo::PendingRemote<mojom::DeviceListClient> client_remote) {
  CreateCastDeviceListHost(CreateCastDialogControllerForPresentationRequest(),
                           std::move(host_receiver), std::move(client_remote),
                           std::nullopt);
}

void MediaNotificationService::SetDevicePickerProvider(
    mojo::PendingRemote<global_media_controls::mojom::DevicePickerProvider>
        provider_remote) {
  presentation_request_notification_producer_->BindProvider(
      std::move(provider_remote));
}

std::unique_ptr<media_router::CastDialogController>
MediaNotificationService::CreateCastDialogControllerForSession(
    const std::string& id) {
  auto* web_contents = content::MediaSession::GetWebContentsFromRequestId(id);
  if (!web_contents) {
    return nullptr;
  }

  if (context_) {
    return media_router::MediaRouterUI::CreateWithStartPresentationContext(
        web_contents, std::move(context_));
  }

  auto remote_playback_metadata =
      media_session_item_producer_->GetRemotePlaybackMetadataFromItem(id);
  if (ShouldInitializeWithRemotePlaybackSource(
          web_contents, remote_playback_metadata.Clone())) {
    return media_router::MediaRouterUI::CreateWithMediaSessionRemotePlayback(
        web_contents,
        media::remoting::ParseVideoCodec(remote_playback_metadata->video_codec),
        media::remoting::ParseAudioCodec(
            remote_playback_metadata->audio_codec));
  }

  return media_router::MediaRouterUI::CreateWithDefaultMediaSource(
      web_contents);
}

std::unique_ptr<media_router::CastDialogController>
MediaNotificationService::CreateCastDialogControllerForPresentationRequest() {
  auto* web_contents =
      presentation_request_notification_producer_->GetWebContents();
  if (!web_contents) {
    return nullptr;
  }
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

void MediaNotificationService::CreateCastDeviceListHost(
    std::unique_ptr<media_router::CastDialogController> dialog_controller,
    mojo::PendingReceiver<mojom::DeviceListHost> host_pending_receiver,
    mojo::PendingRemote<mojom::DeviceListClient> client_remote,
    std::optional<std::string> remoting_session_id) {
  if (!dialog_controller) {
    // We discard the PendingReceiver/Remote here, and if they have disconnect
    // handlers set, those get called.
    return;
  }
  auto media_remoting_callback_ =
      remoting_session_id.has_value()
          ? base::BindRepeating(
                &MediaNotificationService::OnMediaRemotingRequested,
                weak_ptr_factory_.GetWeakPtr(), remoting_session_id.value())
          : base::DoNothing();
  auto on_sinks_discovered_callback =
      remoting_session_id.has_value()
          ? base::BindRepeating(&MediaNotificationService::OnSinksDiscovered,
                                weak_ptr_factory_.GetWeakPtr(),
                                remoting_session_id.value())
          : base::DoNothing();
  auto host = std::make_unique<CastDeviceListHost>(
      std::move(dialog_controller), std::move(client_remote),
      std::move(media_remoting_callback_),
      base::BindRepeating(&global_media_controls::MediaItemManager::HideDialog,
                          item_manager_->GetWeakPtr()),
      std::move(on_sinks_discovered_callback));
  int host_id = host->id();
  mojo::SelfOwnedReceiverRef<global_media_controls::mojom::DeviceListHost>
      host_receiver = mojo::MakeSelfOwnedReceiver(
          std::move(host), std::move(host_pending_receiver));
  host_receiver->set_connection_error_handler(
      base::BindOnce(&MediaNotificationService::RemoveDeviceListHost,
                     weak_ptr_factory_.GetWeakPtr(), host_id));
  host_receivers_.emplace(host_id, std::move(host_receiver));
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

bool MediaNotificationService::HasTabMirroringSessionForWebContents(
    content::WebContents* web_contents) const {
  if (!base::FeatureList::IsEnabled(
          media_router::kFallbackToAudioTabMirroring)) {
    return false;
  }

  // Return true if there exists a tab mirroring session associated with
  // `web_contents`.
  const int item_tab_id =
      sessions::SessionTabHelper::IdForTab(web_contents).id();
  for (const auto& route :
       media_router::MediaRouterFactory::GetApiForBrowserContext(
           web_contents->GetBrowserContext())
           ->GetCurrentRoutes()) {
    media_router::MediaSource media_source = route.media_source();
    if (media_source.IsTabMirroringSource() &&
        media_source.TabId().has_value() &&
        media_source.TabId().value() == item_tab_id) {
      return true;
    }
  }
  return false;
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

void MediaNotificationService::RemoveDeviceListHost(int host_id) {
  // If shutdown has started, then we may currently be iterating through
  // `host_receivers_` so we should not erase from it. `host_receivers_` will
  // get destroyed soon anyways.
  if (!shutdown_has_started_) {
    host_receivers_.erase(host_id);
  }
}
