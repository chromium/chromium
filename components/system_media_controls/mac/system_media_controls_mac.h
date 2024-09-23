// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_SYSTEM_MEDIA_CONTROLS_MAC_H_
#define COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_SYSTEM_MEDIA_CONTROLS_MAC_H_

#include <memory>

#include "base/observer_list.h"
#include "components/remote_cocoa/browser/application_host.h"
#include "components/remote_cocoa/common/application.mojom.h"
#include "components/system_media_controls/mac/now_playing_info_center_delegate.h"
#include "components/system_media_controls/mac/remote_cocoa/system_media_controls.mojom.h"
#include "components/system_media_controls/mac/remote_command_center_delegate.h"
#include "components/system_media_controls/system_media_controls.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media_session {
struct MediaPosition;
}  // namespace media_session
class SkBitmap;

namespace system_media_controls {
class SystemMediaControlsBridge;
class SystemMediaControlsObserver;

namespace internal {

// Interfaces with Mac OS's MPNowPlayingInfoCenter and related MediaPlayer API.
// The combination of those two form the full SystemMediaControls API.
class SystemMediaControlsMac
    : public system_media_controls::SystemMediaControls,
      public mojom::SystemMediaControlsObserver,
      public remote_cocoa::ApplicationHost::Observer {
 public:
  explicit SystemMediaControlsMac(
      remote_cocoa::ApplicationHost* application_host);
  SystemMediaControlsMac(const SystemMediaControlsMac&) = delete;
  SystemMediaControlsMac& operator=(const SystemMediaControlsMac&) = delete;
  ~SystemMediaControlsMac() override;

  // system_media_controls::SystemMediaControls implementation.
  void AddObserver(
      system_media_controls::SystemMediaControlsObserver* observer) override;
  void RemoveObserver(
      system_media_controls::SystemMediaControlsObserver* observer) override;
  void SetEnabled(bool enabled) override {}
  void SetIsNextEnabled(bool value) override;
  void SetIsPreviousEnabled(bool value) override;
  void SetIsPlayPauseEnabled(bool value) override;
  void SetIsStopEnabled(bool value) override;
  void SetIsSeekToEnabled(bool value) override;
  void SetPlaybackStatus(PlaybackStatus status) override;
  void SetTitle(const std::u16string& title) override;
  void SetArtist(const std::u16string& artist) override;
  void SetAlbum(const std::u16string& album) override;
  void SetThumbnail(const SkBitmap& bitmap) override;
  void SetPosition(const media_session::MediaPosition& position) override;
  void ClearThumbnail() override {}
  void ClearMetadata() override;
  void UpdateDisplay() override {}
  bool GetVisibilityForTesting() const override;
  void SetOnBridgeCreatedCallbackForTesting(
      base::RepeatingCallback<void()>) override;

  // system_media_controls::mojom::SystemMediaControlsObserver:
  // Uses bridge_receiver_ to receive messages from AppShim.
  // Also notifies everyone in `observers_`
  void OnNext() override;
  void OnPrevious() override;
  void OnPause() override;
  void OnPlayPause() override;
  void OnStop() override;
  void OnPlay() override;
  void OnSeekTo(base::TimeDelta seek_time) override;
  void OnBridgeCreatedForTesting() override;
  void OnMetadataClearedForTesting() override;

  // remote_cocoa::ApplicationHost::Observer:
  void OnApplicationHostDestroying(
      remote_cocoa::ApplicationHost* host) override;

 private:
  // If needed, rebinds `bridge_remote_` and `bridge_receiver_` to the correct
  // out of process SystemMediaControlsBridge. This is needed because there can
  // be multiple of the same PWA open/playing audio, and each has its own
  // SystemMediaControlsMac. However, there is only one app shim process,
  // meaning only one out of process SystemMediaControlsBridge.
  // When the user switches between the duplicated PWAs, we need to rebind
  // the SystemMediaControlsBridge to the correct SystemMediaControlsMac.
  // In the case that this function rebinds, it first makes a ClearMetadata
  // call to ensure that there's no mixing of metadata across the different
  // mojo connections.
  void MaybeRebindToBridge();

  // The Application that `this` is connected to in the app shim.
  raw_ptr<remote_cocoa::ApplicationHost> application_host_;

  // Receives updates from macOS via a SystemMediaControlsBridge.
  mojo::Receiver<mojom::SystemMediaControlsObserver> bridge_receiver_{this};

  // Sends information and updates to macOS via a SystemMediaControlsBridge.
  mojo::Remote<mojom::SystemMediaControls> bridge_remote_;

  // The above Remote/Receiver will speak to `in_proc_bridge_` for the browser's
  // system media controls connection.
  std::unique_ptr<SystemMediaControlsBridge> in_proc_bridge_;

  // People who need to know when macOS did something -
  // MediaKeysListenerManagerImpl mainly.
  base::ObserverList<system_media_controls::SystemMediaControlsObserver>
      observers_;

  // True if we've received OnApplicationHostDestroying. Prevents a crash
  // when you cmd+Q quit a PWA.
  bool is_app_shim_closing_ = false;

  // Notifies tests upon successful creation of a SystemMediaControlsBridge.
  base::RepeatingCallback<void()> on_bridge_created_callback_for_testing_;
};

}  // namespace internal
}  // namespace system_media_controls

#endif  // COMPONENTS_SYSTEM_MEDIA_CONTROLS_MAC_SYSTEM_MEDIA_CONTROLS_MAC_H_
