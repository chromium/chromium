// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_MEDIA_CONTROLS_WIN_SYSTEM_MEDIA_CONTROLS_WIN_H_
#define COMPONENTS_SYSTEM_MEDIA_CONTROLS_WIN_SYSTEM_MEDIA_CONTROLS_WIN_H_

#include <windows.foundation.h>
#include <windows.media.control.h>
#include <wrl/client.h>

#include "base/observer_list.h"
#include "components/system_media_controls/system_media_controls.h"

namespace system_media_controls {

class SystemMediaControlsObserver;

namespace internal {

// Implementation of SystemMediaControls that connects to Windows's System Media
// Transport Controls.
class SystemMediaControlsWin : public SystemMediaControls {
 public:
  SystemMediaControlsWin(int window);

  SystemMediaControlsWin(const SystemMediaControlsWin&) = delete;
  SystemMediaControlsWin& operator=(const SystemMediaControlsWin&) = delete;

  ~SystemMediaControlsWin() override;

  // Connects to the SystemMediaTransportControls. Returns true if connection
  // is successful. If already connected, does nothing and returns true.
  bool Initialize();

  // SystemMediaControls implementation. Note that the System Media Transport
  // Controls does not support album, so we leave that as a no-op.
  void AddObserver(SystemMediaControlsObserver* observer) override;
  void RemoveObserver(SystemMediaControlsObserver* observer) override;
  void SetEnabled(bool enabled) override;
  void SetIsNextEnabled(bool value) override;
  void SetIsPreviousEnabled(bool value) override;
  void SetIsPlayPauseEnabled(bool value) override;
  void SetIsStopEnabled(bool value) override;
  void SetIsSeekToEnabled(bool value) override;
  void SetPlaybackStatus(PlaybackStatus status) override;
  void SetTitle(const std::u16string& title) override;
  void SetArtist(const std::u16string& artist) override;
  void SetAlbum(const std::u16string& album) override {}
  void SetThumbnail(const SkBitmap& bitmap) override;
  void SetPosition(const media_session::MediaPosition& position) override;
  void ClearThumbnail() override;
  void ClearMetadata() override;
  void UpdateDisplay() override;
  bool GetVisibilityForTesting() const override;

 private:
  HRESULT ButtonPressed(
      ABI::Windows::Media::ISystemMediaTransportControls* sender,
      ABI::Windows::Media::ISystemMediaTransportControlsButtonPressedEventArgs*
          args);

  HRESULT PlaybackPositionChangeRequested(
      ABI::Windows::Media::ISystemMediaTransportControls* sender,
      ABI::Windows::Media::IPlaybackPositionChangeRequestedEventArgs* args);

  // Called by ButtonPressed when the particular key is pressed.
  void OnPlay();
  void OnPause();
  void OnNext();
  void OnPrevious();
  void OnStop();

  // Called by PlaybackPositionChangeRequested.
  void OnSeekTo(const base::TimeDelta& time);

  // Converts PlaybackStatus values to SMTC-friendly values.
  ABI::Windows::Media::MediaPlaybackStatus GetSmtcPlaybackStatus(
      PlaybackStatus status);

  // Test only helper. Called from everywhere `put_IsEnabled` is called (except
  // `SetEnabled` as that's only used for timing out the controls when the
  // screen is locked)
  void OnEnabledStatusChangedForTesting();

  // Control and keep track of the metadata.
  Microsoft::WRL::ComPtr<ABI::Windows::Media::ISystemMediaTransportControls>
      system_media_controls_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::Media::ISystemMediaTransportControlsDisplayUpdater>
      display_updater_;
  Microsoft::WRL::ComPtr<ABI::Windows::Media::IMusicDisplayProperties>
      display_properties_;
  Microsoft::WRL::ComPtr<ABI::Windows::Storage::Streams::IDataWriter>
      icon_data_writer_;
  Microsoft::WRL::ComPtr<ABI::Windows::Storage::Streams::IRandomAccessStream>
      icon_stream_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::Storage::Streams::IRandomAccessStreamReference>
      icon_stream_reference_;

  EventRegistrationToken button_pressed_registration_token_;
  EventRegistrationToken playback_position_change_requested_registration_token_;

  // True if we've already tried to connect to the SystemMediaTransportControls.
  bool attempted_to_initialize_ = false;

  // True if we've successfully registered a button handler on the
  // SystemMediaTransportControls.
  bool has_valid_button_pressed_registration_token_ = false;

  // True if we've successfully registered a playback position change requested
  // handler on the SystemMediaTransportControls.
  bool has_valid_playback_position_change_requested_registration_token_ = false;

  // True if we've successfully connected to the SystemMediaTransportControls.
  bool initialized_ = false;

  // True if this instance is for controlling a web app's media session.
  const bool is_for_web_app_;

  // Web app's window handle to pass to Windows OS. Will be invalid (-1) for non
  // web apps.
  const HWND web_app_window_;

  base::ObserverList<SystemMediaControlsObserver> observers_;
  base::WeakPtrFactory<SystemMediaControlsWin> weak_factory_{this};
};

}  // namespace internal

}  // namespace system_media_controls

#endif  // COMPONENTS_SYSTEM_MEDIA_CONTROLS_WIN_SYSTEM_MEDIA_CONTROLS_WIN_H_
