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

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/fre/glic_fre.mojom.h"
#endif  // BUILDFLAG(ENABLE_GLIC)

class BrowserWindowInterface;
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
                      base::RepeatingClosure expansion_animation_done_callback,
                      const std::u16string& tooltip);
  GlicButton(const GlicButton&) = delete;
  GlicButton& operator=(const GlicButton&) = delete;
  ~GlicButton() override;

  static GlicButton* FromBrowser(BrowserWindowInterface* browser);

  // These functions below work together to hide the nudge label on the static
  // button when another nudge occupies the display space.
  // TODO(crbug.com/460400955): This is a temporary fix for 143, ideally a
  // third state should be added where the label is hidden for m144 and
  // these functions should be removed.
  //
  // Suppresses the default label on the glic button with a hide animation.
  void SuppressLabel();
  // Shows the default label on the glic button with a show animation.
  void ShowDefaultLabel();

  void SetNudgeLabel(std::string label);
  void RestoreDefaultLabel();
  void SetGlicPanelIsOpen(bool open);

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
  void AddedToWidget() override;

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

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  bool IsContextMenuShowingForTest();

  // Sets the button back to its default colors.
  void SetDefaultColors();

  // Called when the slide animation finishes.
  void OnAnimationEnded();

  gfx::SlideAnimation* GetExpansionAnimationForTesting() override;
  bool GetLabelEnabledForTesting() const;

  // Updates the background painter to match the current border insets.
  void RefreshBackground();

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

  PrefService* profile_prefs() {
    return tab_strip_controller_->GetProfile()->GetPrefs();
  }

  void UpdateTextAndBackgroundColors();
  void UpdateIcon();
  bool IsHighlightVisible() const;
  void CreateIconAndLabelContainer();
  void SetCloseButtonVisible(bool visible);

  void StartShowAnimation();
  void StartHideAnimation();
  void ApplyTextAndFadeIn(std::optional<std::u16string> text,
                          base::TimeDelta delay,
                          base::TimeDelta duration);
  void MaybeFadeHighlightOnHover(float final_opacity);
  void StartExpansionAnimations(bool show,
                                base::TimeDelta overall_duration,
                                base::TimeDelta close_button_fade_start,
                                base::TimeDelta close_button_fade_duration);
  void StartSlidingTextAnimation(bool show);
  int CalculateExpandedWidth();

#if BUILDFLAG(ENABLE_GLIC)
  void PanelStateChanged(bool active);

  void OnFreWebUiStateChanged(mojom::FreWebUiState new_state);

  // Used to update the tooltip text when the showing states of the Glic
  // window/FRE change.
  void UpdateTooltipText();

  void OnLabelVisibilityChanged();

  // Callback subscription for listening to changes to the Glic window
  // activation changes.
  base::CallbackListSubscription glic_window_activation_subscription_;

  // Callback subscription for listening to changes to the FRE WebUI state.
  base::CallbackListSubscription fre_subscription_;
#endif  // BUILDFLAG(ENABLE_GLIC)

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

  // Invoked when the button hide animation finishes.
  base::RepeatingClosure expansion_animation_done_callback_;

  // Cached widths for animating label changes.
  int initial_width_ = 0;
  int expanded_width_ = 0;

  // View to be drawn behind the icon and label with a background color.
  raw_ptr<View> highlight_view_ = nullptr;

  // Container view for the icon and label, and the highlight drawn behind them.
  raw_ptr<View> icon_label_highlight_view_ = nullptr;

  // If GlicEntrypointVariations is enabled, this animation is responsible for
  // changing the button width when the nudge is shown.
  std::unique_ptr<gfx::SlideAnimation> expansion_animation_;

  // Holds the incoming nudge text until the point in the animation when it can
  // be applied.
  std::optional<std::u16string> pending_text_;

  const ui::ImageModel normal_icon_;
  const ui::ImageModel icon_for_highlight_;

  bool glic_panel_is_open_ = false;
  // If this flag is set, the default label on the static button is in the
  // process of being shown.
  // TODO(crbug.com/460400955): This is a temporary fix for 143, this code
  // should be refactored to use the new solution in 144.
  bool is_animating_text_ = false;
  int default_label_width_ = 0;
  // Keep track if the label is currently suppressed, for example, when the task
  // icon is showing.
  bool is_label_suppressed_ = false;

  base::WeakPtrFactory<GlicButton> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_GLIC_BUTTON_H_
