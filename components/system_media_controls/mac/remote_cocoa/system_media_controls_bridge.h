// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_REMOTE_COCOA_SYSTEM_MEDIA_CONTROLS_BRIDGE_H_
#define COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_REMOTE_COCOA_SYSTEM_MEDIA_CONTROLS_BRIDGE_H_

#include "base/component_export.h"
#include "components/system_media_controls/mac/now_playing_info_center_delegate.h"
#include "components/system_media_controls/mac/remote_cocoa/system_media_controls.mojom.h"
#include "components/system_media_controls/mac/remote_command_center_delegate.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media_session {
struct MediaPosition;
}  // namespace media_session

namespace system_media_controls {
// This class bridges the browser's media handling code to macOS's
// MPNowPlayingInfoCenter and MPRemoteCommandCenter. It can exist either
// in-process (browser) or out-of-process (app shim). There exists one bridge
// per app shim process. Its browser-side counterpart is SystemMediaControlsMac.
class COMPONENT_EXPORT(SYSTEM_MEDIA_CONTROLS) SystemMediaControlsBridge
    : public mojom::SystemMediaControls {
 public:
  SystemMediaControlsBridge(
      mojo::PendingReceiver<mojom::SystemMediaControls> receiver,
      mojo::PendingRemote<mojom::SystemMediaControlsObserver> remote);
  ~SystemMediaControlsBridge() override;

  // Binds (or rebinds) this bridge's mojo connections. Rebinds in the case when
  // there are 2 windows of the same PWA playing audio, since there will be 2
  // SystemMediaControlsMacs on the browser side, but only 1 app shim/bridge.
  void BindMojoConnections(
      mojo::PendingReceiver<mojom::SystemMediaControls> receiver,
      mojo::PendingRemote<mojom::SystemMediaControlsObserver> remote);

  // mojom::SystemMediaControls:
  void SetIsNextEnabled(bool enabled) override;
  void SetIsPreviousEnabled(bool enabled) override;
  void SetIsPlayPauseEnabled(bool enabled) override;
  void SetIsStopEnabled(bool enabled) override;
  void SetIsSeekToEnabled(bool enabled) override;
  void SetPlaybackStatus(mojom::PlaybackStatus status) override;
  void SetTitle(const std::u16string& title) override;
  void SetArtist(const std::u16string& artist) override;
  void SetAlbum(const std::u16string& album) override;
  void SetThumbnail(const SkBitmap& thumbnail) override;
  void SetPosition(const media_session::MediaPosition& position) override;
  void ClearMetadata() override;

 private:
  // Receives updates and metadata from the browser.
  // The corresponding remote for talking back to the browser lives in
  // RemoteCommandCenterDelegate.
  mojo::Receiver<mojom::SystemMediaControls> receiver_{this};

  // Sends media playback state and metadata to the MPNowPlayingInfoCenter.
  internal::NowPlayingInfoCenterDelegate now_playing_info_center_delegate_;

  // Receives media events (e.g. play/pause controls from the user) and sends
  // them to observers. Also keeps the system informed of which media controls
  // are currently supported.
  internal::RemoteCommandCenterDelegate remote_command_center_delegate_;
};
}  // namespace system_media_controls

#endif  // COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_REMOTE_COCOA_SYSTEM_MEDIA_CONTROLS_BRIDGE_H_
