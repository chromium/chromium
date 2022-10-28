// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_WINDOW_VIEWS_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/video_picture_in_picture_window_controller.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

// Base class for the Chrome desktop implementation of VideoOverlayWindow.
// This will only be implemented in views, which will support all desktop
// platforms.
//
// This class is a views::Widget. The subclasses implement the needed
// methods for their corresponding OverlayWindow subclass.
class OverlayWindowViews : public views::Widget,
                           public display::DisplayObserver {
 public:
  OverlayWindowViews(const OverlayWindowViews&) = delete;
  OverlayWindowViews& operator=(const OverlayWindowViews&) = delete;

  ~OverlayWindowViews() override;

  static views::WidgetDelegate* CreateDelegate();

  enum class WindowQuadrant { kBottomLeft, kBottomRight, kTopLeft, kTopRight };

  // Returns the quadrant the OverlayWindowViews is primarily in on the current
  // work area.
  static OverlayWindowViews::WindowQuadrant GetCurrentWindowQuadrant(
      const gfx::Rect window_bounds,
      content::PictureInPictureWindowController* controller);

  // views::Widget:
  void OnNativeFocus() override;
  void OnNativeBlur() override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnNativeWidgetMove() override;
  void OnNativeWidgetSizeChanged(const gfx::Size& new_size) override;
  void OnNativeWidgetWorkspaceChanged() override;
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  // Subclasses must implement this and call OnGestureEventHandledOrIgnored
  void OnGestureEvent(ui::GestureEvent* event) override = 0;

  virtual bool ControlsHitTestContainsPoint(const gfx::Point& point) = 0;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Gets the proper hit test component when the hit point is on the resize
  // handle in order to force a drag-to-resize.
  virtual int GetResizeHTComponent() const = 0;

  virtual gfx::Rect GetResizeHandleControlsBounds() = 0;

  // Updates the bounds of |resize_handle_view_| based on what |quadrant| the
  // PIP window is in.
  virtual void UpdateResizeHandleBounds(WindowQuadrant quadrant) = 0;
#endif

  // Called when the bounds of the controls should be updated.
  virtual void OnUpdateControlsBounds() = 0;

  // Accessors for use in the base class
  virtual content::PictureInPictureWindowController* GetController() const = 0;
  virtual views::View* GetWindowBackgroundView() const = 0;
  virtual views::View* GetControlsContainerView() const = 0;

  // Helpers for OverlayWindow functions
  gfx::Size& GetNaturalSize();
  void DoShowInactive();
  void DoUpdateNaturalSize(const gfx::Size& natural_size);

  bool OnGestureEventHandledOrIgnored(ui::GestureEvent* event);

  // Returns true if the controls (e.g. close button, play/pause button) are
  // visible.
  bool AreControlsVisible() const;

  // Updates the controls view::Views to reflect |is_visible|.
  void UpdateControlsVisibility(bool is_visible);

  // VideoOverlayWindowViews does this for testing.
  void ForceControlsVisibleForTesting(bool visible);

  // Determines whether a layout of the window controls has been scheduled but
  // is not done yet.
  bool IsLayoutPendingForTesting() const;

  void set_minimum_size_for_testing(const gfx::Size& min_size) {
    min_size_ = min_size;
  }

  // display::DisplayObserver
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

 protected:
  OverlayWindowViews();

  // Return the work area for the nearest display the widget is on.
  gfx::Rect GetWorkAreaForWindow() const;

  // Determine the intended bounds of |this|. This should be called when there
  // is reason for the bounds to change, such as switching primary displays or
  // playing a new video (i.e. different aspect ratio).
  gfx::Rect CalculateAndUpdateWindowBounds();

  // Set up the views::Views that will be shown on the window.
  virtual void SetUpViews() = 0;

  // Finish initialization by performing the steps that require the root View.
  virtual void OnRootViewReady() = 0;

  // Updates the bounds of the controls.
  void UpdateControlsBounds();

  // Update the max size of the widget based on |work_area| and window size.
  void UpdateMaxSize(const gfx::Rect& work_area);

  // Update the bounds of the layers on the window. This may introduce
  // letterboxing.
  virtual void UpdateLayerBoundsWithLetterboxing(gfx::Size window_size) = 0;

  // Calculate and set the bounds of the controls.
  gfx::Rect CalculateControlsBounds(int x, const gfx::Size& size);
  void UpdateControlsPositions();

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class OverlayWindowControl {
    kBackToTab = 0,
    kMuteDeprecated,
    kSkipAd,
    kClose,
    kPlayPause,
    kNextTrack,
    kPreviousTrack,
    kToggleMicrophone,
    kToggleCamera,
    kHangUp,
    kMaxValue = kHangUp
  };
  void RecordButtonPressed(OverlayWindowControl);
  void RecordTapGesture(OverlayWindowControl);

  // Whether or not the window has been shown before. This is used to determine
  // sizing and placement. This is different from checking whether the window
  // components has been initialized.
  bool has_been_shown_ = false;

  // The upper and lower bounds of |current_size_|. These are determined by the
  // size of the primary display work area when Picture-in-Picture is initiated.
  // TODO(apacible): Update these bounds when the display the window is on
  // changes. http://crbug.com/819673
  gfx::Size min_size_;
  gfx::Size max_size_;

  // The natural size of the video to show. This is used to compute sizing and
  // ensuring factors such as aspect ratio is maintained.
  gfx::Size natural_size_;

  // Temporary storage for child Views. Used during the time between
  // construction and initialization, when the views::View pointer members must
  // already be initialized, but there is no root view to add them to yet.
  std::vector<std::unique_ptr<views::View>> view_holder_;

  // Automatically hides the controls a few seconds after user tap gesture.
  base::RetainingOneShotTimer hide_controls_timer_;

  // Timer used to update controls bounds.
  std::unique_ptr<base::OneShotTimer> update_controls_bounds_timer_;

  // If set, controls will always either be shown or hidden, instead of showing
  // and hiding automatically. Only used for testing via
  // ForceControlsVisibleForTesting().
  absl::optional<bool> force_controls_visible_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_WINDOW_VIEWS_H_
