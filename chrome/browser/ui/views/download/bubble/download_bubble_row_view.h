// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ROW_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/download/download_item_mode.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
class MdTextButton;
class ProgressBar;
class FlexLayoutView;
}  // namespace views

class DownloadShelfContextMenuView;
class DownloadBubbleUIController;

class DownloadBubbleRowView : public views::View,
                              public views::ContextMenuController,
                              public DownloadUIModel::Observer {
 public:
  METADATA_HEADER(DownloadBubbleRowView);

  explicit DownloadBubbleRowView(
      DownloadUIModel::DownloadUIModelPtr model,
      DownloadBubbleRowListView* row_list_view,
      DownloadBubbleUIController* bubble_controller,
      DownloadBubbleNavigationHandler* navigation_handler);
  DownloadBubbleRowView(const DownloadBubbleRowView&) = delete;
  DownloadBubbleRowView& operator=(const DownloadBubbleRowView&) = delete;
  ~DownloadBubbleRowView() override;

  // Overrides views::View:
  void AddedToWidget() override;
  void OnThemeChanged() override;
  void Layout() override;
  Views GetChildrenInZOrder() override;

  // Overrides DownloadUIModel::Observer:
  void OnDownloadOpened() override;
  void OnDownloadUpdated() override;
  void OnDownloadDestroyed() override;

  // Overrides views::ContextMenuController:
  void ShowContextMenuForViewImpl(View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  DownloadUIModel* model() { return model_.get(); }

  DownloadUIModel::BubbleUIInfo& ui_info() { return ui_info_; }

 protected:
  // Overrides ui::LayerDelegate:
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

 private:
  raw_ptr<views::MdTextButton> AddMainPageButton(
      DownloadCommands::Command command,
      const std::u16string& button_string);

  // If there is any change in state, update UI info.
  void UpdateBubbleUIInfo();
  void UpdateButtonsForItems();
  void UpdateProgressBar();
  void UpdateLabels();

  // Load the icon, from the cache or from IconManager::LoadIcon.
  void LoadIcon();

  // Called when icon has been loaded by IconManager::LoadIcon.
  void SetIconFromImage(gfx::Image icon);
  void SetIconFromImageModel(ui::ImageModel icon);

  void OnCancelButtonPressed();
  void OnDiscardButtonPressed();
  void OnMainButtonPressed();

  // TODO(bhatiarohit): Add platform-independent icons.
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
  raw_ptr<views::FlexLayoutView> main_button_holder_ = nullptr;

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

  download::DownloadItemMode mode_;
  download::DownloadItem::DownloadState state_;
  DownloadUIModel::BubbleUIInfo ui_info_;

  const gfx::VectorIcon* last_overriden_icon_ = nullptr;
  bool already_set_default_icon_ = false;

  // HoverButton for main button click and inkdrop animations.
  raw_ptr<HoverButton> hover_button_ = nullptr;

  base::WeakPtrFactory<DownloadBubbleRowView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ROW_VIEW_H_
