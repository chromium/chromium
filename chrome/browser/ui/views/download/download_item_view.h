// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A view that implements one download on the Download shelf.
// Each DownloadItemView contains an application icon, a text label
// indicating the download's file name, a text label indicating the
// download's status (such as the number of bytes downloaded so far)
// and a button for canceling an in progress download, or opening
// the completed download.
//
// The DownloadItemView lives in the Browser, and has a corresponding
// DownloadController that receives / writes data which lives in the
// Renderer.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_ITEM_VIEW_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/icon_manager.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/download_manager.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/font_list.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"

class DownloadShelfView;
class DownloadShelfContextMenuView;

namespace gfx {
class Image;
class ImageSkia;
class SlideAnimation;
}

namespace ui {
class ThemeProvider;
}

namespace views {
class ImageButton;
class Label;
class MdTextButton;
}

// Represents a single download item on the download shelf. Encompasses an icon,
// text, malicious download warnings, etc.
class DownloadItemView : public views::InkDropHostView,
                         public views::ButtonListener,
                         public views::ContextMenuController,
                         public DownloadUIModel::Observer,
                         public gfx::AnimationDelegate {
 public:
  DownloadItemView(DownloadUIModel::DownloadUIModelPtr download,
                   DownloadShelfView* parent,
                   views::View* accessible_alert);
  ~DownloadItemView() override;

  // Timer callback for handling animations
  void UpdateDownloadProgress();
  void StartDownloadProgress();
  void StopDownloadProgress();

  // Returns the base color for text on this download item, based on |theme|.
  static SkColor GetTextColorForThemeProvider(const ui::ThemeProvider* theme);

  void OnExtractIconComplete(gfx::Image* icon);

  // Returns the DownloadUIModel object belonging to this item.
  DownloadUIModel* model() { return model_.get(); }

  // Submits download to download feedback service if the user has approved and
  // the download is suitable for submission, then apply |download_command|.
  // If user hasn't seen SBER opt-in text before, show SBER opt-in dialog first.
  void MaybeSubmitDownloadToFeedbackService(
      DownloadCommands::Command download_command);

  // DownloadUIModel::Observer:
  void OnDownloadUpdated() override;
  void OnDownloadOpened() override;
  void OnDownloadDestroyed() override;

  // views::View:
  void Layout() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  gfx::Size CalculatePreferredSize() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool GetTooltipText(const gfx::Point& p,
                      base::string16* tooltip) const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // view::InkDropHostView:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  void OnInkDropCreated() override;
  SkColor GetInkDropBaseColor() const override;

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::ContextMenuController.
  void ShowContextMenuForView(View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // gfx::AnimationDelegate implementation.
  void AnimationProgressed(const gfx::Animation* animation) override;

 protected:
  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;
  void AddedToWidget() override;
  void OnThemeChanged() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(DownloadItemViewDangerousDownloadLabelTest,
                           AdjustTextAndGetSize);

  enum State { NORMAL = 0, HOT, PUSHED };

  enum Mode {
    NORMAL_MODE = 0,  // Showing download item.
    DANGEROUS_MODE,   // Displaying the dangerous download warning.
    MALICIOUS_MODE    // Displaying the malicious download warning.
  };

  static constexpr int kTextWidth = 140;

  // Vertical padding between filename and status text.
  static constexpr int kVerticalTextPadding = 1;

  static constexpr int kTooltipMaxWidth = 800;

  // Padding before the icon and at end of the item.
  static constexpr int kStartPadding = 12;
  static constexpr int kEndPadding = 6;

  // Horizontal padding between progress indicator and filename/status text.
  static constexpr int kProgressTextPadding = 8;

  // The space between the Save and Discard buttons when prompting for a
  // dangerous download.
  static constexpr int kSaveDiscardButtonPadding = 5;

  // The touchable space around the dropdown button's icon.
  static constexpr int kDropdownBorderWidth = 10;

  // The space on the right side of the dangerous download label.
  static constexpr int kLabelPadding = 8;

  // Height/width of the warning icon, also in dp.
  static constexpr int kWarningIconSize = 24;

  // How long the 'download complete' animation should last for.
  static constexpr int kCompleteAnimationDurationMs = 2500;

  // How long the 'download interrupted' animation should last for.
  static constexpr int kInterruptedAnimationDurationMs = 2500;

  // How long we keep the item disabled after the user clicked it to open the
  // downloaded item.
  static constexpr int kDisabledOnOpenDuration = 3000;

  void OpenDownload();

  // Submits the downloaded file to the safebrowsing download feedback service.
  // Returns whether submission was successful. Applies |download_command|, if
  // submission fails.
  bool SubmitDownloadToFeedbackService(
      DownloadCommands::Command download_command);

  // If the user has |enabled| uploading, calls SubmitDownloadToFeedbackService.
  // Otherwise, apply |download_command|.
  void SubmitDownloadWhenFeedbackServiceEnabled(
      DownloadCommands::Command download_command,
      bool feedback_enabled);

  // This function calculates the vertical coordinate to draw the file name text
  // relative to local bounds.
  int GetYForFilenameText() const;

  // Painting of various download item bits.
  void DrawStatusText(gfx::Canvas* canvas);
  void DrawFilename(gfx::Canvas* canvas);
  void DrawIcon(gfx::Canvas* canvas);

  void LoadIcon();
  void LoadIconIfItemPathChanged();

  // Update the button colors based on the current theme.
  void UpdateColorsFromTheme();

  void UpdateDropdownButton();

  // Shows the context menu at the specified location. |point| is in the view's
  // coordinate system.
  void ShowContextMenuImpl(const gfx::Rect& rect,
                           ui::MenuSourceType source_type);

  // Common code for handling pointer events (i.e. mouse or gesture).
  void HandlePressEvent(const ui::LocatedEvent& event, bool active_event);
  void HandleClickEvent(const ui::LocatedEvent& event, bool active_event);

  // Sets the state and triggers a repaint.
  void SetDropdownState(State new_state);

  // Configures the InkDrop. e.g. disables highlight when in dangerous mode.
  void ConfigureInkDrop();

  void SetMode(Mode mode);

  // Whether we are in the dangerous mode.
  bool IsShowingWarningDialog() const {
    return mode_ == DANGEROUS_MODE || mode_ == MALICIOUS_MODE;
  }

  // Clears or shows the warning dialog as per the state of |model_|.
  void ToggleWarningDialog();

  // Reverts from dangerous mode to normal download mode.
  void ClearWarningDialog();

  // Starts displaying the dangerous download warning or the malicious download
  // warning.
  void ShowWarningDialog();

  // Returns the current warning icon (should only be called when the view is
  // actually showing a warning).
  gfx::ImageSkia GetWarningIcon();

  // Sets |size| with the size of the Save and Discard buttons (they have the
  // same size).
  gfx::Size GetButtonSize() const;

  // Sizes the dangerous download label to a minimum width available using 2
  // lines.  The size is computed only the first time this method is invoked
  // and simply returned on subsequent calls.
  void SizeLabelToMinWidth();

  // Given a multiline |label|, decides whether it should be displayed on one
  // line (if short), or broken across two lines.  In the latter case,
  // linebreaks near the middle of the string and sets the label's text
  // accordingly.  Returns the preferred size for the label.
  static gfx::Size AdjustTextAndGetSize(views::Label* label);

  // Reenables the item after it has been disabled when a user clicked it to
  // open the downloaded file.
  void Reenable();

  // Releases drop down button after showing a context menu.
  void ReleaseDropdown();

  // Update the accessible name to reflect the current state of the control,
  // so that screenreaders can access the filename, status text, and
  // dangerous download warning message (if any). The name will be presented
  // when the download item receives focus.
  void UpdateAccessibleName();

  // Update accessible status text.
  // If |is_last_update| is false, then a timer is used to notify screen readers
  // to speak the alert text on a regular interval. If |is_last_update| is true,
  // then the screen reader is notified of the request to speak the alert
  // immediately, and any running timer is ended.
  void UpdateAccessibleAlert(const base::string16& alert, bool is_last_update);

  // Get the accessible alert text for a download that is currently in progress.
  base::string16 GetInProgressAccessibleAlertText();

  // Callback for |accessible_update_timer_|, or can be used to ask a screen
  // reader to speak the current alert immediately.
  void AnnounceAccessibleAlert();

  // Show/Hide/Reset |animation| based on the state transition specified by
  // |from| and |to|.
  void AnimateStateTransition(State from,
                              State to,
                              gfx::SlideAnimation* animation);

  // Callback for |progress_timer_|.
  void ProgressTimerFired();

  // Returns the base text color.
  SkColor GetTextColor() const;

  // Returns a slightly dimmed version of the base text color.
  SkColor GetDimmedTextColor() const;

  // Returns the status text to show in the notification.
  base::string16 GetStatusText() const;

  // The download shelf that owns us.
  DownloadShelfView* shelf_;

  // The focus ring for this Button.
  std::unique_ptr<views::FocusRing> focus_ring_;

  // Elements of our particular download
  base::string16 status_text_;

  // The font list used to print the file name and warning text.
  gfx::FontList font_list_;

  // The font list used to print the status text below the file name.
  gfx::FontList status_font_list_;

  // The tooltip.  Only displayed when not showing a warning dialog.
  base::string16 tooltip_text_;

  // The current state (normal, hot or pushed) of the body and drop-down.
  State dropdown_state_;

  // Mode of the download item view.
  Mode mode_;

  // When download progress last began animating (pausing and resuming will
  // update this). Used for downloads of unknown size.
  base::TimeTicks progress_start_time_;

  // Keeps the amount of time spent already animating. Used to keep track of
  // total active time for downloads of unknown size.
  base::TimeDelta previous_progress_elapsed_;

  // Whether we are dragging the download button.
  bool dragging_;

  // Whether we are tracking a possible drag.
  bool starting_drag_;

  // Position that a possible drag started at.
  gfx::Point drag_start_point_;

  // For canceling an in progress icon request.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // A model class to control the status text we display.
  DownloadUIModel::DownloadUIModelPtr model_;

  // Animation for download complete.
  std::unique_ptr<gfx::SlideAnimation> complete_animation_;

  // Progress animation
  base::RepeatingTimer progress_timer_;

  // Dangerous mode buttons.
  views::MdTextButton* save_button_;
  views::MdTextButton* discard_button_;

  // The drop down button.
  views::ImageButton* dropdown_button_;

  // Dangerous mode label.
  views::Label* dangerous_download_label_;

  // Whether the dangerous mode label has been sized yet.
  bool dangerous_download_label_sized_;

  // Whether we are currently disabled as part of opening the downloaded file.
  bool disabled_while_opening_;

  // The time at which this view was created.
  base::Time creation_time_;

  // The time at which a dangerous download warning was displayed.
  base::Time time_download_warning_shown_;

  // The currently running download context menu.
  std::unique_ptr<DownloadShelfContextMenuView> context_menu_;

  // The name of this view as reported to assistive technology.
  base::string16 accessible_name_;

  // A hidden view for accessible status alerts, that are spoken by screen
  // readers when a download changes state.
  views::View* accessible_alert_;

  // A timer for accessible alerts that helps reduce the number of similar
  // messages spoken in a short period of time.
  base::RepeatingTimer accessible_alert_timer_;

  // Force the reading of the current alert text the next time it updates.
  bool announce_accessible_alert_soon_;

  // The icon loaded in the download shelf is based on the file path of the
  // item.  Store the path used, so that we can detect a change in the path
  // and reload the icon.
  base::FilePath last_download_item_path_;

  // Method factory used to delay reenabling of the item when opening the
  // downloaded file.
  base::WeakPtrFactory<DownloadItemView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadItemView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_ITEM_VIEW_H_
