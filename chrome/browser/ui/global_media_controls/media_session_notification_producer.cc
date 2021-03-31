// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_session_notification_producer.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_impl.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_provider_impl.h"
#include "chrome/browser/ui/media_router/media_router_ui.h"
#include "components/media_message_center/media_session_notification_item.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/media_session_service.h"
#include "media/base/media_switches.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace {

// The maximum number of actions we will record to UKM for a specific source.
constexpr int kMaxActionsRecordedToUKM = 100;

constexpr int kAutoDismissTimerInMinutesDefault = 60;  // minutes

constexpr const char kAutoDismissTimerInMinutesParamName[] = "timer_in_minutes";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MediaNotificationClickSource {
  kMedia = 0,
  kPresentation,
  kMediaFling,
  kMaxValue = kMediaFling
};

// Here we check to see if the WebContents is focused. Note that since Session
// is a WebContentsObserver, we could in theory listen for
// |OnWebContentsFocused()| and |OnWebContentsLostFocus()|. However, this won't
// actually work since focusing the MediaDialogView causes the WebContents to
// "lose focus", so we'd never be focused.
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

// Returns the time value to be used for the auto-dismissing of the
// notifications after they are inactive.
// If the feature (auto-dismiss) is disabled, the returned value will be
// TimeDelta::Max() which is the largest int64 possible.
base::TimeDelta GetAutoDismissTimerValue() {
  if (!base::FeatureList::IsEnabled(media::kGlobalMediaControlsAutoDismiss))
    return base::TimeDelta::Max();

  return base::TimeDelta::FromMinutes(base::GetFieldTrialParamByFeatureAsInt(
      media::kGlobalMediaControlsAutoDismiss,
      kAutoDismissTimerInMinutesParamName, kAutoDismissTimerInMinutesDefault));
}

base::WeakPtr<media_router::WebContentsPresentationManager>
GetPresentationManager(content::WebContents* web_contents) {
  if (!web_contents ||
      !media_router::MediaRouterEnabled(web_contents->GetBrowserContext())) {
    return nullptr;
  }
  return media_router::WebContentsPresentationManager::Get(web_contents);
}

}  // namespace

MediaSessionNotificationProducer::Session::Session(
    MediaSessionNotificationProducer* owner,
    const std::string& id,
    std::unique_ptr<media_message_center::MediaSessionNotificationItem> item,
    content::WebContents* web_contents,
    mojo::Remote<media_session::mojom::MediaController> controller)
    : content::WebContentsObserver(web_contents),
      owner_(owner),
      id_(id),
      item_(std::move(item)),
      presentation_manager_(GetPresentationManager(web_contents)) {
  DCHECK(owner_);
  DCHECK(item_);

  SetController(std::move(controller));
  if (presentation_manager_)
    presentation_manager_->AddObserver(this);
}

MediaSessionNotificationProducer::Session::~Session() {
  if (presentation_manager_)
    presentation_manager_->RemoveObserver(this);

  // If we've been marked inactive, then we've already recorded inactivity as
  // the dismiss reason.
  if (is_marked_inactive_)
    return;

  RecordDismissReason(dismiss_reason_.value_or(
      GlobalMediaControlsDismissReason::kMediaSessionStopped));
}

void MediaSessionNotificationProducer::Session::WebContentsDestroyed() {
  // If the WebContents is destroyed, then we should just remove the item
  // instead of freezing it.
  set_dismiss_reason(GlobalMediaControlsDismissReason::kTabClosed);
  owner_->RemoveItem(id_);
}

