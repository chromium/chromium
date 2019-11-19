// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_MEDIA_CONTROLS_WIN_SYSTEM_MEDIA_CONTROLS_WIN_H_
#define COMPONENTS_SYSTEM_MEDIA_CONTROLS_WIN_SYSTEM_MEDIA_CONTROLS_WIN_H_

#include <windows.foundation.h>
#include <windows.media.control.h>
#include <wrl/client.h>

#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "components/system_media_controls/system_media_controls.h"

namespace system_media_controls {

class SystemMediaControlsObserver;

namespace internal {

// Implementation of SystemMediaControls that connects to Windows's System Media
// Transport Controls.
class SystemMediaControlsWin : public SystemMediaControls {
 public:
  SystemMediaControlsWin();
  ~SystemMediaControlsWin() override;

  static SystemMediaControlsWin* GetInstance();

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
  void SetPlaybackStatus(PlaybackStatus status) override;
  void SetTitle(const base::string16& title) override;
  void SetArtist(const base::string16& artist) override;
  void SetAlbum(const base::string16& album) override {}
  void SetThumbnail(const SkBitmap& bitmap) override;
  void ClearThumbnail() override;
  void ClearMetadata() override;
  void UpdateDisplay() override;

 private:
  friend struct base::DefaultSingletonTraits<SystemMediaControlsWin>;

  static HRESULT ButtonPressed(
      ABI::Windows::Media::ISystemMediaTransportControls* sender,
      ABI::Windows::Media::ISystemMediaTransportControlsButtonPressedEventArgs*
          args);

  // Called by ButtonPressed when the particular key is pressed.
  void OnPlay();
  void OnPause();
  void OnNext();
  void OnPrevious();
  void OnStop();

  // Converts PlaybackStatus values to SMTC-friendly values.
  ABI::Windows::Media::MediaPlaybackStatus GetSmtcPlaybackStatus(
      PlaybackStatus status);

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

  EventRegistrationToken registration_token_;

  // True if we've already tried to connect to the SystemMediaTransportControls.
  bool attempted_to_initialize_ = false;

  // True if we've successfully registered a button handler on the
  // SystemMediaTransportControls.
  bool has_valid_registration_token_ = false;

  // True if we've successfully connected to the SystemMediaTransportControls.
  bool initialized_ = false;

  base::ObserverList<SystemMediaControlsObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(SystemMediaControlsWin);
};

}  // namespace internal

}  // namespace system_media_controls

#endif  // COMPONENTS_SYSTEM_MEDIA_CONTROLS_WIN_SYSTEM_MEDIA_CONTROLS_WIN_H_
