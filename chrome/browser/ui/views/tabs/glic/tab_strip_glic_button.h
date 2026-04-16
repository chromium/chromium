// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_GLIC_TAB_STRIP_GLIC_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_GLIC_TAB_STRIP_GLIC_BUTTON_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/browser_ui/glic_button_controller_delegate.h"
#include "chrome/browser/glic/fre/glic_fre.mojom.h"
#include "chrome/browser/ui/views/glic/glic_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_nudge_button.h"
#include "chrome/common/buildflags.h"
#include "ui/base/class_property.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/menu/menu_model_adapter.h"

class BrowserWindowInterface;

namespace glic {

// TabStripGlicButton should leverage the look and feel of the existing
// TabSearchButton for sizing and appropriate theming.

class TabStripGlicButton : public GlicButton<TabStripNudgeButton>,
                           public views::ContextMenuController {
  METADATA_HEADER(TabStripGlicButton, TabStripNudgeButton)

 public:
  explicit TabStripGlicButton(
      BrowserWindowInterface* browser_window_interface,
      base::RepeatingClosure hovered_callback,
      base::RepeatingClosure mouse_down_callback,
      base::RepeatingClosure expansion_animation_done_callback,
      const std::u16string& tooltip,
      PressedCallback pressed_callback,
      PressedCallback close_pressed_callback);

  TabStripGlicButton(const TabStripGlicButton&) = delete;
  TabStripGlicButton& operator=(const TabStripGlicButton&) = delete;
  ~TabStripGlicButton() override;

  // These states represent the button's width and label contents.
  enum class WidthState {
    // Spark icon and "Gemini".
    kNormal,

    // Spark icon, contextual nudge text and "X" close button.
    kNudge,

    // Just the spark icon.
    kCollapsed
  };

  bool GetIsShowingNudge() const override;

  void SetDropToAttachIndicator(bool indicate);

  // GetBoundsInScreen() gives a rect with some padding that extends beyond the
  // visible edges of the button. This function returns a rect without that
  // padding.
  gfx::Rect GetBoundsWithInset() const;

  void ResetSplitButtonCornerStyling() override;

  void SetLabelMargins() override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(
      View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;

  // TabStripNudgeButton:
  gfx::SlideAnimation* GetExpansionAnimationForTesting() override;

 private:
  void OnLabelVisibilityChanged() override;

  float GetWidthFactor() const override;

  base::WeakPtrFactory<TabStripGlicButton> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_GLIC_TAB_STRIP_GLIC_BUTTON_H_
