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
#include "chrome/browser/ui/global_media_controls/media_dialog_delegate.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_impl.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_provider_impl.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_observer.h"
#include "chrome/browser/ui/global_media_controls/media_session_notification_producer.h"
#include "chrome/browser/ui/global_media_controls/overlay_media_notification.h"
#include "chrome/browser/ui/media_router/media_router_ui.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_message_center/media_notification_util.h"
#include "components/media_message_center/media_session_notification_item.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/media_session_service.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace {

constexpr int kAutoDismissTimerInMinutesDefault = 60;  // minutes

constexpr const char kAutoDismissTimerInMinutesParamName[] = "timer_in_minutes";

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

}  // anonymous namespace

MediaNotificationService::Session::Session(
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

MediaNotificationService::Session::~Session() {
  if (presentation_manager_)
    presentation_manager_->RemoveObserver(this);

  // If we've been marked inactive, then we've already recorded inactivity as
  // the dismiss reason.
  if (is_marked_inactive_)
    return;

  RecordDismissReason(dismiss_reason_.value_or(
      GlobalMediaControlsDismissReason::kMediaSessionStopped));
}

void MediaNotificationService::Session::WebContentsDestroyed() {
  // If the WebContents is destroyed, then we should just remove the item
  // instead of freezing it.
  set_dismiss_reason(GlobalMediaControlsDismissReason::kTabClosed);
  owner_->RemoveItem(id_);
}

void MediaNotificationService::Session::MediaSessionInfoChanged(
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

void MediaNotificationService::Session::MediaSessionActionsChanged(
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

void MediaNotificationService::Session::MediaSessionPositionChanged(
    const base::Optional<media_session::MediaPosition>& position) {
  OnSessionInteractedWith();
}

void MediaNotificationService::Session::OnMediaRoutesChanged(
    const std::vector<media_router::MediaRoute>& routes) {
  // Closes the media dialog after a cast session starts.
  if (!routes.empty()) {
    owner_->HideMediaDialog();
    item_->Dismiss();
  }
}

void MediaNotificationService::Session::SetController(
    mojo::Remote<media_session::mojom::MediaController> controller) {
  if (controller.is_bound()) {
    observer_receiver_.reset();
    controller->AddObserver(observer_receiver_.BindNewPipeAndPassRemote());
    controller_ = std::move(controller);
  }
}

void MediaNotificationService::Session::set_dismiss_reason(
    GlobalMediaControlsDismissReason reason) {
  DCHECK(!dismiss_reason_.has_value());
  dismiss_reason_ = reason;
}

void MediaNotificationService::Session::OnSessionInteractedWith() {
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

void MediaNotificationService::Session::OnSessionOverlayStateChanged(
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

bool MediaNotificationService::Session::IsPlaying() const {
  return is_playing_;
}

void MediaNotificationService::Session::SetAudioSinkId(const std::string& id) {
  controller_->SetAudioSinkId(id);
}

base::CallbackListSubscription MediaNotificationService::Session::
    RegisterIsAudioDeviceSwitchingSupportedCallback(
        base::RepeatingCallback<void(bool)> callback) {
  callback.Run(is_audio_device_switching_supported_);
  return is_audio_device_switching_supported_callback_list_.Add(
      std::move(callback));
}

void MediaNotificationService::Session::SetPresentationManagerForTesting(
    base::WeakPtr<media_router::WebContentsPresentationManager>
        presentation_manager) {
  presentation_manager_ = presentation_manager;
  presentation_manager_->AddObserver(this);
}

// static
void MediaNotificationService::Session::RecordDismissReason(
    GlobalMediaControlsDismissReason reason) {
  base::UmaHistogramEnumeration("Media.GlobalMediaControls.DismissReason",
                                reason);
}

void MediaNotificationService::Session::StartInactiveTimer() {
  DCHECK(!inactive_timer_.IsRunning());

  // Using |base::Unretained()| here is okay since |this| owns
  // |inactive_timer_|.
  // If the feature is disabled, the timer will run forever, in order for the
  // rest of the code to continue running as expected.
  inactive_timer_.Start(
      FROM_HERE, GetAutoDismissTimerValue(),
      base::BindOnce(&MediaNotificationService::Session::OnInactiveTimerFired,
                     base::Unretained(this)));
}

void MediaNotificationService::Session::OnInactiveTimerFired() {
  // Overlay notifications should never be marked as inactive.
  DCHECK(!is_in_overlay_);

  // If the session has been paused and inactive for long enough, then mark it
  // as inactive.
  is_marked_inactive_ = true;
  RecordDismissReason(GlobalMediaControlsDismissReason::kInactiveTimeout);
  owner_->OnSessionBecameInactive(id_);
}

void MediaNotificationService::Session::RecordInteractionDelayAfterPause() {
  base::TimeDelta time_since_last_interaction =
      base::TimeTicks::Now() - last_interaction_time_;
  base::UmaHistogramCustomTimes(
      "Media.GlobalMediaControls.InteractionDelayAfterPause",
      time_since_last_interaction, base::TimeDelta::FromMinutes(1),
      base::TimeDelta::FromDays(1), 100);
}

void MediaNotificationService::Session::MarkActiveIfNecessary() {
  if (!is_marked_inactive_)
    return;
  is_marked_inactive_ = false;

  owner_->OnSessionBecameActive(id_);
}

MediaNotificationService::MediaNotificationService(
    Profile* profile,
    bool show_from_all_profiles) {
  media_session_notification_producer_ =
      std::make_unique<MediaSessionNotificationProducer>(
          this, profile, show_from_all_profiles);
  notification_providers_.insert(media_session_notification_producer_.get());

  if (media_router::MediaRouterEnabled(profile)) {
    if (base::FeatureList::IsEnabled(media::kGlobalMediaControlsForCast)) {
      cast_notification_provider_ =
          std::make_unique<CastMediaNotificationProvider>(
              profile, this,
              base::BindRepeating(
                  &MediaNotificationService::OnCastNotificationsChanged,
                  base::Unretained(this)));
      notification_providers_.insert(cast_notification_provider_.get());
    }
    if (media_router::GlobalMediaControlsCastStartStopEnabled()) {
      presentation_request_notification_provider_ =
          std::make_unique<PresentationRequestNotificationProvider>(this);
      notification_providers_.insert(
          presentation_request_notification_provider_.get());
    }
  }
}

MediaNotificationService::~MediaNotificationService() = default;

void MediaNotificationService::AddObserver(
    MediaNotificationServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaNotificationService::RemoveObserver(
    MediaNotificationServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MediaNotificationService::ShowNotification(const std::string& id) {
  if (media_session_notification_producer_->HasSession(id) &&
      !media_session_notification_producer_->ActivateItem(id)) {
    return;
  }
  ShowAndObserveContainer(id);
}

void MediaNotificationService::HideNotification(const std::string& id) {
  if (media_session_notification_producer_) {
    media_session_notification_producer_->HideItem(id);
  }
  OnNotificationChanged(&id);
  if (!dialog_delegate_) {
    return;
  }
  dialog_delegate_->HideMediaSession(id);
}

void MediaNotificationService::RemoveItem(const std::string& id) {
  // Copy |id| to avoid a dangling reference after the item is deleted. This
  // happens when |id| refers to a string owned by the item being removed.
  const auto id_copy{id};
  media_session_notification_producer_->RemoveItem(id);
  supplemental_notifications_.erase(id_copy);
  OnNotificationChanged(&id_copy);
}

scoped_refptr<base::SequencedTaskRunner>
MediaNotificationService::GetTaskRunner() const {
  return nullptr;
}

void MediaNotificationService::LogMediaSessionActionButtonPressed(
    const std::string& id,
    media_session::mojom::MediaSessionAction action) {
  media_session_notification_producer_->LogMediaSessionActionButtonPressed(
      id, action);
}

void MediaNotificationService::Shutdown() {
  // |cast_notification_provider_| and
  // |presentation_request_notification_provider_| depend on MediaRouter,
  // which is another keyed service.
  cast_notification_provider_.reset();
  presentation_request_notification_provider_.reset();
}

void MediaNotificationService::AddSupplementalNotification(
    const std::string& id,
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  supplemental_notifications_.emplace(id, web_contents);
  if (!HasSessionForWebContents(web_contents))
    ShowNotification(id);
}

void MediaNotificationService::OnOverlayNotificationClosed(
    const std::string& id) {
  if (!media_session_notification_producer_->OnOverlayNotificationClosed(id)) {
    return;
  }
  ShowAndObserveContainer(id);
}

void MediaNotificationService::OnCastNotificationsChanged() {
  OnNotificationChanged(nullptr);
}

void MediaNotificationService::SetDialogDelegate(
    MediaDialogDelegate* delegate) {
  DCHECK(!delegate || !dialog_delegate_);
  dialog_delegate_ = delegate;

  if (dialog_delegate_) {
    for (auto& observer : observers_)
      observer.OnMediaDialogOpened();
  } else {
    for (auto& observer : observers_)
      observer.OnMediaDialogClosed();
  }

  if (!dialog_delegate_)
    return;

  auto notification_ids = GetActiveControllableNotificationIds();
  std::list<std::string> sorted_notification_ids;
  for (const std::string& id : notification_ids) {
    if (media_session_notification_producer_->IsSessionPlaying(id)) {
      sorted_notification_ids.push_front(id);
    } else {
      sorted_notification_ids.push_back(id);
    }
  }

  for (const std::string& id : sorted_notification_ids) {
    base::WeakPtr<media_message_center::MediaNotificationItem> item =
        GetNotificationItem(id);
    MediaNotificationContainerImpl* container =
        dialog_delegate_->ShowMediaSession(id, item);
    auto* notification_producer = GetNotificationProducer(id);
    if (notification_producer)
      notification_producer->OnItemShown(id, container);
  }

  media_message_center::RecordConcurrentNotificationCount(
      notification_ids.size());

  if (cast_notification_provider_) {
    media_message_center::RecordConcurrentCastNotificationCount(
        cast_notification_provider_->GetActiveItemCount());
  }
}

std::set<std::string>
MediaNotificationService::GetActiveControllableNotificationIds() const {
  std::set<std::string> ids;
  for (auto* notification_provider : notification_providers_) {
    const std::set<std::string>& provider_ids =
        notification_provider->GetActiveControllableNotificationIds();
    ids.insert(provider_ids.begin(), provider_ids.end());
  }
  return ids;
}

bool MediaNotificationService::HasActiveNotifications() const {
  return !GetActiveControllableNotificationIds().empty();
}

bool MediaNotificationService::HasFrozenNotifications() const {
  return media_session_notification_producer_->HasFrozenNotifications();
}

bool MediaNotificationService::HasOpenDialog() const {
  return !!dialog_delegate_;
}

void MediaNotificationService::HideMediaDialog() {
  if (dialog_delegate_) {
    dialog_delegate_->HideMediaDialog();
  }
}

std::unique_ptr<OverlayMediaNotification>
MediaNotificationService::PopOutNotification(const std::string& id,
                                             gfx::Rect bounds) {
  return dialog_delegate_ ? dialog_delegate_->PopOut(id, bounds) : nullptr;
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
  if (presentation_request_notification_provider_) {
    presentation_request_notification_provider_
        ->OnStartPresentationContextCreated(std::move(context));
  }
}

std::unique_ptr<media_router::CastDialogController>
MediaNotificationService::CreateCastDialogControllerForSession(
    const std::string& id) {
  return media_session_notification_producer_
      ->CreateCastDialogControllerForSession(id);
}

void MediaNotificationService::ShowAndObserveContainer(const std::string& id) {
  OnNotificationChanged(&id);
  if (!dialog_delegate_) {
    return;
  }
  base::WeakPtr<media_message_center::MediaNotificationItem> item =
      GetNotificationItem(id);
  MediaNotificationContainerImpl* container =
      dialog_delegate_->ShowMediaSession(id, item);
  auto* notification_producer = GetNotificationProducer(id);
  if (notification_producer)
    notification_producer->OnItemShown(id, container);
}

base::WeakPtr<media_message_center::MediaNotificationItem>
MediaNotificationService::GetNotificationItem(const std::string& id) {
  for (auto* notification_provider : notification_providers_) {
    auto item = notification_provider->GetNotificationItem(id);
    if (item) {
      return item;
    }
  }
  return nullptr;
}

void MediaNotificationService::OnNotificationChanged(
    const std::string* changed_notification_id) {
  for (auto& observer : observers_)
    observer.OnNotificationListChanged();

  // Avoid re-examining the supplemental notifications as a side-effect of
  // hiding a supplemental notification.
  if (!changed_notification_id ||
      base::Contains(supplemental_notifications_, *changed_notification_id))
    return;

  // Hide supplemental notifications if necessary.
  for (const auto& pair : supplemental_notifications_) {
    // If there is an active session associated with the same web contents as
    // this supplemental notification, hide it.
    if (HasSessionForWebContents(pair.second)) {
      HideNotification(pair.first);
    }
  }
}

bool MediaNotificationService::HasSessionForWebContents(
    content::WebContents* web_contents) const {
  return media_session_notification_producer_->HasSessionForWebContents(
      web_contents);
}

MediaNotificationProducer* MediaNotificationService::GetNotificationProducer(
    const std::string& notification_id) {
  for (auto* notification_producer : notification_providers_) {
    if (notification_producer->GetNotificationItem(notification_id)) {
      return notification_producer;
    }
  }
  return nullptr;
}
