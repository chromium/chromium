// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/system_media_controls/mac/remote_command_center_delegate.h"

#include "base/time/time.h"
#include "components/system_media_controls/mac/remote_command_center_delegate_cocoa.h"
#include "components/system_media_controls/system_media_controls_observer.h"

namespace system_media_controls::internal {

RemoteCommandCenterDelegate::RemoteCommandCenterDelegate() {
  remote_command_center_delegate_cocoa_ =
      [[RemoteCommandCenterDelegateCocoa alloc] initWithDelegate:this];
}

RemoteCommandCenterDelegate::~RemoteCommandCenterDelegate() {
  // Ensure that we unregister from all commands.
  SetIsNextEnabled(false);
  SetIsPreviousEnabled(false);
  SetIsPlayPauseEnabled(false);
  SetIsStopEnabled(false);
  SetIsSeekToEnabled(false);
}

void RemoteCommandCenterDelegate::SetIsNextEnabled(bool value) {
  if (!ShouldSetCommandEnabled(Command::kNextTrack, value))
    return;

  [remote_command_center_delegate_cocoa_ setCanGoNextTrack:value];
}

void RemoteCommandCenterDelegate::SetIsPreviousEnabled(bool value) {
  if (!ShouldSetCommandEnabled(Command::kPreviousTrack, value))
    return;

  [remote_command_center_delegate_cocoa_ setCanGoPreviousTrack:value];
}

void RemoteCommandCenterDelegate::SetIsPlayPauseEnabled(bool value) {
  if (!ShouldSetCommandEnabled(Command::kPlayPause, value))
    return;

  [remote_command_center_delegate_cocoa_ setCanPlay:value];
  [remote_command_center_delegate_cocoa_ setCanPause:value];
  [remote_command_center_delegate_cocoa_ setCanPlayPause:value];
}

void RemoteCommandCenterDelegate::SetIsStopEnabled(bool value) {
  if (!ShouldSetCommandEnabled(Command::kStop, value))
    return;

  [remote_command_center_delegate_cocoa_ setCanStop:value];
}

void RemoteCommandCenterDelegate::SetIsSeekToEnabled(bool value) {
  if (!ShouldSetCommandEnabled(Command::kSeekTo, value))
    return;

  [remote_command_center_delegate_cocoa_ setCanSeekTo:value];
}

void RemoteCommandCenterDelegate::OnNext() {
  DCHECK(enabled_commands_.contains(Command::kNextTrack));
  observer_remote_->OnNext();
}

void RemoteCommandCenterDelegate::OnPrevious() {
  DCHECK(enabled_commands_.contains(Command::kPreviousTrack));
  observer_remote_->OnPrevious();
}

void RemoteCommandCenterDelegate::OnPlay() {
  DCHECK(enabled_commands_.contains(Command::kPlayPause));
  observer_remote_->OnPlay();
}

void RemoteCommandCenterDelegate::OnPause() {
  DCHECK(enabled_commands_.contains(Command::kPlayPause));
  observer_remote_->OnPause();
}

void RemoteCommandCenterDelegate::OnPlayPause() {
  DCHECK(enabled_commands_.contains(Command::kPlayPause));
  observer_remote_->OnPlayPause();
}

void RemoteCommandCenterDelegate::OnStop() {
  DCHECK(enabled_commands_.contains(Command::kStop));
  observer_remote_->OnStop();
}

void RemoteCommandCenterDelegate::OnSeekTo(const base::TimeDelta& time) {
  DCHECK(enabled_commands_.contains(Command::kSeekTo));
  observer_remote_->OnSeekTo(time);
}

void RemoteCommandCenterDelegate::BindObserverRemote(
    mojo::PendingRemote<mojom::SystemMediaControlsObserver> remote) {
  if (observer_remote_.is_bound()) {
    observer_remote_.reset();
  }
  observer_remote_.Bind(std::move(remote));

  DCHECK(observer_remote_.is_bound());
  DCHECK(observer_remote_.is_connected());
}

bool RemoteCommandCenterDelegate::ShouldSetCommandEnabled(Command command,
                                                          bool will_enable) {
  if (will_enable == enabled_commands_.contains(command))
    return false;

  if (will_enable)
    enabled_commands_.insert(command);
  else
    enabled_commands_.erase(command);

  return true;
}

void RemoteCommandCenterDelegate::OnBridgeCreatedForTesting() {
  observer_remote_->OnBridgeCreatedForTesting();
}

void RemoteCommandCenterDelegate::OnMetadataClearedForTesting() {
  observer_remote_->OnMetadataClearedForTesting();
}

}  // namespace system_media_controls::internal