void MediaSessionNotificationProducer::Session::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  is_playing_ =
      session_info && session_info->playback_state ==
                          media_session::mojom::MediaPlaybackState::kPlaying;

  // If we've started playing, we don't want the inactive timer to be running.
  if (is_playing_) {
    if (inactive_timer_.IsRunning() || is_marked_inactive_) {
      MarkActiveIfNecessary();
      RecordInteractionDelayAfterPause();
      inactive_timer_.Stop();
    }
    return;
  }

  // If we're in an overlay, then we don't want to count the session as
  // inactive.
  // TODO(https://crbug.com/1032841): This means we won't record interaction
  // delays. Consider changing to record them.
  if (is_in_overlay_)
    return;

  // If the timer is already running, we don't need to do anything.
  if (inactive_timer_.IsRunning())
    return;

  last_interaction_time_ = base::TimeTicks::Now();
  StartInactiveTimer();
}

void MediaSessionNotificationProducer::Session::MediaSessionActionsChanged(
    const std::vector<media_session::mojom::MediaSessionAction>& actions) {
  bool is_audio_device_switching_supported =
      base::ranges::find(
          actions,
          media_session::mojom::MediaSessionAction::kSwitchAudioDevice) !=
      actions.end();
  if (is_audio_device_switching_supported !=
      is_audio_device_switching_supported_) {
    is_audio_device_switching_supported_ = is_audio_device_switching_supported;
    is_audio_device_switching_supported_callback_list_.Notify(
        is_audio_device_switching_supported_);
  }
}

void MediaSessionNotificationProducer::Session::MediaSessionPositionChanged(
    const base::Optional<media_session::MediaPosition>& position) {
  OnSessionInteractedWith();
}

void MediaSessionNotificationProducer::Session::OnMediaRoutesChanged(
    const std::vector<media_router::MediaRoute>& routes) {
  // Closes the media dialog after a cast session starts.
  if (!routes.empty()) {
    owner_->HideMediaDialog();
    item_->Dismiss();
  }
}

void MediaSessionNotificationProducer::Session::SetController(
    mojo::Remote<media_session::mojom::MediaController> controller) {
  if (controller.is_bound()) {
    observer_receiver_.reset();
    controller->AddObserver(observer_receiver_.BindNewPipeAndPassRemote());
    controller_ = std::move(controller);
  }
}

void MediaSessionNotificationProducer::Session::set_dismiss_reason(
    GlobalMediaControlsDismissReason reason) {
  DCHECK(!dismiss_reason_.has_value());
  dismiss_reason_ = reason;
}

void MediaSessionNotificationProducer::Session::OnSessionInteractedWith() {
  // If we're not currently tracking inactive time, then no action is needed.
  if (!inactive_timer_.IsRunning() && !is_marked_inactive_)
    return;

  MarkActiveIfNecessary();

  RecordInteractionDelayAfterPause();
  last_interaction_time_ = base::TimeTicks::Now();

  // Otherwise, reset the timer.
  inactive_timer_.Stop();
  StartInactiveTimer();
}

void MediaSessionNotificationProducer::Session::OnSessionOverlayStateChanged(
    bool is_in_overlay) {
  is_in_overlay_ = is_in_overlay;

  if (is_in_overlay_) {
    // If we enter an overlay, then we don't want the session to be marked
    // inactive.
    if (inactive_timer_.IsRunning()) {
      RecordInteractionDelayAfterPause();
      inactive_timer_.Stop();
    }
  } else if (!is_playing_ && !inactive_timer_.IsRunning()) {
    // If we exit an overlay and the session is paused, then the session is
    // inactive.
    StartInactiveTimer();
  }
}

bool MediaSessionNotificationProducer::Session::IsPlaying() const {
  return is_playing_;
}

void MediaSessionNotificationProducer::Session::SetAudioSinkId(
    const std::string& id) {
  controller_->SetAudioSinkId(id);
}

base::CallbackListSubscription MediaSessionNotificationProducer::Session::
    RegisterIsAudioDeviceSwitchingSupportedCallback(
        base::RepeatingCallback<void(bool)> callback) {
  callback.Run(is_audio_device_switching_supported_);
  return is_audio_device_switching_supported_callback_list_.Add(
      std::move(callback));
}

void MediaSessionNotificationProducer::Session::
    SetPresentationManagerForTesting(
        base::WeakPtr<media_router::WebContentsPresentationManager>
            presentation_manager) {
  presentation_manager_ = presentation_manager;
  presentation_manager_->AddObserver(this);
}

