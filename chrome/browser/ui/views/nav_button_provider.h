// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NAV_BUTTON_PROVIDER_H_
#define CHROME_BROWSER_UI_VIEWS_NAV_BUTTON_PROVIDER_H_

#include "build/buildflag.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/features.h"

#if !BUILDFLAG(ENABLE_NATIVE_WINDOW_NAV_BUTTONS)
#error "Include not allowed."
#endif

namespace chrome {
enum class FrameButtonDisplayType;
}

namespace gfx {
class ImageSkia;
class Insets;
}  // namespace gfx

namespace views {

class NavButtonProvider {
 public:
  virtual ~NavButtonProvider() {}

  // Redraws all images and updates all size state.  |top_area_height|
  // is the total available height to render the buttons, and buttons
  // may be drawn larger when more height is available.  |active|
  // indicates if the window the buttons reside in has activation.
  virtual void RedrawImages(int top_area_height,
                            bool maximized,
                            bool active) = 0;

  // Gets the cached button image corresponding to |type| and |state|.
  virtual gfx::ImageSkia GetImage(chrome::FrameButtonDisplayType type,
                                  views::Button::ButtonState state) const = 0;

  // Gets the external margin around each button.  The left inset
  // represents the leading margin, and the right inset represents the
  // trailing margin.
  virtual gfx::Insets GetNavButtonMargin(
      chrome::FrameButtonDisplayType type) const = 0;

  // Gets the internal spacing (padding + border) of the top area.
  // The left inset represents the leading spacing, and the right
  // inset represents the trailing spacing.
  virtual gfx::Insets GetTopAreaSpacing() const = 0;

  // Gets the spacing to be used to separate buttons.
  virtual int GetInterNavButtonSpacing() const = 0;
};

}  // namespace views

#endif  // CHROME_BROWSER_UI_VIEWS_NAV_BUTTON_PROVIDER_H_
