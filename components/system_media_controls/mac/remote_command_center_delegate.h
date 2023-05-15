// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_REMOTE_COMMAND_CENTER_DELEGATE_H_
#define COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_REMOTE_COMMAND_CENTER_DELEGATE_H_

#include "base/containers/flat_set.h"
#include "base/observer_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@class RemoteCommandCenterDelegateCocoa;

namespace base {
class TimeDelta;
}

namespace system_media_controls {

class SystemMediaControlsObserver;

namespace internal {

// Wraps an NSObject which interfaces with the MPRemoteCommandCenter.
class RemoteCommandCenterDelegate {
 public:
  RemoteCommandCenterDelegate();

  RemoteCommandCenterDelegate(const RemoteCommandCenterDelegate&) = delete;
  RemoteCommandCenterDelegate& operator=(const RemoteCommandCenterDelegate&) =
      delete;

  ~RemoteCommandCenterDelegate();

  // Part of the implementation of SystemMediaControls.
  void AddObserver(SystemMediaControlsObserver* observer);
  void RemoveObserver(SystemMediaControlsObserver* observer);
  void SetIsNextEnabled(bool value);
  void SetIsPreviousEnabled(bool value);
  void SetIsPlayPauseEnabled(bool value);
  void SetIsStopEnabled(bool value);
  void SetIsSeekToEnabled(bool value);

  // Called by |remote_command_center_delegate_cocoa_| when the event happens.
  void OnNext();
  void OnPrevious();
  void OnPause();
  void OnPlayPause();
  void OnStop();
  void OnPlay();
  void OnSeekTo(const base::TimeDelta& time);

 private:
  // Used to track which commands we're already listening for.
  enum class Command {
    kStop,
    kPlayPause,
    kNextTrack,
    kPreviousTrack,
    kSeekTo,
  };

  bool ShouldSetCommandEnabled(Command command, bool will_enable);

  RemoteCommandCenterDelegateCocoa* __strong
      remote_command_center_delegate_cocoa_;
  base::ObserverList<SystemMediaControlsObserver> observers_;
  base::flat_set<Command> enabled_commands_;
};

}  // namespace internal
}  // namespace system_media_controls

#endif  // COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_REMOTE_COMMAND_CENTER_DELEGATE_H_
