// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTKUI_NAV_BUTTON_PROVIDER_GTK_H_
#define CHROME_BROWSER_UI_LIBGTKUI_NAV_BUTTON_PROVIDER_GTK_H_

#include <map>

#include "base/component_export.h"
#include "chrome/browser/ui/frame_button_display_types.h"
#include "chrome/browser/ui/views/nav_button_provider.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/button.h"

namespace libgtkui {

class COMPONENT_EXPORT(LIBGTKUI) NavButtonProviderGtk
    : public views::NavButtonProvider {
 public:
  NavButtonProviderGtk();
  ~NavButtonProviderGtk() override;

  // views::NavButtonProvider:
  void RedrawImages(int top_area_height, bool maximized, bool active) override;
  gfx::ImageSkia GetImage(chrome::FrameButtonDisplayType type,
                          views::Button::ButtonState state) const override;
  gfx::Insets GetNavButtonMargin(
      chrome::FrameButtonDisplayType type) const override;
  gfx::Insets GetTopAreaSpacing() const override;
  int GetInterNavButtonSpacing() const override;

 private:
  std::map<chrome::FrameButtonDisplayType,
           gfx::ImageSkia[views::Button::STATE_COUNT]>
      button_images_;
  std::map<chrome::FrameButtonDisplayType, gfx::Insets> button_margins_;
  gfx::Insets top_area_spacing_;
  int inter_button_spacing_;
};

}  // namespace libgtkui

#endif  // CHROME_BROWSER_UI_LIBGTKUI_NAV_BUTTON_PROVIDER_GTK_H_