// static
void MediaSessionNotificationProducer::Session::RecordDismissReason(
    GlobalMediaControlsDismissReason reason) {
  base::UmaHistogramEnumeration("Media.GlobalMediaControls.DismissReason",
                                reason);
}

void MediaSessionNotificationProducer::Session::StartInactiveTimer() {
  DCHECK(!inactive_timer_.IsRunning());

  // Using |base::Unretained()| here is okay since |this| owns
  // |inactive_timer_|.
  // If the feature is disabled, the timer will run forever, in order for the
  // rest of the code to continue running as expected.
  inactive_timer_.Start(
      FROM_HERE, GetAutoDismissTimerValue(),
      base::BindOnce(
          &MediaSessionNotificationProducer::Session::OnInactiveTimerFired,
          base::Unretained(this)));
}

void MediaSessionNotificationProducer::Session::OnInactiveTimerFired() {
  // Overlay notifications should never be marked as inactive.
  DCHECK(!is_in_overlay_);

  // If the session has been paused and inactive for long enough, then mark it
  // as inactive.
  is_marked_inactive_ = true;
  RecordDismissReason(GlobalMediaControlsDismissReason::kInactiveTimeout);
  owner_->OnSessionBecameInactive(id_);
}

void MediaSessionNotificationProducer::Session::
    RecordInteractionDelayAfterPause() {
  base::TimeDelta time_since_last_interaction =
      base::TimeTicks::Now() - last_interaction_time_;
  base::UmaHistogramCustomTimes(
      "Media.GlobalMediaControls.InteractionDelayAfterPause",
      time_since_last_interaction, base::TimeDelta::FromMinutes(1),
      base::TimeDelta::FromDays(1), 100);
}

void MediaSessionNotificationProducer::Session::MarkActiveIfNecessary() {
  if (!is_marked_inactive_)
    return;
  is_marked_inactive_ = false;

  owner_->OnSessionBecameActive(id_);
}

