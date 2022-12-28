// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ROW_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/download/download_item_mode.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
class MdTextButton;
class ImageButton;
class ProgressBar;
class FlexLayoutView;
class InkDropContainerView;
}  // namespace views

class DownloadShelfContextMenuView;
class DownloadBubbleUIController;

class DownloadBubbleRowView : public views::View,
                              public views::ContextMenuController,
                              public DownloadUIModel::Delegate,
                              public views::FocusChangeListener {
 public:
  METADATA_HEADER(DownloadBubbleRowView);

  explicit DownloadBubbleRowView(
      DownloadUIModel::DownloadUIModelPtr model,
      DownloadBubbleRowListView* row_list_view,
      DownloadBubbleUIController* bubble_controller,
      DownloadBubbleNavigationHandler* navigation_handler,
      Browser* browser);
  DownloadBubbleRowView(const DownloadBubbleRowView&) = delete;
  DownloadBubbleRowView& operator=(const DownloadBubbleRowView&) = delete;
  ~DownloadBubbleRowView() override;

  // Overrides views::View:
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnThemeChanged() override;
  void Layout() override;
  Views GetChildrenInZOrder() override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  gfx::Size CalculatePreferredSize() const override;
  void AddLayerBeneathView(ui::Layer* layer) override;
  void RemoveLayerBeneathView(ui::Layer* layer) override;

  // Overrides views::FocusChangeListener
  void OnWillChangeFocus(views::View* before, views::View* now) override;
  void OnDidChangeFocus(views::View* before, views::View* now) override {}

  // Update the row and its elements for hover and focus events.
  void UpdateRowForHover(bool hovered);
  void UpdateRowForFocus(bool visible, bool request_focus_on_last_quick_action);

  // Overrides DownloadUIModel::Delegate:
  void OnDownloadOpened() override;
  void OnDownloadUpdated() override;
  void OnDownloadDestroyed(const ContentId& id) override;

  // Overrides views::ContextMenuController:
  void ShowContextMenuForViewImpl(View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // Overrides ui::AcceleratorTarget
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  DownloadUIModel* model() { return model_.get(); }

  DownloadUIModel::BubbleUIInfo& ui_info() { return ui_info_; }
  void SetUIInfoForTesting(DownloadUIModel::BubbleUIInfo ui_info) {
    ui_info_ = ui_info;
  }

 protected:
  // Overrides ui::LayerDelegate:
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

 private:
  views::MdTextButton* AddMainPageButton(DownloadCommands::Command command,
                                         const std::u16string& button_string);
  views::ImageButton* AddQuickAction(DownloadCommands::Command command);
  views::ImageButton* GetActionButtonForCommand(
      DownloadCommands::Command command);
  std::u16string GetAccessibleNameForQuickAction(
      DownloadCommands::Command command);
  views::MdTextButton* GetMainPageButton(DownloadCommands::Command command);
  std::u16string GetAccessibleNameForMainPageButton(
      DownloadCommands::Command command);

  // If there is any change in state, update UI info.
  // Returns whether the ui info was changed.
  bool UpdateBubbleUIInfo(bool initial_setup);

  // Update the DownloadBubbleRowView's members.
  void UpdateRow(bool initial_setup);

  void UpdateButtons();
  void UpdateProgressBar();
  void UpdateLabels();
  void RecordMetricsOnUpdate();
  void RecordDownloadDisplayed();

  // Load the icon, from the cache or from IconManager::LoadIcon.
  void LoadIcon();

  // Called when icon has been loaded by IconManager::LoadIcon.
  // |use_over_last_override| controls whether icon should be set if
  // the current icon is an override_icon. |load_start_time| is the time when
  // the calling LoadIcon() started, and is recorded for metrics.
  void SetIconFromImage(bool use_over_last_override,
                        base::Time load_start_time,
                        gfx::Image icon);
  void SetIconFromImageModel(bool use_over_last_override,
                             base::Time load_start_time,
                             const ui::ImageModel& icon);

  void OnCancelButtonPressed();
  void OnDiscardButtonPressed();
  void OnMainButtonPressed();

  void AnnounceInProgressAlert();

  // Registers/unregisters copy accelerator for copy/paste support.
  void RegisterAccelerators(views::FocusManager* focus_manager);
  void UnregisterAccelerators(views::FocusManager* focus_manager);

  // The icon for the file. We get platform-specific icons from IconLoader.
  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<views::ImageView> subpage_icon_ = nullptr;
  raw_ptr<views::FlexLayoutView> subpage_icon_holder_ = nullptr;

  // The primary label.
  raw_ptr<views::Label> primary_label_ = nullptr;

  // The secondary label.
  raw_ptr<views::Label> secondary_label_ = nullptr;

  // Buttons on the main page.
  raw_ptr<views::MdTextButton> cancel_button_ = nullptr;
  raw_ptr<views::MdTextButton> discard_button_ = nullptr;
  raw_ptr<views::MdTextButton> keep_button_ = nullptr;
  raw_ptr<views::MdTextButton> scan_button_ = nullptr;
  raw_ptr<views::MdTextButton> open_now_button_ = nullptr;
  raw_ptr<views::MdTextButton> resume_button_ = nullptr;
  raw_ptr<views::MdTextButton> review_button_ = nullptr;
  raw_ptr<views::MdTextButton> retry_button_ = nullptr;

  // Quick Actions on the main page.
  raw_ptr<views::ImageButton> resume_action_ = nullptr;
  raw_ptr<views::ImageButton> pause_action_ = nullptr;
  raw_ptr<views::ImageButton> show_in_folder_action_ = nullptr;
  raw_ptr<views::ImageButton> cancel_action_ = nullptr;
  raw_ptr<views::ImageButton> open_when_complete_action_ = nullptr;

  // Holder for the main button.
  raw_ptr<views::FlexLayoutView> main_button_holder_ = nullptr;
  // Holder for the quick actions.
  raw_ptr<views::FlexLayoutView> quick_action_holder_ = nullptr;

  // The progress bar for in-progress downloads.
  raw_ptr<views::ProgressBar> progress_bar_ = nullptr;
  raw_ptr<views::FlexLayoutView> progress_bar_holder_ = nullptr;

  // Device scale factor, used to load icons.
  float current_scale_ = 1.0f;

  // Tracks tasks requesting file icons.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // The model controlling this object's state.
  DownloadUIModel::DownloadUIModelPtr model_;

  // Reuse the download shelf context menu in the bubble.
  std::unique_ptr<DownloadShelfContextMenuView> context_menu_;

  // Parent row list view.
  raw_ptr<DownloadBubbleRowListView> row_list_view_ = nullptr;

  // Controller for keeping track of downloads.
  raw_ptr<DownloadBubbleUIController> bubble_controller_ = nullptr;

  raw_ptr<DownloadBubbleNavigationHandler> navigation_handler_ = nullptr;

  raw_ptr<Browser> browser_ = nullptr;

  download::DownloadItemMode mode_;
  download::DownloadItem::DownloadState state_;
  DownloadUIModel::BubbleUIInfo ui_info_;
  bool is_paused_;

  raw_ptr<const gfx::VectorIcon> last_overriden_icon_ = nullptr;
  bool already_set_default_icon_ = false;

  // Button for transparent button click, inkdrop animations and drag and drop
  // events.
  raw_ptr<views::Button> transparent_button_ = nullptr;

  raw_ptr<views::InkDropContainerView> inkdrop_container_;

  // Drag and drop:
  // Whether we are dragging the download bubble row.
  bool dragging_ = false;
  // Position that a possible drag started at.
  absl::optional<gfx::Point> drag_start_point_;

  // Whether the download's completion has already been logged. This is used to
  // avoid inaccurate repeated logging.
  bool has_download_completion_been_logged_ = false;

  // A timer for accessible alerts of progress updates
  base::RepeatingTimer accessible_alert_in_progress_timer_;

  base::WeakPtrFactory<DownloadBubbleRowView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ROW_VIEW_H_
