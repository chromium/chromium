// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/media_session_item_producer.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/observer_list.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/global_media_controls/public/media_item_ui.h"
#include "components/global_media_controls/public/media_session_item_producer_observer.h"
#include "media/base/media_switches.h"

namespace global_media_controls {

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

  return base::Minutes(base::GetFieldTrialParamByFeatureAsInt(
      media::kGlobalMediaControlsAutoDismiss,
      kAutoDismissTimerInMinutesParamName, kAutoDismissTimerInMinutesDefault));
}

}  // namespace

MediaSessionItemProducer::Session::Session(
    MediaSessionItemProducer* owner,
    const std::string& id,
    std::unique_ptr<MediaSessionNotificationItem> item,
    mojo::Remote<media_session::mojom::MediaController> controller)
    : owner_(owner), id_(id), item_(std::move(item)) {
  DCHECK(owner_);
  DCHECK(item_);

  SetController(std::move(controller));
}

MediaSessionItemProducer::Session::~Session() {
  // If we've been marked inactive, then we've already recorded inactivity as
  // the dismiss reason.
  if (is_marked_inactive_)
    return;

  RecordDismissReason(dismiss_reason_.value_or(
      GlobalMediaControlsDismissReason::kMediaSessionStopped));
}

void MediaSessionItemProducer::Session::MediaSessionInfoChanged(
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

  // If the timer is already running, we don't need to do anything.
  if (inactive_timer_.IsRunning())
    return;

  last_interaction_time_ = base::TimeTicks::Now();
  StartInactiveTimer();
}

