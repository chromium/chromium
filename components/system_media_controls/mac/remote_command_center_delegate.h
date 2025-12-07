// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_REMOTE_COMMAND_CENTER_DELEGATE_H_
#define COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_REMOTE_COMMAND_CENTER_DELEGATE_H_

#include "base/containers/flat_set.h"
#include "components/system_media_controls/mac/remote_cocoa/system_media_controls.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

@class RemoteCommandCenterDelegateCocoa;

namespace base {
class TimeDelta;
}

namespace system_media_controls {

class SystemMediaControls;
class SystemMediaControlsObserver;

namespace internal {

// Wraps an NSObject which interfaces with the MPRemoteCommandCenter.
// This class can be in-process (browser) or out-of-process (app shim). In both
// cases it communicates to the browser about events received from macOS.
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

  // Called by `remote_command_center_delegate_cocoa_` when the event happens.
  // Passes the message back to the browser process via `observer_remote_`.
  void OnNext();
  void OnPrevious();
  void OnPause();
  void OnPlayPause();
  void OnStop();
  void OnPlay();
  void OnSeekTo(const base::TimeDelta& time);

  // This rebinds `observer_remote_` to a different listener set.
  // This is used to account for duplicate dPWAs sharing a single app shim.
  void BindObserverRemote(
      mojo::PendingRemote<mojom::SystemMediaControlsObserver> remote);

  // Notifies the browser process when a SystemMediaControlsBridge is made.
  void OnBridgeCreatedForTesting();

  // Notifies the browser process when the now playing info has been cleared.
  void OnMetadataClearedForTesting();

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

  // Sends messages back to the browser process about events from macOS.
  mojo::Remote<mojom::SystemMediaControlsObserver> observer_remote_;

  RemoteCommandCenterDelegateCocoa* __strong
      remote_command_center_delegate_cocoa_;
  base::flat_set<Command> enabled_commands_;
};

}  // namespace internal
}  // namespace system_media_controls

#endif  // COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_REMOTE_COMMAND_CENTER_DELEGATE_H_
