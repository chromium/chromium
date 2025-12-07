// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_MEDIA_CONTROLS_SYSTEM_MEDIA_CONTROLS_H_
#define COMPONENTS_SYSTEM_MEDIA_CONTROLS_SYSTEM_MEDIA_CONTROLS_H_

#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "services/media_session/public/cpp/media_position.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace remote_cocoa {
class ApplicationHost;
}

namespace system_media_controls {

class SystemMediaControlsObserver;

// OSes often provide a system-level media controls API (e.g. Media Player
// Remote Interfacing Specification on Linux or System Media Transport Controls
// on Windows). These tend to have similar APIs to each other.
// SystemMediaControls is a platform-agnostic API that clients can use to
// interface with the operating system's media controls API if it exists.
class COMPONENT_EXPORT(SYSTEM_MEDIA_CONTROLS) SystemMediaControls {
 public:
  enum class PlaybackStatus {
    kPlaying,
    kPaused,
    kStopped,
  };

#if BUILDFLAG(IS_MAC)
  static std::unique_ptr<SystemMediaControls> Create(
      remote_cocoa::ApplicationHost* application_host);
#else
  // `window` used by Windows OS for web app (dPWA) connections.
  static std::unique_ptr<SystemMediaControls> Create(
      const std::string& product_name,
      int window = -1);
#endif  // BUILDFLAG(IS_MAC)

  virtual ~SystemMediaControls() = default;

  virtual void AddObserver(SystemMediaControlsObserver* observer) = 0;
  virtual void RemoveObserver(SystemMediaControlsObserver* observer) = 0;

  // Enable or disable the service.
  virtual void SetEnabled(bool enabled) = 0;

  // Enable or disable specific controls.
  virtual void SetIsNextEnabled(bool value) = 0;
  virtual void SetIsPreviousEnabled(bool value) = 0;
  virtual void SetIsPlayPauseEnabled(bool value) = 0;
  virtual void SetIsStopEnabled(bool value) = 0;
  virtual void SetIsSeekToEnabled(bool value) {}

  // Setters for metadata.
  virtual void SetPlaybackStatus(PlaybackStatus value) = 0;
  virtual void SetID(const std::string* value) {}
  virtual void SetTitle(const std::u16string& value) = 0;
  virtual void SetArtist(const std::u16string& value) = 0;
  virtual void SetAlbum(const std::u16string& value) = 0;
  virtual void SetThumbnail(const SkBitmap& bitmap) = 0;
  virtual void SetPosition(const media_session::MediaPosition& position) {}

  // Helpers for metadata
  virtual void ClearThumbnail() = 0;
  virtual void ClearMetadata() = 0;
  virtual void UpdateDisplay() = 0;

  // Helpers for testing only.
  static void SetVisibilityChangedCallbackForTesting(
      base::RepeatingCallback<void(bool)>*);
  virtual bool GetVisibilityForTesting() const = 0;
  virtual void SetOnBridgeCreatedCallbackForTesting(
      base::RepeatingCallback<void()>) {}
};

}  // namespace system_media_controls

#endif  // COMPONENTS_SYSTEM_MEDIA_CONTROLS_SYSTEM_MEDIA_CONTROLS_H_
