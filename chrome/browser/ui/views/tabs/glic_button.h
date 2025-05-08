// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_GLIC_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_GLIC_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/browser_ui/glic_button_controller_delegate.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_nudge_button.h"
#include "chrome/common/buildflags.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/menu/menu_model_adapter.h"

class PrefService;

namespace glic {

// GlicButton should leverage the look and feel of the existing
// TabSearchButton for sizing and appropriate theming.

class GlicButton : public TabStripNudgeButton,
                   public views::ContextMenuController,
                   public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(GlicButton, TabStripNudgeButton)

 public:
  explicit GlicButton(TabStripController* tab_strip_controller,
                      PressedCallback pressed_callback,
                      PressedCallback close_pressed_callback,
                      base::RepeatingClosure hovered_callback,
                      base::RepeatingClosure mouse_down_callback,
                      const gfx::VectorIcon& icon,
                      const std::u16string& tooltip);
  GlicButton(const GlicButton&) = delete;
  GlicButton& operator=(const GlicButton&) = delete;
  ~GlicButton() override;

  // TabStripNudgeButton:
  void SetIsShowingNudge(bool is_showing) override;

  void SetDropToAttachIndicator(bool indicate);

  // GetBoundsInScreen() gives a rect with some padding that extends beyond the
  // visible edges of the button. This function returns a rect without that
  // padding.
  gfx::Rect GetBoundsWithInset() const;

  // TabStripControlButton:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void StateChanged(ButtonState old_state) override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(
      View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  // views::View:
  // Note that this is an optimization for fetching zero-state suggestions so
  // that we can load the suggestions in the UI as quickly as possible.
  bool OnMousePressed(const ui::MouseEvent& event) override;

  bool IsContextMenuShowingForTest();

 private:
  // Creates the model for the context menu.
  std::unique_ptr<ui::SimpleMenuModel> CreateMenuModel();

  // Callback when the context menu closes.
  void OnMenuClosed();

  // Called every time the contextual cue is shown to make a screen reader
  // announcement.
  void AnnounceNudgeShown();

  PrefService* profile_prefs() {
    return tab_strip_controller_->GetProfile()->GetPrefs();
  }

  // The model adapter for the context menu.
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;

  // Model for the context menu.
  std::unique_ptr<ui::MenuModel> menu_model_;

  // Used to ensure the button remains highlighted while the menu is active.
  std::optional<Button::ScopedAnchorHighlight> menu_anchor_higlight_;

  // Menu runner for the context menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // Tab strip that contains this button.
  raw_ptr<TabStripController> tab_strip_controller_;

  // Callback which is invoked when the button is hovered (i.e., the user is
  // more likely to interact with it soon).
  base::RepeatingClosure hovered_callback_;

  // Callback which is invoked when there is a mouse down event on the button
  // (i.e., the user is very likely to interact with it soon).
  base::RepeatingClosure mouse_down_callback_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_GLIC_BUTTON_H_
