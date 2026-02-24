// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_GLIC_TAB_STRIP_GLIC_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_GLIC_TAB_STRIP_GLIC_BUTTON_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/browser_ui/glic_button_controller_delegate.h"
#include "chrome/browser/ui/views/glic/glic_button_interface.h"
#include "chrome/browser/ui/views/tabs/tab_strip_nudge_button.h"
#include "chrome/common/buildflags.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/menu/menu_model_adapter.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/fre/glic_fre.mojom.h"
#endif  // BUILDFLAG(ENABLE_GLIC)

class BrowserWindowInterface;
class PrefService;
class Profile;

namespace glic {

// TabStripGlicButton should leverage the look and feel of the existing
// TabSearchButton for sizing and appropriate theming.

class TabStripGlicButton : public TabStripNudgeButton,
                           public GlicButtonInterface,
                           public views::ContextMenuController,
                           public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(TabStripGlicButton, TabStripNudgeButton)

 public:
  explicit TabStripGlicButton(
      BrowserWindowInterface* browser_window_interface,
      PressedCallback pressed_callback,
      PressedCallback close_pressed_callback,
      base::RepeatingClosure hovered_callback,
      base::RepeatingClosure mouse_down_callback,
      base::RepeatingClosure expansion_animation_done_callback,
      const std::u16string& tooltip);
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

  // These functions below work together to hide the nudge label on the static
  // button when another nudge occupies the display space.
  //
  // Suppresses the default label on the glic button with a hide animation.
  void Collapse();
  // Shows the default label on the glic button with a show animation.
  void Expand();

  void SetNudgeLabel(std::string label);
  void RestoreDefaultLabel();
  void SetGlicPanelIsOpen(bool open);

  // TabStripNudgeButton:
  void SetIsShowingNudge(bool is_showing) override;
  bool GetIsShowingNudge() const override;

  void SetDropToAttachIndicator(bool indicate);

  // GetBoundsInScreen() gives a rect with some padding that extends beyond the
  // visible edges of the button. This function returns a rect without that
  // padding.
  gfx::Rect GetBoundsWithInset() const;

  // TabStripControlButton:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void StateChanged(ButtonState old_state) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;

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

  // Sets the button back to its default colors.
  void SetDefaultColors();

  // Called when the slide animation finishes.
  void OnAnimationEnded();

  gfx::SlideAnimation* GetExpansionAnimationForTesting() override;
  bool GetLabelEnabledForTesting() const;

  // Updates the background painter to match the current border insets.
  void RefreshBackground();

  // Show or hide the split button styling, used when the task indicator is
  // present.
  void SetSplitButtonCornerStyling();
  void ResetSplitButtonCornerStyling();

  void OnBrowserWindowDidBecomeActive(BrowserWindowInterface* bwi);
  void OnBrowserWindowDidBecomeInactive(BrowserWindowInterface* bwi);
  void UpdateInkdropHoverColor(bool is_frame_active);

 private:
  // views::LabelButton:
  void SetText(std::u16string_view text) override;
  void NotifyClick(const ui::Event& event) override;

  // Creates the model for the context menu.
  std::unique_ptr<ui::SimpleMenuModel> CreateMenuModel();

  // Callback when the context menu closes.
  void OnMenuClosed();

  // Called every time the contextual cue is shown to make a screen reader
  // announcement.
  void AnnounceNudgeShown();

  PrefService* GetPrefService();

  void UpdateTextAndBackgroundColors();
  void UpdateIcon();
  bool IsHighlightVisible() const;
  void CreateIconAndLabelContainer();
  void SetCloseButtonVisible(bool visible);

  void ShowNudge();
  void HideNudge();
  void ApplyTextAndFadeIn(std::optional<std::u16string> text,
                          base::TimeDelta delay,
                          base::TimeDelta duration);
  void MaybeFadeHighlightOnHover(float final_opacity);
  int CalculateExpandedWidth();

  bool IsAnimatingTextVisibility() const;

  bool IsHidingNudge() const;

  void SetWidthState(WidthState state);

  gfx::Size PreferredSize() const;

  void SetLabelMargins();

  views::View* highlight_view() { return highlight_view_; }
  WidthState width_state() { return width_state_; }

#if BUILDFLAG(ENABLE_GLIC)
  void OnLabelVisibilityChanged();
#endif  // BUILDFLAG(ENABLE_GLIC)

  // The model adapter for the context menu.
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;

  // Model for the context menu.
  std::unique_ptr<ui::MenuModel> menu_model_;

  // Used to ensure the button remains highlighted while the menu is active.
  std::optional<Button::ScopedAnchorHighlight> menu_anchor_higlight_;

  // Menu runner for the context menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  raw_ptr<BrowserWindowInterface> browser_window_interface_;
  // Profile corresponding to the browser that this button is on.
  raw_ptr<Profile> profile_;

  // Callback which is invoked when the button is hovered (i.e., the user is
  // more likely to interact with it soon).
  base::RepeatingClosure hovered_callback_;

  // Callback which is invoked when there is a mouse down event on the button
  // (i.e., the user is very likely to interact with it soon).
  base::RepeatingClosure mouse_down_callback_;

  // Start and end values for width animations.
  int start_width_ = 0;
  int end_width_ = 0;

  // View to be drawn behind the icon and label with a background color.
  raw_ptr<View> highlight_view_ = nullptr;

  // Container view for the icon and label, and the highlight drawn behind them.
  raw_ptr<View> icon_label_highlight_view_ = nullptr;

  // Holds the incoming nudge text until the point in the animation when it can
  // be applied.
  std::optional<std::u16string> pending_text_;

  const ui::ImageModel normal_icon_;
  const ui::ImageModel icon_for_highlight_;

  bool glic_panel_is_open_ = false;

  // Width of the button when in WidthState::kNormal, set in AddedToWidget().
  int normal_width_ = 0;
  WidthState last_width_state_ = WidthState::kNormal;
  WidthState width_state_ = WidthState::kNormal;
  // Whether or not the button was collapsed before the nudge was shown.
  bool collapsed_before_nudge_shown_ = false;

  class WidthAnimationController;
  std::unique_ptr<WidthAnimationController> width_animation_controller_;

  // Window active and inactive subscriptions for changing the hover color.
  base::CallbackListSubscription window_did_become_active_subscription_;
  base::CallbackListSubscription window_did_become_inactive_subscription_;

  base::WeakPtrFactory<TabStripGlicButton> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_GLIC_TAB_STRIP_GLIC_BUTTON_H_
