// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_ACTOR_TASK_ICON_H_
#define CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_ACTOR_TASK_ICON_H_

#include <concepts>
#include <optional>
#include <string>

#include "base/callback_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/glic/glic_base_shim.h"
#include "chrome/browser/ui/views/glic/glic_button_interface.h"
#include "chrome/browser/ui/views/tabs/tab_strip_nudge_button.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/color/color_variant.h"
#include "ui/compositor/layer.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/view_class_properties.h"

class BrowserWindowInterface;

namespace glic {

inline constexpr int kActorNudgeLabelMargin = 6;
inline constexpr int kSplitLeftEdgeRadius = 2;

// Rounded edge radius for Gemini Button when split with actor task icon.
inline constexpr int kSplitButtonRoundedRadius = 10;
// Flag edge radius for Gemini Button when split with actor task icon.
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
    // Disable subpixel rendering to prevent crash on transparent layer
    this->label()->SetSubpixelRenderingEnabled(false);
    this->label()->SetPaintToLayer();
    if (this->label()->layer()) {
      this->label()->layer()->SetFillsBoundsOpaquely(false);
    }

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
  virtual void SetDefaultColors() {
    SetForegroundFrameActiveColorId(kColorNewTabButtonForegroundFrameActive);
    SetForegroundFrameInactiveColorId(
        kColorNewTabButtonForegroundFrameInactive);
    SetBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
    SetBackgroundFrameInactiveColorId(
        kColorNewTabButtonCRBackgroundFrameInactive);
  }

  void SetPressedState(bool is_pressed) {
    SetPressedColor(is_pressed);
    SetOrResetPressedLock(is_pressed);
  }

  void SetOrResetPressedLock(bool is_pressed) {
    views::MenuButtonController* controller =
        static_cast<views::MenuButtonController*>(this->button_controller());
    if (is_pressed && controller) {
      pressed_lock_ = controller->TakeLock();
    } else {
      pressed_lock_.reset();
    }
  }

  // Get whether the button is currently pressed. The button should be pressed
  // when the task list bubble is showing.
  bool GetIsPressed() { return pressed_lock_.get(); }

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

  gfx::Rect GetAnchorBoundsInScreen() const override {
    return this->GetBoundsInScreen();
  }

  float GetWidthFactor() const override { return width_factor_; }
  void SetWidthFactor(float factor) override {
    width_factor_ = factor;
    this->PreferredSizeChanged();
  }

 protected:
  // views::LabelButton:
  void SetText(std::u16string_view text) override { T::SetText(text); }

  virtual int GetSplitRoundedEdgeRadius() { return kSplitButtonRoundedRadius; }

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

  void SetLeftRightCornerRadii(int left, int right) override {
    GlicBaseShim<T>::SetLeftRightCornerRadii(left, right);
  }

  void UpdateColors() override { GlicBaseShim<T>::UpdateColors(); }

  void SetInkdropHoverColorId(const ChromeColorIds new_color_id) override {
    GlicBaseShim<T>::SetInkdropHoverColorId(new_color_id);
  }

 private:
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

  std::unique_ptr<views::MenuButtonController::PressedLock> pressed_lock_;

  const raw_ptr<BrowserWindowInterface> browser_window_interface_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_ACTOR_TASK_ICON_H_