MediaSessionNotificationProducer::MediaSessionNotificationProducer(
    MediaNotificationService* service,
    Profile* profile,
    bool show_from_all_profiles)
    : service_(service),
      container_observer_set_(this),
      overlay_media_notifications_manager_(service_) {
  // Connect to the controller manager so we can create media controllers for
  // media sessions.
  content::GetMediaSessionService().BindMediaControllerManager(
      controller_manager_remote_.BindNewPipeAndPassReceiver());

  // Connect to receive audio focus events.
  content::GetMediaSessionService().BindAudioFocusManager(
      audio_focus_remote_.BindNewPipeAndPassReceiver());

  if (show_from_all_profiles) {
    audio_focus_remote_->AddObserver(
        audio_focus_observer_receiver_.BindNewPipeAndPassRemote());

    audio_focus_remote_->GetFocusRequests(base::BindOnce(
        &MediaSessionNotificationProducer::OnReceivedAudioFocusRequests,
        weak_ptr_factory_.GetWeakPtr()));
  } else {
    const base::UnguessableToken& source_id =
        content::MediaSession::GetSourceId(profile);

    audio_focus_remote_->AddSourceObserver(
        source_id, audio_focus_observer_receiver_.BindNewPipeAndPassRemote());

    audio_focus_remote_->GetSourceFocusRequests(
        source_id,
        base::BindOnce(
            &MediaSessionNotificationProducer::OnReceivedAudioFocusRequests,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

MediaSessionNotificationProducer::~MediaSessionNotificationProducer() = default;

base::WeakPtr<media_message_center::MediaNotificationItem>
MediaSessionNotificationProducer::GetNotificationItem(const std::string& id) {
  auto it = sessions_.find(id);
  return it == sessions_.end() ? nullptr : it->second.item()->GetWeakPtr();
}

std::set<std::string>
MediaSessionNotificationProducer::GetActiveControllableNotificationIds() const {
  return active_controllable_session_ids_;
}

void MediaSessionNotificationProducer::OnFocusGained(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  const std::string id = session->request_id->ToString();

  // If we have an existing unfrozen item then this is a duplicate call and
  // we should ignore it.
  auto it = sessions_.find(id);
  if (it != sessions_.end() && !it->second.item()->frozen())
    return;

  mojo::Remote<media_session::mojom::MediaController> item_controller;
  mojo::Remote<media_session::mojom::MediaController> session_controller;

  controller_manager_remote_->CreateMediaControllerForSession(
      item_controller.BindNewPipeAndPassReceiver(), *session->request_id);
  controller_manager_remote_->CreateMediaControllerForSession(
      session_controller.BindNewPipeAndPassReceiver(), *session->request_id);

  if (it != sessions_.end()) {
    // If the notification was previously frozen then we should reset the
    // controller because the mojo pipe would have been reset.
    it->second.SetController(std::move(session_controller));
    it->second.item()->SetController(std::move(item_controller),
                                     std::move(session->session_info));
  } else {
    sessions_.emplace(
        std::piecewise_construct, std::forward_as_tuple(id),
        std::forward_as_tuple(
            this, id,
            std::make_unique<
                media_message_center::MediaSessionNotificationItem>(
                service_, id, session->source_name.value_or(std::string()),
                std::move(item_controller), std::move(session->session_info)),
            content::MediaSession::GetWebContentsFromRequestId(
                *session->request_id),
            std::move(session_controller)));
  }
}

void MediaSessionNotificationProducer::OnFocusLost(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  const std::string id = session->request_id->ToString();

  auto it = sessions_.find(id);
  if (it == sessions_.end())
    return;

  // If we're not currently showing this item, then we can just remove it.
  if (!base::Contains(active_controllable_session_ids_, id) &&
      !base::Contains(frozen_session_ids_, id) &&
      !base::Contains(dragged_out_session_ids_, id)) {
    service_->RemoveItem(id);
    return;
  }

  // Otherwise, freeze it in case it regains focus quickly.
  it->second.item()->Freeze(
      base::BindOnce(&MediaSessionNotificationProducer::OnItemUnfrozen,
                     base::Unretained(this), id));
  active_controllable_session_ids_.erase(id);
  frozen_session_ids_.insert(id);
  service_->OnNotificationChanged(&id);
}

void MediaSessionNotificationProducer::OnContainerClicked(
    const std::string& id) {
  auto it = sessions_.find(id);
  if (it == sessions_.end())
    return;

  it->second.OnSessionInteractedWith();

  content::WebContents* web_contents = it->second.web_contents();
  if (!web_contents)
    return;

  content::WebContentsDelegate* delegate = web_contents->GetDelegate();
  if (!delegate)
    return;

  base::UmaHistogramEnumeration("Media.Notification.Click",
                                MediaNotificationClickSource::kMedia);

  delegate->ActivateContents(web_contents);
}

void MediaSessionNotificationProducer::OnContainerDismissed(
    const std::string& id) {
  // If the notification is dragged out, then dismissing should just close the
  // overlay notification.
  if (base::Contains(dragged_out_session_ids_, id)) {
    overlay_media_notifications_manager_.CloseOverlayNotification(id);
    return;
  }

  Session* session = GetSession(id);
  if (!session) {
    return;
  }

  session->set_dismiss_reason(
      GlobalMediaControlsDismissReason::kUserDismissedNotification);
  session->item()->Dismiss();
}

void MediaSessionNotificationProducer::OnContainerDraggedOut(
    const std::string& id,
    gfx::Rect bounds) {
  if (!HasSession(id)) {
    return;
  }
  std::unique_ptr<OverlayMediaNotification> overlay_notification =
      service_->PopOutNotification(id, bounds);

  if (!overlay_notification) {
    return;
  }

  // If the session has been destroyed, no action is needed.
  auto it = sessions_.find(id);
  DCHECK(it != sessions_.end());
  // Inform the Session that it's in an overlay so should not timeout as
  // inactive.
  it->second.OnSessionOverlayStateChanged(/*is_in_overlay=*/true);
  active_controllable_session_ids_.erase(id);
  dragged_out_session_ids_.insert(id);
  overlay_media_notifications_manager_.ShowOverlayNotification(
      id, std::move(overlay_notification));
  service_->OnNotificationChanged(&id);
}

void MediaSessionNotificationProducer::OnAudioSinkChosen(
    const std::string& id,
    const std::string& sink_id) {
  auto it = sessions_.find(id);
  DCHECK(it != sessions_.end());
  it->second.SetAudioSinkId(sink_id);
}

void MediaSessionNotificationProducer::OnItemShown(
    const std::string& id,
    MediaNotificationContainerImpl* container) {
  if (container)
    container_observer_set_.Observe(id, container);
}

void MediaSessionNotificationProducer::HideItem(const std::string& id) {
  active_controllable_session_ids_.erase(id);
  frozen_session_ids_.erase(id);

  if (base::Contains(dragged_out_session_ids_, id)) {
    overlay_media_notifications_manager_.CloseOverlayNotification(id);
    dragged_out_session_ids_.erase(id);
  }
}

void MediaSessionNotificationProducer::RemoveItem(const std::string& id) {
  active_controllable_session_ids_.erase(id);
  frozen_session_ids_.erase(id);
  inactive_session_ids_.erase(id);

  if (base::Contains(dragged_out_session_ids_, id)) {
    overlay_media_notifications_manager_.CloseOverlayNotification(id);
    dragged_out_session_ids_.erase(id);
  }
  sessions_.erase(id);
}

bool MediaSessionNotificationProducer::ActivateItem(const std::string& id) {
  DCHECK(HasSession(id));
  if (base::Contains(dragged_out_session_ids_, id) ||
      base::Contains(inactive_session_ids_, id)) {
    return false;
  }
  active_controllable_session_ids_.insert(id);
  return true;
}

bool MediaSessionNotificationProducer::HasSession(const std::string& id) const {
  return base::Contains(sessions_, id);
}
bool MediaSessionNotificationProducer::IsSessionPlaying(
    const std::string& id) const {
  const auto it = sessions_.find(id);
  return it == sessions_.end() ? false : it->second.IsPlaying();
}

bool MediaSessionNotificationProducer::OnOverlayNotificationClosed(
    const std::string& id) {
  // If the session has been destroyed, no action is needed.
  auto it = sessions_.find(id);
  if (it == sessions_.end())
    return false;

  it->second.OnSessionOverlayStateChanged(/*is_in_overlay=*/false);

  // Otherwise, if it's a non-frozen item, then it's now an active one.
  if (!base::Contains(frozen_session_ids_, id))
    active_controllable_session_ids_.insert(id);
  dragged_out_session_ids_.erase(id);

  // Since the overlay is closing, we no longer need to observe the associated
  // container.
  container_observer_set_.StopObserving(id);
  return true;
}

bool MediaSessionNotificationProducer::HasFrozenNotifications() const {
  return !frozen_session_ids_.empty();
}

std::unique_ptr<media_router::CastDialogController>
MediaSessionNotificationProducer::CreateCastDialogControllerForSession(
    const std::string& session_id) {
  auto it = sessions_.find(session_id);
  if (it != sessions_.end()) {
    auto ui = std::make_unique<media_router::MediaRouterUI>(
        it->second.web_contents());
    ui->InitWithDefaultMediaSource();
    return ui;
  }
  return nullptr;
}

bool MediaSessionNotificationProducer::
    HasActiveControllableSessionForWebContents(
        content::WebContents* web_contents) const {
  DCHECK(web_contents);
  return std::any_of(sessions_.begin(), sessions_.end(),
                     [web_contents, this](const auto& pair) {
                       return pair.second.web_contents() == web_contents &&
                              base::Contains(active_controllable_session_ids_,
                                             pair.first);
                     });
}

std::string
MediaSessionNotificationProducer::GetActiveControllableSessionForWebContents(
    content::WebContents* web_contents) const {
  DCHECK(web_contents);
  for (auto& session : sessions_) {
    if (session.second.web_contents() == web_contents &&
        base::Contains(active_controllable_session_ids_, session.first)) {
      return session.first;
    }
  }
  return "";
}

void MediaSessionNotificationProducer::LogMediaSessionActionButtonPressed(
    const std::string& id,
    media_session::mojom::MediaSessionAction action) {
  auto it = sessions_.find(id);
  if (it == sessions_.end())
    return;

  content::WebContents* web_contents = it->second.web_contents();
  if (!web_contents)
    return;

  base::UmaHistogramBoolean("Media.GlobalMediaControls.UserActionFocus",
                            IsWebContentsFocused(web_contents));

  ukm::UkmRecorder* recorder = ukm::UkmRecorder::Get();
  ukm::SourceId source_id =
      ukm::GetSourceIdForWebContentsDocument(web_contents);

  if (++actions_recorded_to_ukm_[source_id] > kMaxActionsRecordedToUKM)
    return;

  ukm::builders::Media_GlobalMediaControls_ActionButtonPressed(source_id)
      .SetMediaSessionAction(static_cast<int64_t>(action))
      .Record(recorder);
}

base::CallbackListSubscription
MediaSessionNotificationProducer::RegisterAudioOutputDeviceDescriptionsCallback(
    MediaNotificationDeviceProvider::GetOutputDevicesCallback callback) {
  if (!device_provider_)
    device_provider_ = std::make_unique<MediaNotificationDeviceProviderImpl>(
        content::CreateAudioSystemForAudioService());
  return device_provider_->RegisterOutputDeviceDescriptionsCallback(
      std::move(callback));
}

base::CallbackListSubscription MediaSessionNotificationProducer::
    RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
        const std::string& id,
        base::RepeatingCallback<void(bool)> callback) {
  auto it = sessions_.find(id);
  DCHECK(it != sessions_.end());

  return it->second.RegisterIsAudioDeviceSwitchingSupportedCallback(
      std::move(callback));
}

void MediaSessionNotificationProducer::set_device_provider_for_testing(
    std::unique_ptr<MediaNotificationDeviceProvider> device_provider) {
  device_provider_ = std::move(device_provider);
}

MediaSessionNotificationProducer::Session*
MediaSessionNotificationProducer::GetSession(const std::string& id) {
  auto it = sessions_.find(id);
  return it == sessions_.end() ? nullptr : &it->second;
}

void MediaSessionNotificationProducer::OnSessionBecameActive(
    const std::string& id) {
  DCHECK(base::Contains(inactive_session_ids_, id));

  auto it = sessions_.find(id);
  DCHECK(it != sessions_.end());

  inactive_session_ids_.erase(id);

  if (it->second.item()->frozen()) {
    frozen_session_ids_.insert(id);
  } else {
    active_controllable_session_ids_.insert(id);
  }
  service_->ShowAndObserveContainer(id);
}

void MediaSessionNotificationProducer::OnSessionBecameInactive(
    const std::string& id) {
  // If this session is already marked inactive, then there's nothing to do.
  if (base::Contains(inactive_session_ids_, id))
    return;

  inactive_session_ids_.insert(id);

  service_->HideNotification(id);
}

void MediaSessionNotificationProducer::HideMediaDialog() {
  service_->HideMediaDialog();
}

void MediaSessionNotificationProducer::OnReceivedAudioFocusRequests(
    std::vector<media_session::mojom::AudioFocusRequestStatePtr> sessions) {
  for (auto& session : sessions)
    OnFocusGained(std::move(session));
}

void MediaSessionNotificationProducer::OnItemUnfrozen(const std::string& id) {
  frozen_session_ids_.erase(id);

  if (!base::Contains(dragged_out_session_ids_, id))
    active_controllable_session_ids_.insert(id);

  service_->OnNotificationChanged(&id);
}
