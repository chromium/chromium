// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_ITEM_VIEW_H_

#include <stddef.h>

#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/icon_loader.h"
#include "chrome/browser/ui/download/download_item_mode.h"
#include "chrome/browser/ui/views/download/download_shelf_context_menu_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

class DownloadShelfView;

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class Canvas;
class Point;
class Rect;
}  // namespace gfx

namespace views {
class Button;
class ImageButton;
class Label;
class MdTextButton;
class StyledLabel;
}  // namespace views

// A view that implements one download on the Download shelf. Each
// DownloadItemView contains an application icon, a text label indicating the
// download's file name, a text label indicating the download's status (such as
// the number of bytes downloaded so far), and a button for canceling an
// in-progress download, or opening the completed download.
//
// The DownloadItemView lives in the Browser, and has a corresponding
// DownloadController that receives / writes data which lives in the Renderer.
class DownloadItemView : public views::View,
                         public views::ContextMenuController,
                         public DownloadUIModel::Delegate,
                         public views::AnimationDelegateViews {
  METADATA_HEADER(DownloadItemView, views::View)

 public:
  DownloadItemView(DownloadUIModel::DownloadUIModelPtr model,
                   DownloadShelfView* shelf,
                   views::View* accessible_alert);
  DownloadItemView(const DownloadItemView&) = delete;
  DownloadItemView& operator=(const DownloadItemView&) = delete;
  ~DownloadItemView() override;

  // views::View:
  void AddedToWidget() override;
  void Layout(PassKey) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  std::u16string GetTooltipText(const gfx::Point& p) const override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // DownloadUIModel::Delegate:
  void OnDownloadUpdated() override;
  void OnDownloadOpened() override;
  void OnDownloadDestroyed(const ContentId& id) override;

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // Returns the DownloadUIModel object belonging to this item.
  DownloadUIModel* model() { return model_.get(); }
  const DownloadUIModel* model() const { return model_.get(); }

  std::u16string GetStatusTextForTesting() const;
  void OpenItemForTesting();

 protected:
  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& /*available_size*/) const override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  // ui::LayerDelegate:
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

 private:
  class ContextMenuButton;

  // Sets the current mode to |mode| and updates UI appropriately.
  void SetMode(download::DownloadItemMode mode);
  download::DownloadItemMode GetMode() const;

  // Updates the file path, and if necessary, begins loading the file icon in
  // various sizes. This may eventually result in a callback to
  // OnFileIconLoaded().
  void UpdateFilePathAndIcons();

  // Begins loading the file icon in various sizes.
  void StartLoadIcons();

  // Updates the visibility, text, size, etc. of all labels.
  void UpdateLabels();

  // Updates the visible and enabled state of all buttons.
  void UpdateButtons();

  // Updates the accessible alert and animation-related state for normal mode.
  void UpdateAccessibleAlertAndAnimationsForNormalMode();

  // Update accessible status text, and announce it if desired.
  void UpdateAccessibleAlert(const std::u16string& alert);

  // Updates the animation used during deep scanning. The animation is started
  // or stopped depending on the current mode.
  void UpdateAnimationForDeepScanningMode();

  // Get the accessible alert text for a download that is currently in progress.
  std::u16string GetInProgressAccessibleAlertText() const;

  // Callback for |accessible_update_timer_|, or can be used to ask a screen
  // reader to speak the current alert immediately.
  void AnnounceAccessibleAlert();

  // Sets |file_icon_| to |icon|. Called when the icon manager has loaded the
  // normal-size icon for the current file path.
  void OnFileIconLoaded(IconLoader::IconSize icon_size, gfx::Image icon);

  // Paint the common download animation progress foreground and background. If
  // |percent_done| < 0, the total size is indeterminate.
  // |indeterminate_progress_time| is only used in that case.
  void PaintDownloadProgress(gfx::Canvas* canvas,
                             const gfx::RectF& bounds,
                             const base::TimeDelta& indeterminate_progress_time,
                             int percent_done) const;

  // When not in normal mode, returns the current help/warning/error icon.
  ui::ImageModel GetIcon() const;

  // When not in nromal mode, returns the bounds of the current icon.
  gfx::RectF GetIconBounds() const;

  // Returns the text and style to use for the status label.
  std::pair<std::u16string, int> GetStatusTextAndStyle() const;

  // Returns the size of any button visible next to the label (all visible
  // buttons are given the same size).
  gfx::Size GetButtonSize() const;

  // Returns the file name to report to the user. It might be elided to fit into
  // the text width. |label| dictates the default text style.
  std::u16string ElidedFilename(const views::Label& label) const;
  std::u16string ElidedFilename(const views::StyledLabel& label) const;

  // Returns the Y coordinate that centers |element_height| within the current
  // height().
  int CenterY(int element_height) const;

  // Returns either:
  //   * 200, if |label| can fit in one line given at most 200 DIP width.
  //   * The minimum width needed to display |label| on two lines.
  int GetLabelWidth(const views::StyledLabel& label) const;

  // Sets the state and triggers a repaint.
  void SetDropdownPressed(bool pressed);
  bool GetDropdownPressed() const;

  // Sets |dropdown_button_| to have the correct image for the current state.
  void UpdateDropdownButtonImage();

  // Called when various buttons are pressed.
  void OpenButtonPressed();
  void DropdownButtonPressed(const ui::Event& event);
  void ReviewButtonPressed();

  // Shows an appropriate prompt dialog when the user hits the "open" button
  // when not in normal mode.
  void ShowOpenDialog(content::WebContents* web_contents);

  // Shows the context menu at the specified location. |point| is in the view's
  // coordinate system.
  void ShowContextMenuImpl(const gfx::Rect& rect,
                           ui::MenuSourceType source_type);

  // Opens a file while async scanning is still pending.
  void OpenDownloadDuringAsyncScanning();

  // Forwards |command| to |commands_|; useful for callbacks.
  void ExecuteCommand(DownloadCommands::Command command);

  void UpdateAccessibleName();

  std::u16string CalculateAccessibleName() const;

  // The model controlling this object's state.
  const DownloadUIModel::DownloadUIModelPtr model_;

  // A utility object to help execute commands on the model.
  DownloadCommands commands_{model()->GetWeakPtr()};

  // The download shelf that owns us.
  const raw_ptr<DownloadShelfView> shelf_;

  // Mode of the download item view.
  download::DownloadItemMode mode_;

  // The "open download" button. This button is visually transparent and fills
  // the entire bounds of the DownloadItemView, to make the DownloadItemView
  // itself seem to be clickable while not requiring DownloadItemView itself to
  // be a button. This is necessary because buttons are not allowed to have
  // children in macOS Accessibility, and to avoid reimplementing much of the
  // button logic in DownloadItemView.
  raw_ptr<views::Button> open_button_;

  // Whether we are dragging the download button.
  bool dragging_ = false;

  // Position that a possible drag started at.
  std::optional<gfx::Point> drag_start_point_;

  gfx::ImageSkia file_icon_;

  // Tracks in-progress file icon loading tasks.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // |file_icon_| is based on the path of the downloaded item.  Store the path
  // used, so that we can detect a change in the path and reload the icon.
  base::FilePath file_path_;

  raw_ptr<views::Label> file_name_label_;
  raw_ptr<views::Label> status_label_;
  raw_ptr<views::StyledLabel> warning_label_;
  raw_ptr<views::StyledLabel> deep_scanning_label_;

  raw_ptr<views::MdTextButton> open_now_button_;
  raw_ptr<views::MdTextButton> save_button_;
  raw_ptr<views::MdTextButton> discard_button_;
  raw_ptr<views::MdTextButton> scan_button_;
  raw_ptr<views::MdTextButton> review_button_;
  raw_ptr<views::ImageButton> dropdown_button_;

  // Whether the dropdown is currently pressed.
  bool dropdown_pressed_ = false;

  DownloadShelfContextMenuView context_menu_{this};

  base::RepeatingTimer indeterminate_progress_timer_;

  // The start of the most recent active period of downloading a file of
  // indeterminate size.
  base::TimeTicks indeterminate_progress_start_time_;

  // The total active time downloading a file of indeterminate size.
  base::TimeDelta indeterminate_progress_time_elapsed_;

  gfx::SlideAnimation complete_animation_{this};

  gfx::ThrobAnimation scanning_animation_{this};

  // The tooltip.  Only displayed when not showing a warning dialog.
  std::u16string tooltip_text_;

  // A hidden view for accessible status alerts that are spoken by screen
  // readers when a download changes state.
  const raw_ptr<views::View> accessible_alert_;

  // A timer for accessible alerts that helps reduce the number of similar
  // messages spoken in a short period of time.
  base::RepeatingTimer accessible_alert_timer_;

  // Forces reading the current alert text the next time it updates.
  bool announce_accessible_alert_soon_ = false;

  float current_scale_;

  // Whether or not a histogram has been emitted recording that the dropdown
  // button shown.
  bool dropdown_button_shown_recorded_ = false;

  // Whether or not a histogram has been emitted recording that the dropdown
  // button was pressed.
  bool dropdown_button_pressed_recorded_ = false;

  // Whether the download's completion has already been logged. This is used to
  // avoid inaccurate repeated logging.
  bool has_download_completion_been_logged_ = false;

  // Method factory used to delay reenabling of the item when opening the
  // downloaded file.
  base::WeakPtrFactory<DownloadItemView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_ITEM_VIEW_H_
