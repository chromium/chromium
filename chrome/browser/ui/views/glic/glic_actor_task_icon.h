// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_ACTOR_TASK_ICON_H_
#define CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_ACTOR_TASK_ICON_H_

#include <concepts>
#include <string>

#include "base/callback_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/glic/glic_base_shim.h"
#include "chrome/browser/ui/views/glic/glic_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_nudge_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/view_class_properties.h"

class BrowserWindowInterface;

namespace glic {

inline constexpr int kActorNudgeLabelMargin = 6;
inline constexpr int kSplitLeftEdgeRadius = 2;
inline constexpr int kSplitRightEdgeRadius = 10;

// Defines how the button calculates its width during animation.
enum class AnimationMode {
  kEntry,  // Animating from 0 width -> icon width
  kNudge   // Animating from icon width -> full nudge width
};

template <typename T>
  requires std::derived_from<T, views::LabelButton>
class GlicActorTaskIcon : public GlicBaseShim<T> {
 public:
  template <typename... BaseArgs>
  explicit GlicActorTaskIcon(BrowserWindowInterface* browser_window_interface,
                             BaseArgs&&... base_args)
      : GlicBaseShim<T>(std::move(base_args)...),
        browser_window_interface_(browser_window_interface) {
    this->SetProperty(views::kElementIdentifierKey,
                      kGlicActorTaskIconElementId);

    // Explicitly overwrite the horizontal margins.
    this->label()->SetProperty(views::kMarginsKey, gfx::Insets().set_left_right(
                                                       kActorNudgeLabelMargin,
                                                       kActorNudgeLabelMargin));

    SetTaskIconToDefault();

    // The task icon will only ever be shown with the GlicButton, so can always
    // set the corner radii for split button styling.
    SetLeftRightCornerRadii(kSplitLeftEdgeRadius, kSplitRightEdgeRadius);
    SetInkdropHoverColorId(kColorTabBackgroundInactiveHoverFrameActive);

    UpdateColors();

    this->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

    auto* const layout_manager =
        this->SetLayoutManager(std::make_unique<views::BoxLayout>());
    layout_manager->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kStart);
  }

  GlicActorTaskIcon(const GlicActorTaskIcon&) = delete;
  GlicActorTaskIcon& operator=(const GlicActorTaskIcon&) = delete;
  ~GlicActorTaskIcon() override = default;

  // GlicActorTaskIcon width changes depending on animation mode, use
  // CalculatePrefferedSize to handle variations.
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    int full_width = this->GetLayoutManager()
                         ->GetPreferredSize(this, available_size)
                         .width();

