// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_GLIC_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_GLIC_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/glic_button_controller_delegate.h"
#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "chrome/common/buildflags.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/vector_icon_types.h"

class TabStripController;

namespace glic {

// GlicButton should leverage the look and feel of the existing
// TabSearchButton for sizing and appropriate theming.
class GlicButton : public TabStripControlButton,
                   public GlicButtonControllerDelegate {
  METADATA_HEADER(GlicButton, TabStripControlButton)

 public:
  explicit GlicButton(TabStripController* tab_strip_controller,
                      PressedCallback callback,
                      const gfx::VectorIcon& icon,
                      const std::u16string& tooltip);
  GlicButton(const GlicButton&) = delete;
  GlicButton& operator=(const GlicButton&) = delete;
  ~GlicButton() override;

  // GlicButtonControllerDelegate:
  void SetShowState(bool show) override;
  void SetIcon(const gfx::VectorIcon& icon) override;

  void SetIsShowingNudge(bool is_showing);
  void SetDropToAttachIndicator(bool indicate);

  // GetBoundsInScreen() gives a rect with some padding that extends beyond the
  // visible edges of the button. This function returns a rect without that
  // padding.
  gfx::Rect GetBoundsWithInset() const;

 private:
  // Tab strip that contains this button.
  raw_ptr<TabStripController> tab_strip_controller_;

  // Represents the show state of the button. Visibility of the button
  // is reflected by the show state except when the nudge is showing.
  bool show_state_ = true;

  // Represents if a nudge is currently showing. The button is not visible
  // while the nudge is showing.
  bool is_showing_nudge_ = false;
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_GLIC_BUTTON_H_
