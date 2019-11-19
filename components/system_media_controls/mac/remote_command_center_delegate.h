// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_REMOTE_COMMAND_CENTER_DELEGATE_H_
#define COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_REMOTE_COMMAND_CENTER_DELEGATE_H_

#include "base/containers/flat_set.h"
#include "base/mac/scoped_nsobject.h"
#include "base/observer_list.h"

@class RemoteCommandCenterDelegateCocoa;

namespace system_media_controls {

class SystemMediaControlsObserver;

namespace internal {

// Wraps an NSObject which interfaces with the MPRemoteCommandCenter.
class API_AVAILABLE(macos(10.12.2)) RemoteCommandCenterDelegate {
 public:
  RemoteCommandCenterDelegate();
  ~RemoteCommandCenterDelegate();

  // Part of the implementation of SystemMediaControls.
  void AddObserver(SystemMediaControlsObserver* observer);
  void RemoveObserver(SystemMediaControlsObserver* observer);
  void SetIsNextEnabled(bool value);
  void SetIsPreviousEnabled(bool value);
  void SetIsPlayPauseEnabled(bool value);
  void SetIsStopEnabled(bool value);

  // Called by |remote_command_center_delegate_cocoa_| when the event happens.
  void OnNext();
  void OnPrevious();
  void OnPause();
  void OnPlayPause();
  void OnStop();
  void OnPlay();

 private:
  // Used to track which commands we're already listening for.
  enum class Command {
    kStop,
    kPlayPause,
    kNextTrack,
    kPreviousTrack,
  };

  bool ShouldSetCommandEnabled(Command command, bool will_enable);

  base::scoped_nsobject<RemoteCommandCenterDelegateCocoa>
      remote_command_center_delegate_cocoa_;
  base::ObserverList<SystemMediaControlsObserver> observers_;
  base::flat_set<Command> enabled_commands_;

  DISALLOW_COPY_AND_ASSIGN(RemoteCommandCenterDelegate);
};

}  // namespace internal
}  // namespace system_media_controls

#endif  // COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_REMOTE_COMMAND_CENTER_DELEGATE_H_
