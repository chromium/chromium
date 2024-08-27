// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/system_media_controls/mac/system_media_controls_mac.h"

#include "base/check_is_test.h"
#include "components/remote_cocoa/browser/application_host.h"
#include "components/system_media_controls/mac/remote_cocoa/system_media_controls_bridge.h"
#include "components/system_media_controls/system_media_controls_observer.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {
system_media_controls::mojom::PlaybackStatus ConvertPlaybackStatus(
    system_media_controls::SystemMediaControls::PlaybackStatus status) {
  switch (status) {
    case system_media_controls::SystemMediaControls::PlaybackStatus::kPlaying:
      return system_media_controls::mojom::PlaybackStatus::kPlaying;
    case system_media_controls::SystemMediaControls::PlaybackStatus::kPaused:
      return system_media_controls::mojom::PlaybackStatus::kPaused;
    case system_media_controls::SystemMediaControls::PlaybackStatus::kStopped:
      return system_media_controls::mojom::PlaybackStatus::kStopped;
  }
}
}  // namespace

namespace system_media_controls {

// For testing only.
base::RepeatingCallback<void(bool)>*
    g_on_visibility_changed_for_testing_callback = nullptr;

// static
std::unique_ptr<SystemMediaControls> SystemMediaControls::Create(
    remote_cocoa::ApplicationHost* application_host) {
  return std::make_unique<internal::SystemMediaControlsMac>(application_host);
}
// static
void SystemMediaControls::SetVisibilityChangedCallbackForTesting(
    base::RepeatingCallback<void(bool)>* callback) {
  CHECK_IS_TEST();
  g_on_visibility_changed_for_testing_callback = callback;
}

namespace internal {

SystemMediaControlsMac::SystemMediaControlsMac(
    remote_cocoa::ApplicationHost* application_host)
    : application_host_(application_host) {
  if (application_host) {
    // ApplicationHost only has a value for PWAs. Use it to make an
    // out-of-process SystemMediaControlsBridge in the correct app shim process.
    remote_cocoa::mojom::Application* application_bridge =
        application_host->GetApplication();
    CHECK(application_bridge);

    application_bridge->CreateSystemMediaControlsBridge(
        bridge_remote_.BindNewPipeAndPassReceiver(),
        bridge_receiver_.BindNewPipeAndPassRemote());

    DCHECK(bridge_remote_.is_bound());
    DCHECK(bridge_receiver_.is_bound());
    DCHECK(bridge_remote_.is_connected());

    // Additionally, if we're making an out of process bridge, we should
    // observe `application_host` so we can get notified when the app shim
    // is going away.
    application_host->AddObserver(this);
  } else {
    // This SMC is for the browser, make an in-process bridge.
    in_proc_bridge_ = std::make_unique<SystemMediaControlsBridge>(
        bridge_remote_.BindNewPipeAndPassReceiver(),
        bridge_receiver_.BindNewPipeAndPassRemote());
  }
}

SystemMediaControlsMac::~SystemMediaControlsMac() {
  if (application_host_) {
    application_host_->RemoveObserver(this);
  }
}

void SystemMediaControlsMac::AddObserver(
    system_media_controls::SystemMediaControlsObserver* observer) {
  MaybeRebindToBridge();
  observers_.AddObserver(observer);
}

void SystemMediaControlsMac::RemoveObserver(
    system_media_controls::SystemMediaControlsObserver* observer) {
  MaybeRebindToBridge();
  observers_.RemoveObserver(observer);
}

void SystemMediaControlsMac::SetIsNextEnabled(bool value) {
  MaybeRebindToBridge();
  bridge_remote_->SetIsNextEnabled(value);
}

void SystemMediaControlsMac::SetIsPreviousEnabled(bool value) {
  MaybeRebindToBridge();
  bridge_remote_->SetIsPreviousEnabled(value);
}

void SystemMediaControlsMac::SetIsPlayPauseEnabled(bool value) {
  MaybeRebindToBridge();
  bridge_remote_->SetIsPlayPauseEnabled(value);
}

void SystemMediaControlsMac::SetIsStopEnabled(bool value) {
  MaybeRebindToBridge();
  bridge_remote_->SetIsStopEnabled(value);
}

void SystemMediaControlsMac::SetIsSeekToEnabled(bool value) {
  MaybeRebindToBridge();
  bridge_remote_->SetIsSeekToEnabled(value);
}

void SystemMediaControlsMac::SetPlaybackStatus(PlaybackStatus status) {
  MaybeRebindToBridge();
  bridge_remote_->SetPlaybackStatus(ConvertPlaybackStatus(status));
}

void SystemMediaControlsMac::SetTitle(const std::u16string& title) {
  MaybeRebindToBridge();
  bridge_remote_->SetTitle(title);
}

void SystemMediaControlsMac::SetArtist(const std::u16string& artist) {
  MaybeRebindToBridge();
  bridge_remote_->SetArtist(artist);
}

void SystemMediaControlsMac::SetAlbum(const std::u16string& album) {
  MaybeRebindToBridge();
  bridge_remote_->SetAlbum(album);
}

void SystemMediaControlsMac::SetThumbnail(const SkBitmap& bitmap) {
  MaybeRebindToBridge();
  bridge_remote_->SetThumbnail(bitmap);
}

void SystemMediaControlsMac::SetPosition(
    const media_session::MediaPosition& position) {
  MaybeRebindToBridge();
  bridge_remote_->SetPosition(position);
}

void SystemMediaControlsMac::ClearMetadata() {
  MaybeRebindToBridge();
  bridge_remote_->ClearMetadata();
}

bool SystemMediaControlsMac::GetVisibilityForTesting() const {
  NOTIMPLEMENTED();
  return false;
}

void SystemMediaControlsMac::SetOnBridgeCreatedCallbackForTesting(
    base::RepeatingCallback<void()> callback) {
  on_bridge_created_callback_for_testing_ = callback;
}

// These need to go tell the browser that Mac did something.
// These used to live in RemoteCommandCenterDelegate, but now move here since
// the observers list moved here.
void SystemMediaControlsMac::OnNext() {
  for (auto& observer : observers_) {
    observer.OnNext(this);
  }
}

void SystemMediaControlsMac::OnPrevious() {
  for (auto& observer : observers_) {
    observer.OnPrevious(this);
  }
}

void SystemMediaControlsMac::OnPause() {
  for (auto& observer : observers_) {
    observer.OnPause(this);
  }
}

void SystemMediaControlsMac::OnPlayPause() {
  for (auto& observer : observers_) {
    observer.OnPlayPause(this);
  }
}

void SystemMediaControlsMac::OnStop() {
  for (auto& observer : observers_) {
    observer.OnStop(this);
  }
}

void SystemMediaControlsMac::OnPlay() {
  for (auto& observer : observers_) {
    observer.OnPlay(this);
  }
}

void SystemMediaControlsMac::OnSeekTo(base::TimeDelta seek_time) {
  for (auto& observer : observers_) {
    observer.OnSeekTo(this, seek_time);
  }
}

void SystemMediaControlsMac::OnBridgeCreatedForTesting() {
  // The app shim has just told us that the SMCBridge was created. Update any
  // tests that are listening.
  if (on_bridge_created_callback_for_testing_) {
    std::move(on_bridge_created_callback_for_testing_).Run();
  }
}

void SystemMediaControlsMac::OnMetadataClearedForTesting() {
  if (g_on_visibility_changed_for_testing_callback) {
    // The mojo test API told us that the metadata has been cleared.
    // We are using this as a best-approximate signal that the visibility of the
    // controls has changed, so run the callback with false, as clearing
    // metadata implies that the controls should be hidden soon.
    CHECK_IS_TEST();
    g_on_visibility_changed_for_testing_callback->Run(false);
  }
}

void SystemMediaControlsMac::OnApplicationHostDestroying(
    remote_cocoa::ApplicationHost* host) {
  // If we get here, the user has Cmd+Q quit the app.
  is_app_shim_closing_ = true;
}

void SystemMediaControlsMac::MaybeRebindToBridge() {
  if (bridge_remote_ && bridge_remote_.is_connected()) {
    return;
  }

  // Don't try to rebind if we've received OnApplicationHostDestroying, as
  // ApplicationBridge will have gone away.
  if (!is_app_shim_closing_) {
    // Before we reset our connections, clear the existing metadata to ensure
    // we don't mix data between the mojo connections.
    bridge_remote_->ClearMetadata();

    bridge_remote_.reset();
    bridge_receiver_.reset();

    remote_cocoa::mojom::Application* application_bridge =
        application_host_->GetApplication();
    DCHECK(application_bridge);

    // We need to reconnect.
    application_bridge->CreateSystemMediaControlsBridge(
        bridge_remote_.BindNewPipeAndPassReceiver(),
        bridge_receiver_.BindNewPipeAndPassRemote());
  }
}

}  // namespace internal
}  // namespace system_media_controls
