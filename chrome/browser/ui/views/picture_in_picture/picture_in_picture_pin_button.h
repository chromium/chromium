// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_PIN_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_PIN_BUTTON_H_

#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"

static_assert(BUILDFLAG(IS_CHROMEOS));

// A toggle button to pin the Picture-in-Picture window. A pinned window stays
// where the user places it, instead of being laid out automatically (e.g.
// snapped to the edge of the screen) by the window manager. The pinned state is
// stored as `ash::PipParams::auto_layout` (`ash::kPipParamsKey`) on the native
// window of the widget that the button is added to, and the button is toggled
// while the window is pinned.
class PictureInPicturePinButton : public views::ToggleImageButton,
                                  public aura::WindowObserver {
  METADATA_HEADER(PictureInPicturePinButton, views::ToggleImageButton)

 public:
  // Returns true if the pin button should be added to PiP windows.
  static bool IsEnabled();

  PictureInPicturePinButton();
  PictureInPicturePinButton(const PictureInPicturePinButton&) = delete;
  PictureInPicturePinButton& operator=(const PictureInPicturePinButton&) =
      delete;
  ~PictureInPicturePinButton() override;

  // views::ToggleImageButton:
  ui::Cursor GetCursor(const ui::MouseEvent& event) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;

 private:
  void OnPressed();

  // Updates the toggled state from the native window.
  void UpdateToggled();

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_PIN_BUTTON_H_
