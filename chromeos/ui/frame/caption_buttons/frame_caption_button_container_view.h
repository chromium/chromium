// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_CONTAINER_VIEW_H_
#define CHROMEOS_UI_FRAME_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_CONTAINER_VIEW_H_

#include <map>

#include "base/check.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ui/frame/caption_buttons/caption_button_model.h"
#include "chromeos/ui/frame/caption_buttons/frame_size_button_delegate.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_nudge_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/frame_caption_button.h"

namespace gfx {
class SlideAnimation;
struct VectorIcon;
}  // namespace gfx

namespace views {
class Widget;
}

namespace chromeos {

// Container view for the frame caption buttons. It performs the appropriate
// action when a caption button is clicked.
//
// NOTE: The associated test (frame_caption_button_container_view_unittest.cc)
// is in //ash because it needs ash test support (AshTestBase and its
// utilities).
class COMPONENT_EXPORT(CHROMEOS_UI_FRAME) FrameCaptionButtonContainerView
    : public views::BoxLayoutView,
      public FrameSizeButtonDelegate,
      public views::AnimationDelegateViews,
      public views::WidgetObserver {
  METADATA_HEADER(FrameCaptionButtonContainerView, views::BoxLayoutView)

 public:
  // `frame` is the views::Widget that the caption buttons act on.
  // `custom_button` is an optional caption button. It is placed as the
  // left-most caption button (in LTR mode).
  FrameCaptionButtonContainerView(
      views::Widget* frame,
      bool is_close_button_enabled = true,
      std::unique_ptr<views::FrameCaptionButton> custom_button = nullptr);
  FrameCaptionButtonContainerView(const FrameCaptionButtonContainerView&) =
      delete;
  FrameCaptionButtonContainerView& operator=(
      const FrameCaptionButtonContainerView&) = delete;
  ~FrameCaptionButtonContainerView() override;

  // For testing.
  class COMPONENT_EXPORT(CHROMEOS_UI_FRAME) TestApi {
   public:
    explicit TestApi(FrameCaptionButtonContainerView* container_view)
        : container_view_(container_view) {}
    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;

    void EndAnimations();

    views::FrameCaptionButton* minimize_button() const {
      return container_view_->minimize_button_;
    }

    views::FrameCaptionButton* size_button() const {
      return container_view_->size_button_;
    }

    views::FrameCaptionButton* close_button() const {
      return container_view_->close_button_;
    }

    views::FrameCaptionButton* menu_button() const {
      return container_view_->menu_button_;
    }

    views::FrameCaptionButton* custom_button() const {
      return container_view_->custom_button_;
    }

    views::FrameCaptionButton* float_button() const {
      return container_view_->float_button_;
    }

    MultitaskMenuNudgeController* nudge_controller() const {
      return &container_view_->nudge_controller_;
    }

   private:
    raw_ptr<FrameCaptionButtonContainerView> container_view_;
  };

  views::FrameCaptionButton* size_button() { return size_button_; }
  bool window_controls_overlay_enabled() const {
    return window_controls_overlay_enabled_;
  }

  // Sets whether the buttons should be painted as active. Does not schedule
  // a repaint.
  void SetPaintAsActive(bool paint_as_active);

  // Sets the id of the vector image to paint the button for |icon|. The
  // FrameCaptionButtonContainerView will keep track of the image to use for
  // |icon| even if none of the buttons currently use |icon|.
  void SetButtonImage(views::CaptionButtonIcon icon,
                      const gfx::VectorIcon& icon_definition);

  // Sets the background frame color that buttons should compute their color
  // respective to.
  void SetButtonBackgroundColor(SkColor background_color);

  // Set the color token which should be used to resolve the button's icon color
  // directly.
  void SetButtonIconColor(ui::ColorId icon_color_id);

  // Tell the window controls to reset themselves to the normal state.
  void ResetWindowControls();

  // Creates or removes a layer for the caption button container when window
  // controls overlay is enabled or disabled.
  void OnWindowControlsOverlayEnabledChanged(bool enabled,
                                             SkColor background_color);

  // Updates the visibility of the caption button container based on whether the
  // app is in borderless mode or not, which means whether the title bar is
  // shown or not.
  void UpdateBorderlessModeEnabled(bool enabled);

  // Updates the caption buttons' state based on the caption button model's
  // state. A parent view should relayout to reflect the change in states.
  void UpdateCaptionButtonState(bool animate);

  // Updates the image and tooltips of the size, snap, and float buttons. These
  // can change on state change or display orientation change.
  void UpdateButtonsImageAndTooltip();

  // Sets the size of the buttons in this container.
  void SetButtonSize(const gfx::Size& size);

  // Sets whether close button is enabled.
  void SetCloseButtonEnabled(bool enabled);

  // Sets the CaptionButtonModel. Caller is responsible for updating
  // the state by calling UpdateCaptionButtonState.
  void SetModel(std::unique_ptr<CaptionButtonModel> model);
  const CaptionButtonModel* model() const { return model_.get(); }

  // Sets the callback that will be invoked when any size button is pressed. If
  // the callback is set, the default behavior (e.g. maximize |frame_|) will be
  // skipped so caller must be responsible for the action. If the callback
  // returns false, it will fall back to the default dehavior.
  void SetOnSizeButtonPressedCallback(base::RepeatingCallback<bool()> callback);
  void ClearOnSizeButtonPressedCallback();

  // views::View:
  void Layout(PassKey) override;
  void ChildPreferredSizeChanged(View* child) override;
  void ChildVisibilityChanged(View* child) override;

  // views::AnimationDelegateViews:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

 private:
  // Sets |button|'s icon to |icon|. If |animate| is Animate::kYes, the button
  // will crossfade to the new icon. If |animate| is Animate::kNo and
  // |icon| == |button|->icon(), the crossfade animation is progressed to the
  // end.
  void SetButtonIcon(views::FrameCaptionButton* button,
                     views::CaptionButtonIcon icon,
                     Animate animate);

  // Helpers to update the icons of various buttons and maybe their tooltips as
  // well.
  void UpdateSizeButton();
  void UpdateSnapButtons();
  void UpdateFloatButton();

  void MinimizeButtonPressed();
  void SizeButtonPressed();
  void CloseButtonPressed();
  void MenuButtonPressed();
  void FloatButtonPressed();

  bool SizeButtonShouldBeVisible() const;
  void LayoutButtonsFromAnimation(int x_slide, int alpha);

  // FrameSizeButtonDelegate:
  bool IsMinimizeButtonVisible() const override;
  void SetButtonsToNormal(Animate animate) override;
  void SetButtonIcons(views::CaptionButtonIcon minimize_button_icon,
                      views::CaptionButtonIcon close_button_icon,
                      Animate animate) override;
  const views::FrameCaptionButton* GetButtonClosestTo(
      const gfx::Point& position_in_screen) const override;
  void SetHoveredAndPressedButtons(
      const views::FrameCaptionButton* to_hover,
      const views::FrameCaptionButton* to_press) override;
  bool CanSnap() override;
  void ShowSnapPreview(SnapDirection snap, bool allow_haptic_feedback) override;
  void CommitSnap(SnapDirection snap) override;
  MultitaskMenuNudgeController* GetMultitaskMenuNudgeController() override;

  // The widget that the buttons act on.
  raw_ptr<views::Widget> frame_;

  // The buttons. In the normal button style, at most one of |minimize_button_|
  // and |size_button_| is visible.
  raw_ptr<views::FrameCaptionButton> custom_button_ = nullptr;
  raw_ptr<views::FrameCaptionButton> float_button_ = nullptr;
  raw_ptr<views::FrameCaptionButton> menu_button_ = nullptr;
  raw_ptr<views::FrameCaptionButton> minimize_button_ = nullptr;
  raw_ptr<views::FrameCaptionButton> close_button_ = nullptr;

  // Stored as a `FrameSizeButton` so the multitask menu can be accessed.
  raw_ptr<views::FrameCaptionButton> size_button_ = nullptr;

  // Handles showing the educational nudge for the clamshell multitask menu.
  MultitaskMenuNudgeController nudge_controller_;

  // Mapping of the image needed to paint a button for each of the values of
  // CaptionButtonIcon.
  std::map<views::CaptionButtonIcon,
           raw_ptr<const gfx::VectorIcon, CtnExperimental>>
      button_icon_map_;

  // Animation that affects the visibility of |size_button_| and the position of
  // buttons to the left of it. Usually this is just the minimize button but it
  // can also include a PWA menu button.
  std::unique_ptr<gfx::SlideAnimation> tablet_mode_animation_;

  std::unique_ptr<CaptionButtonModel> model_;

  // Callback which has to be called if not null when `SetCloseButtonEnabled`
  // is called. When called, it updates `DefaultCaptionButtonModel` state.
  base::RepeatingCallback<void(bool close_button_enabled)>
      on_close_button_enabled_changed_callback_;

  // Callback for the size button action, which overrides the default behavior.
  // If the callback returns false, it will fall back to the default dehavior.
  base::RepeatingCallback<bool()> on_size_button_pressed_callback_;

  // Keeps track of the window-controls-overlay toggle, and defines if the
  // background of the entire view should be updated when the background of the
  // button container changes and SetBackgroundColor() gets called.
  bool window_controls_overlay_enabled_ = false;

  // Keeps track of the borderless mode being enabled or not. This defines the
  // visibility of the caption button container.
  bool is_borderless_mode_enabled_ = false;

  base::ScopedObservation<views::Widget, views::WidgetObserver> frame_observer_{
      this};
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_CONTAINER_VIEW_H_
