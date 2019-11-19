// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/system_media_controls/mac/remote_command_center_delegate.h"

#include "components/system_media_controls/mac/remote_command_center_delegate_cocoa.h"
#include "components/system_media_controls/system_media_controls_observer.h"

namespace system_media_controls {
namespace internal {

RemoteCommandCenterDelegate::RemoteCommandCenterDelegate() {
  remote_command_center_delegate_cocoa_.reset(
      [[RemoteCommandCenterDelegateCocoa alloc] initWithDelegate:this]);
}

RemoteCommandCenterDelegate::~RemoteCommandCenterDelegate() {
  // Ensure that we unregister from all commands.
  SetIsNextEnabled(false);
  SetIsPreviousEnabled(false);
  SetIsPlayPauseEnabled(false);
  SetIsStopEnabled(false);
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

void RemoteCommandCenterDelegate::OnNext() {
  DCHECK(enabled_commands_.contains(Command::kNextTrack));
  for (auto& observer : observers_)
    observer.OnNext();
}

void RemoteCommandCenterDelegate::OnPrevious() {
  DCHECK(enabled_commands_.contains(Command::kPreviousTrack));
  for (auto& observer : observers_)
    observer.OnPrevious();
}

void RemoteCommandCenterDelegate::OnPlay() {
  DCHECK(enabled_commands_.contains(Command::kPlayPause));
  for (auto& observer : observers_)
    observer.OnPlay();
}

void RemoteCommandCenterDelegate::OnPause() {
  DCHECK(enabled_commands_.contains(Command::kPlayPause));
  for (auto& observer : observers_)
    observer.OnPause();
}

void RemoteCommandCenterDelegate::OnPlayPause() {
  DCHECK(enabled_commands_.contains(Command::kPlayPause));
  for (auto& observer : observers_)
    observer.OnPlayPause();
}

void RemoteCommandCenterDelegate::OnStop() {
  DCHECK(enabled_commands_.contains(Command::kStop));
  for (auto& observer : observers_)
    observer.OnStop();
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

}  // namespace internal
}  // namespace system_media_controls
