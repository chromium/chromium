// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOASTS_TOAST_VIEW_H_
#define CHROME_BROWSER_UI_TOASTS_TOAST_VIEW_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

namespace ui {
class MenuModel;
}  // namespace ui

namespace views {
class ImageButton;
class ImageView;
class Label;
class MdTextButton;
class MenuModelAdapter;
class MenuRunner;
}  // namespace views

namespace toasts {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ToastCloseReason {
  kAutoDismissed = 0,
  kActionButton = 1,
  kCloseButton = 2,
  kPreempted = 3,
  kMenuItemClick = 4,
  kFeatureDismiss = 5,
  kAbort = 6,
  kMaxValue = kAbort
};

// The view for toasts.
class ToastView : public views::BubbleDialogDelegateView,
                  public views::AnimationDelegateViews {
  METADATA_HEADER(ToastView, views::BubbleDialogDelegateView)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kToastViewId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kToastActionButton);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kToastCloseButton);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kToastMenuButton);
  ToastView(
      views::View* anchor_view,
      const std::u16string& toast_text,
      const gfx::VectorIcon& icon,
      const ui::ImageModel* image_override,
      bool should_hide_ui_for_fullscreen,
      base::RepeatingCallback<void(ToastCloseReason)> on_toast_close_callback);
  ~ToastView() override;

  // Must be called prior to Init (which is called from
  // views::BubbleDialogDelegateView::CreateBubble).
  void AddActionButton(
      const std::u16string& action_button_text,
      base::RepeatingClosure action_button_callback = base::DoNothing());

  // Must be called prior to Init (which is called from
  // views::BubbleDialogDelegateView::CreateBubble).
  void AddCloseButton(
      base::RepeatingClosure close_button_callback = base::DoNothing());

  // Adds a three-dot menu button that, when clicked, runs the menu defined by
  // `model`. Must be called prior to `Init` (which is called from
  // views::BubbleDialogDelegateView::CreateBubble).
  void AddMenu(std::unique_ptr<ui::MenuModel> model);

  // views::BubbleDialogDelegateView:
  void Init() override;

  // AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;

  void AnimateIn();

  // Animates out the toast, then closes the toast widget.
  void Close(ToastCloseReason close_reason);

  void UpdateRenderToastOverWebContentsAndPaint(
      const bool render_toast_over_web_contents);

  views::Label* label_for_testing() { return label_; }
  views::MdTextButton* action_button_for_testing() { return action_button_; }
  views::ImageButton* close_button_for_testing() { return close_button_; }

  // Gets the icon/image size from the layout provider.
  static int GetIconSize();

 protected:
  // views::BubbleDialogDelegateView:
  gfx::Rect GetBubbleBounds() override;
  void OnThemeChanged() override;

 private:
  void AnimateOut(base::OnceClosure callback, bool show_height_animation);

  // Opens the menu if it is not already open or closes the existing menu
  // otherwise. Assumes that `menu_model_` and `menu_button_` are not null.
  void OnMenuButtonClicked();

  // Resets `menu_runner_` and removes the highlight from the menu button.
  void OnMenuClosed();

  gfx::LinearAnimation height_animation_{this};
  gfx::Rect starting_widget_bounds_;
  gfx::Rect target_widget_bounds_;
  gfx::Tween::Type height_animation_tween_;

  const std::u16string toast_text_;
  const raw_ref<const gfx::VectorIcon> icon_;
  const raw_ptr<const ui::ImageModel> image_override_;
  bool render_toast_over_web_contents_;
  bool has_close_button_ = false;
  bool has_action_button_ = false;
  std::u16string action_button_text_;
  base::RepeatingClosure action_button_callback_;
  base::RepeatingClosure close_button_callback_;
  base::RepeatingCallback<void(ToastCloseReason)> toast_close_callback_;
  std::unique_ptr<ui::MenuModel> menu_model_;
  // Wraps `menu_model_` and triggers closing the toast after executing menu
  // commands.
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // Raw pointers to child views.
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::ImageView> icon_view_ = nullptr;
  raw_ptr<views::MdTextButton> action_button_ = nullptr;
  raw_ptr<views::ImageButton> close_button_ = nullptr;

  raw_ptr<views::ImageButton> menu_button_ = nullptr;
  std::optional<views::Button::ScopedAnchorHighlight> menu_button_highlight_;

  // If there was an attempt to close `this` while a menu was showing,
  // `pending_close_reason_` contains the reason for closing the view and it
  // is used to close the view once the menu stops showing.
  std::optional<ToastCloseReason> pending_close_reason_;
};

}  // namespace toasts

#endif  // CHROME_BROWSER_UI_TOASTS_TOAST_VIEW_H_