    const int height =
        T::CalculatePreferredSize(
            views::SizeBounds(full_width, available_size.height()))
            .height();
    int icon_only_width = height;
    int width = 0;
    int min_width = 0;
    switch (animation_mode_) {
      case AnimationMode::kEntry:
        // Animate from 0 to icon width
        full_width = icon_only_width;
        break;
      case AnimationMode::kNudge:
        // Animate from icon width to full width
        min_width = icon_only_width;
        break;
    }
    width = std::lerp(min_width, full_width, GetWidthFactor());
    return gfx::Size(width, height);
  }

  // views::View:
  void AddedToWidget() override {
    T::AddedToWidget();
    views::Widget* widget = this->GetWidget();
    if (!widget) {
      return;
    }

    window_did_become_active_subscription_ =
        browser_window_interface_->RegisterDidBecomeActive(base::BindRepeating(
            &GlicActorTaskIcon<T>::OnBrowserWindowDidBecomeActive,
            base::Unretained(this)));
    window_did_become_inactive_subscription_ =
        browser_window_interface_->RegisterDidBecomeInactive(
            base::BindRepeating(
                &GlicActorTaskIcon<T>::OnBrowserWindowDidBecomeInactive,
                base::Unretained(this)));

    UpdateInkdropHoverColor(browser_window_interface_->IsActive());
  }

  void RemovedFromWidget() override {
    window_did_become_active_subscription_ = {};
    window_did_become_inactive_subscription_ = {};
    T::RemovedFromWidget();
  }

  void SetIsShowingNudge(bool is_showing) override {
    GlicBaseShim<T>::SetIsShowingNudge(is_showing);
  }

  // Sets the task icon back to its default colors.
  void SetDefaultColors() {
    SetForegroundFrameActiveColorId(kColorNewTabButtonForegroundFrameActive);
    SetForegroundFrameInactiveColorId(
        kColorNewTabButtonForegroundFrameInactive);
    SetBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
    SetBackgroundFrameInactiveColorId(
        kColorNewTabButtonCRBackgroundFrameInactive);
  }

  // Sets the task icon's color to its pressed state color if `is_pressed` is
  // true, or to its default color otherwise.
  void SetPressedColor(bool is_pressed) {
    this->SetHighlighted(is_pressed);
    UpdateColors();
  }

  // Show the task nudge with the given text.
  void ShowNudgeLabel(const std::u16string nudge_label) {
    SetText(nudge_label);
    this->SetTooltipText(nudge_label);
  }

  // Sets the task icon to its default colors, label, and tooltip text.
  void SetTaskIconToDefault() {
    SetText(std::u16string());
    this->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ACTOR_TASK_INDICATOR_TOOLTIP));
    SetDefaultColors();
  }

  // Updates the background painter to match the current border insets.
  void RefreshBackground() { UpdateColors(); }

  void SetAnimationMode(AnimationMode mode) {
    if (animation_mode_ != mode) {
      animation_mode_ = mode;
      this->PreferredSizeChanged();
    }
  }

  AnimationMode GetAnimationMode() const { return animation_mode_; }

  // GetBoundsInScreen() gives a rect with some padding that extends beyond the
  // visible edges of the button. This function returns a rect without that
  // padding in order to anchor the ActorTaskListBubble on the edge of the
  // button.
  gfx::Rect GetAnchorBoundsInScreen() const override {
    gfx::Rect bounds = this->GetBoundsInScreen();
    bounds.Inset(this->GetInsets());
    return bounds;
  }

  float GetWidthFactor() const { return width_factor_; }

 protected:
  // views::LabelButton:
  void SetText(std::u16string_view text) override {
    if constexpr (std::is_same_v<T, ToolbarButton>) {
      // SetText is private in ToolbarButton and prefers to use SetHighlight.
      std::u16string highlight_text(text);
      this->SetHighlight(highlight_text, kTextOnHighlight);
    } else {
      T::SetText(text);
    }
  }

  void SetForegroundFrameActiveColorId(ui::ColorId new_color_id) override {
    GlicBaseShim<T>::SetForegroundFrameActiveColorId(new_color_id);
  }

  void SetForegroundFrameInactiveColorId(ui::ColorId new_color_id) override {
    GlicBaseShim<T>::SetForegroundFrameInactiveColorId(new_color_id);
  }

  void SetBackgroundFrameActiveColorId(ui::ColorId new_color_id) override {
    GlicBaseShim<T>::SetBackgroundFrameActiveColorId(new_color_id);
  }

  void SetBackgroundFrameInactiveColorId(ui::ColorId new_color_id) override {
    GlicBaseShim<T>::SetBackgroundFrameInactiveColorId(new_color_id);
  }

 private:
  void SetLeftRightCornerRadii(int left, int right) override {
    GlicBaseShim<T>::SetLeftRightCornerRadii(left, right);
  }

  void SetInkdropHoverColorId(const ChromeColorIds new_color_id) override {
    GlicBaseShim<T>::SetInkdropHoverColorId(new_color_id);
  }

  void UpdateColors() override { GlicBaseShim<T>::UpdateColors(); }

  void OnBrowserWindowDidBecomeActive(BrowserWindowInterface* bwi) {
    UpdateInkdropHoverColor(true);
  }

  void OnBrowserWindowDidBecomeInactive(BrowserWindowInterface* bwi) {
    UpdateInkdropHoverColor(false);
  }

  void UpdateInkdropHoverColor(bool is_frame_active) {
    SetInkdropHoverColorId(is_frame_active
                               ? kColorTabBackgroundInactiveHoverFrameActive
                               : kColorTabBackgroundInactiveHoverFrameInactive);
    UpdateColors();
  }

  void NotifyClick(const ui::Event& event) override { T::NotifyClick(event); }

  AnimationMode animation_mode_ = AnimationMode::kEntry;
  base::CallbackListSubscription window_did_become_active_subscription_;
  base::CallbackListSubscription window_did_become_inactive_subscription_;

  float width_factor_ = 0;
  bool is_showing_nudge_ = false;

  const raw_ptr<BrowserWindowInterface> browser_window_interface_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_ACTOR_TASK_ICON_H_
