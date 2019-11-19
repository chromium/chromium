// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_MEDIA_CONTROLS_SYSTEM_MEDIA_CONTROLS_H_
#define COMPONENTS_SYSTEM_MEDIA_CONTROLS_SYSTEM_MEDIA_CONTROLS_H_

#include "base/component_export.h"
#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkBitmap.h"

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

  // Returns the singleton instance, creating if necessary.
  static SystemMediaControls* GetInstance();

  virtual void AddObserver(SystemMediaControlsObserver* observer) = 0;
  virtual void RemoveObserver(SystemMediaControlsObserver* observer) = 0;

  // Enable or disable the service.
  virtual void SetEnabled(bool enabled) = 0;

  // Enable or disable specific controls.
  virtual void SetIsNextEnabled(bool value) = 0;
  virtual void SetIsPreviousEnabled(bool value) = 0;
  virtual void SetIsPlayPauseEnabled(bool value) = 0;
  virtual void SetIsStopEnabled(bool value) = 0;

  // Setters for metadata.
  virtual void SetPlaybackStatus(PlaybackStatus value) = 0;
  virtual void SetTitle(const base::string16& value) = 0;
  virtual void SetArtist(const base::string16& value) = 0;
  virtual void SetAlbum(const base::string16& value) = 0;
  virtual void SetThumbnail(const SkBitmap& bitmap) = 0;

  // Helpers for metadata
  virtual void ClearThumbnail() = 0;
  virtual void ClearMetadata() = 0;
  virtual void UpdateDisplay() = 0;

 protected:
  virtual ~SystemMediaControls() = default;
};

}  // namespace system_media_controls

#endif  // COMPONENTS_SYSTEM_MEDIA_CONTROLS_SYSTEM_MEDIA_CONTROLS_H_
