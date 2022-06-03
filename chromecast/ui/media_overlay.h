// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_UI_MEDIA_OVERLAY_H_
#define CHROMECAST_UI_MEDIA_OVERLAY_H_

#include <string>


namespace chromecast {

// MediaOverlay is responsible for displaying information to the user about
// media playback, such as the volume level, and notifications about volume
// control limitations.
class MediaOverlay {
 public:
  class Controller {
   public:
    // Notifies the controller that the media is rendering in surround-sound.
    virtual void SetSurroundSoundInUse(bool in_use) = 0;
  };

  virtual ~MediaOverlay() = default;

  virtual void SetController(Controller* controller) = 0;

  // Displays a brief toast to the user.
  virtual void ShowMessage(const std::u16string& message) = 0;

  // Shows the volume bar for a given |volume|.
  virtual void ShowVolumeBar(float volume) = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_UI_MEDIA_OVERLAY_H_