void MediaSessionItemProducer::Session::MediaSessionActionsChanged(
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

void MediaSessionItemProducer::Session::MediaSessionPositionChanged(
    const std::optional<media_session::MediaPosition>& position) {
  OnSessionInteractedWith();
}

void MediaSessionItemProducer::Session::OnRequestIdReleased() {
  // The request ID is released when the tab is closed.
  set_dismiss_reason(GlobalMediaControlsDismissReason::kTabClosed);
}

void MediaSessionItemProducer::Session::SetController(
    mojo::Remote<media_session::mojom::MediaController> controller) {
  if (controller.is_bound()) {
    observer_receiver_.reset();
    controller->AddObserver(observer_receiver_.BindNewPipeAndPassRemote());
    controller_ = std::move(controller);
  }
}

void MediaSessionItemProducer::Session::set_dismiss_reason(
    GlobalMediaControlsDismissReason reason) {
  DCHECK(!dismiss_reason_.has_value());
  dismiss_reason_ = reason;
}

void MediaSessionItemProducer::Session::OnSessionInteractedWith() {
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

bool MediaSessionItemProducer::Session::IsPlaying() const {
  // Since both MediaSessionItemProducer and MediaSessionNotificationItem
  // registered for MediaControllerObserver::MediaSessionInfoChanged(), we need
  // to check both places to get the most recent playback state in case one has
  // been updated while the other has not yet when this is called.
  return is_playing_ || item_->IsPlaying();
}

void MediaSessionItemProducer::Session::SetAudioSinkId(const std::string& id) {
  controller_->SetAudioSinkId(id);
}

base::CallbackListSubscription MediaSessionItemProducer::Session::
    RegisterIsAudioDeviceSwitchingSupportedCallback(
        base::RepeatingCallback<void(bool)> callback) {
  callback.Run(is_audio_device_switching_supported_);
  return is_audio_device_switching_supported_callback_list_.Add(
      std::move(callback));
}

// static
void MediaSessionItemProducer::Session::RecordDismissReason(
    GlobalMediaControlsDismissReason reason) {
  base::UmaHistogramEnumeration("Media.GlobalMediaControls.DismissReason",
                                reason);
}

void MediaSessionItemProducer::Session::StartInactiveTimer() {
  DCHECK(!inactive_timer_.IsRunning());

  // Using |base::Unretained()| here is okay since |this| owns
  // |inactive_timer_|.
  // If the feature is disabled, the timer will run forever, in order for the
  // rest of the code to continue running as expected.
  inactive_timer_.Start(
      FROM_HERE, GetAutoDismissTimerValue(),
      base::BindOnce(&MediaSessionItemProducer::Session::OnInactiveTimerFired,
                     base::Unretained(this)));
}

void MediaSessionItemProducer::Session::OnInactiveTimerFired() {
  // If the session has been paused and inactive for long enough, then mark it
  // as inactive.
  is_marked_inactive_ = true;
  RecordDismissReason(GlobalMediaControlsDismissReason::kInactiveTimeout);
  owner_->OnSessionBecameInactive(id_);
}

void MediaSessionItemProducer::Session::RecordInteractionDelayAfterPause() {
  base::TimeDelta time_since_last_interaction =
      base::TimeTicks::Now() - last_interaction_time_;
  base::UmaHistogramCustomTimes(
      "Media.GlobalMediaControls.InteractionDelayAfterPause",
      time_since_last_interaction, base::Minutes(1), base::Days(1), 100);
}

void MediaSessionItemProducer::Session::MarkActiveIfNecessary() {
  if (!is_marked_inactive_)
    return;
  is_marked_inactive_ = false;

  owner_->OnSessionBecameActive(id_);
}

MediaSessionItemProducer::MediaSessionItemProducer(
    mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote,
    mojo::Remote<media_session::mojom::MediaControllerManager>
        controller_manager_remote,
    MediaItemManager* item_manager,
    std::optional<base::UnguessableToken> source_id)
    : audio_focus_remote_(std::move(audio_focus_remote)),
      controller_manager_remote_(std::move(controller_manager_remote)),
      item_manager_(item_manager),
      item_ui_observer_set_(this) {
  if (source_id.has_value()) {
    audio_focus_remote_->AddSourceObserver(
        *source_id, audio_focus_observer_receiver_.BindNewPipeAndPassRemote());

    audio_focus_remote_->GetSourceFocusRequests(
        *source_id,
        base::BindOnce(&MediaSessionItemProducer::OnReceivedAudioFocusRequests,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    audio_focus_remote_->AddObserver(
        audio_focus_observer_receiver_.BindNewPipeAndPassRemote());

    audio_focus_remote_->GetFocusRequests(
        base::BindOnce(&MediaSessionItemProducer::OnReceivedAudioFocusRequests,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

MediaSessionItemProducer::~MediaSessionItemProducer() = default;

base::WeakPtr<media_message_center::MediaNotificationItem>
MediaSessionItemProducer::GetMediaItem(const std::string& id) {
  auto it = sessions_.find(id);
  return it == sessions_.end() ? nullptr : it->second.item()->GetWeakPtr();
}

std::set<std::string> MediaSessionItemProducer::GetActiveControllableItemIds()
    const {
  return active_controllable_session_ids_;
}

bool MediaSessionItemProducer::HasFrozenItems() {
  return !frozen_session_ids_.empty();
}

void MediaSessionItemProducer::OnFocusGained(
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
            std::make_unique<MediaSessionNotificationItem>(
                this, id, session->source_name.value_or(std::string()),
                session->source_id, std::move(item_controller),
                std::move(session->session_info)),
            std::move(session_controller)));
  }
}

void MediaSessionItemProducer::OnFocusLost(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  const std::string id = session->request_id->ToString();

  auto it = sessions_.find(id);
  if (it == sessions_.end())
    return;

  // If we're not currently showing this item, then we can just remove it.
  if (!base::Contains(active_controllable_session_ids_, id) &&
      !base::Contains(frozen_session_ids_, id)) {
    RemoveItem(id);
    return;
  }

  // Otherwise, freeze it in case it regains focus quickly.
  it->second.item()->Freeze(base::BindOnce(
      &MediaSessionItemProducer::OnItemUnfrozen, base::Unretained(this), id));
  active_controllable_session_ids_.erase(id);
  frozen_session_ids_.insert(id);
  item_manager_->OnItemsChanged();
}

void MediaSessionItemProducer::OnRequestIdReleased(
    const base::UnguessableToken& request_id) {
  const std::string id = request_id.ToString();
  auto it = sessions_.find(id);
  if (it == sessions_.end())
    return;

  // When the tab is closed, just remove the item instead of freezing it.
  it->second.OnRequestIdReleased();
  RemoveItem(id);
}

void MediaSessionItemProducer::OnMediaItemUIClicked(
    const std::string& id,
    bool activate_original_media) {
  auto it = sessions_.find(id);
  if (it == sessions_.end()) {
    return;
  }

  it->second.OnSessionInteractedWith();

  if (activate_original_media) {
    it->second.item()->Raise();
  }
}

void MediaSessionItemProducer::OnMediaItemUIDismissed(const std::string& id) {
  Session* session = GetSession(id);
  if (!session) {
    return;
  }

  session->set_dismiss_reason(
      GlobalMediaControlsDismissReason::kUserDismissedNotification);
  session->item()->Stop();
  session->item()->Dismiss();
}

void MediaSessionItemProducer::AddObserver(
    MediaSessionItemProducerObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaSessionItemProducer::RemoveObserver(
    MediaSessionItemProducerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MediaSessionItemProducer::OnItemShown(const std::string& id,
                                           MediaItemUI* item_ui) {
  if (item_ui)
    item_ui_observer_set_.Observe(id, item_ui);
}

bool MediaSessionItemProducer::IsItemActivelyPlaying(const std::string& id) {
  const auto it = sessions_.find(id);
  return it == sessions_.end() ? false : it->second.IsPlaying();
}

void MediaSessionItemProducer::ActivateItem(const std::string& id) {
  DCHECK(HasSession(id));
  if (base::Contains(inactive_session_ids_, id))
    return;

  active_controllable_session_ids_.insert(id);
  item_manager_->ShowItem(id);
}

void MediaSessionItemProducer::HideItem(const std::string& id) {
  active_controllable_session_ids_.erase(id);
  frozen_session_ids_.erase(id);

  item_manager_->HideItem(id);
}

void MediaSessionItemProducer::RemoveItem(const std::string& id) {
  active_controllable_session_ids_.erase(id);
  frozen_session_ids_.erase(id);
  inactive_session_ids_.erase(id);
  item_manager_->HideItem(id);
  sessions_.erase(id);
}

void MediaSessionItemProducer::RefreshItem(const std::string& id) {
  DCHECK(HasSession(id));
  if (base::Contains(inactive_session_ids_, id))
    return;

  item_manager_->RefreshItem(id);
}

bool MediaSessionItemProducer::HasSession(const std::string& id) const {
  return base::Contains(sessions_, id);
}

void MediaSessionItemProducer::LogMediaSessionActionButtonPressed(
    const std::string& id,
    media_session::mojom::MediaSessionAction action) {
  for (auto& observer : observers_)
    observer.OnMediaSessionActionButtonPressed(id, action);
}

void MediaSessionItemProducer::SetAudioSinkId(const std::string& id,
                                              const std::string& sink_id) {
  auto it = sessions_.find(id);
  CHECK(it != sessions_.end(), base::NotFatalUntil::M130);
  it->second.SetAudioSinkId(sink_id);
}

media_session::mojom::RemotePlaybackMetadataPtr
MediaSessionItemProducer::GetRemotePlaybackMetadataFromItem(
    const std::string& id) {
  auto* session = GetSession(id);
  return session ? session->item()->GetRemotePlaybackMetadata() : nullptr;
}

base::CallbackListSubscription
MediaSessionItemProducer::RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
    const std::string& id,
    base::RepeatingCallback<void(bool)> callback) {
  auto it = sessions_.find(id);
  CHECK(it != sessions_.end(), base::NotFatalUntil::M130);

  return it->second.RegisterIsAudioDeviceSwitchingSupportedCallback(
      std::move(callback));
}

void MediaSessionItemProducer::UpdateMediaItemSourceOrigin(
    const std::string& id,
    const url::Origin& origin) {
  auto it = sessions_.find(id);
  if (it != sessions_.end())
    it->second.item()->UpdatePresentationRequestOrigin(origin);
}

MediaSessionItemProducer::Session* MediaSessionItemProducer::GetSession(
    const std::string& id) {
  auto it = sessions_.find(id);
  return it == sessions_.end() ? nullptr : &it->second;
}

void MediaSessionItemProducer::OnSessionBecameActive(const std::string& id) {
  DCHECK(base::Contains(inactive_session_ids_, id));

  auto it = sessions_.find(id);
  CHECK(it != sessions_.end(), base::NotFatalUntil::M130);

  inactive_session_ids_.erase(id);

  if (it->second.item()->frozen()) {
    frozen_session_ids_.insert(id);
  } else {
    active_controllable_session_ids_.insert(id);
  }
  item_manager_->ShowItem(id);
}

void MediaSessionItemProducer::OnSessionBecameInactive(const std::string& id) {
  // If this session is already marked inactive, then there's nothing to do.
  if (base::Contains(inactive_session_ids_, id))
    return;

  inactive_session_ids_.insert(id);

  // Mark hidden on our end.
  HideItem(id);

  // Let the service know that the item is hidden.
  item_manager_->HideItem(id);
}

void MediaSessionItemProducer::OnReceivedAudioFocusRequests(
    std::vector<media_session::mojom::AudioFocusRequestStatePtr> sessions) {
  for (auto& session : sessions)
    OnFocusGained(std::move(session));
}

void MediaSessionItemProducer::OnItemUnfrozen(const std::string& id) {
  frozen_session_ids_.erase(id);

  active_controllable_session_ids_.insert(id);

  item_manager_->OnItemsChanged();
}

}  // namespace global_media_controls
