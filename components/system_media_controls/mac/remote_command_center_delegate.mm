// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/system_media_controls/mac/remote_command_center_delegate.h"

#include "base/time/time.h"
#include "components/system_media_controls/mac/remote_command_center_delegate_cocoa.h"
#include "components/system_media_controls/system_media_controls_observer.h"

namespace system_media_controls::internal {

RemoteCommandCenterDelegate::RemoteCommandCenterDelegate(
    SystemMediaControls* system_media_controls)
    : system_media_controls_(system_media_controls) {
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

void RemoteCommandCenterDelegate::AddObserver(
    SystemMediaControlsObserver* observer) {
  observers_.AddObserver(observer);
}

void RemoteCommandCenterDelegate::RemoveObserver(
    SystemMediaControlsObserver* observer) {
  observers_.RemoveObserver(observer);
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
  for (auto& observer : observers_)
    observer.OnNext(system_media_controls_);
}

void RemoteCommandCenterDelegate::OnPrevious() {
  DCHECK(enabled_commands_.contains(Command::kPreviousTrack));
  for (auto& observer : observers_)
    observer.OnPrevious(system_media_controls_);
}

void RemoteCommandCenterDelegate::OnPlay() {
  DCHECK(enabled_commands_.contains(Command::kPlayPause));
  for (auto& observer : observers_)
    observer.OnPlay(system_media_controls_);
}

void RemoteCommandCenterDelegate::OnPause() {
  DCHECK(enabled_commands_.contains(Command::kPlayPause));
  for (auto& observer : observers_)
    observer.OnPause(system_media_controls_);
}

void RemoteCommandCenterDelegate::OnPlayPause() {
  DCHECK(enabled_commands_.contains(Command::kPlayPause));
  for (auto& observer : observers_)
    observer.OnPlayPause(system_media_controls_);
}

void RemoteCommandCenterDelegate::OnStop() {
  DCHECK(enabled_commands_.contains(Command::kStop));
  for (auto& observer : observers_)
    observer.OnStop(system_media_controls_);
}

void RemoteCommandCenterDelegate::OnSeekTo(const base::TimeDelta& time) {
  DCHECK(enabled_commands_.contains(Command::kSeekTo));
  for (auto& observer : observers_)
    observer.OnSeekTo(system_media_controls_, time);
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

}  // namespace system_media_controls::internal
