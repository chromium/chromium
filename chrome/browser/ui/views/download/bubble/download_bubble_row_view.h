// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ROW_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_list_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
class MdTextButton;
class ProgressBar;
}  // namespace views

class DownloadShelfContextMenuView;
class DownloadBubbleUIController;

class DownloadBubbleRowView : public views::View,
                              public views::ContextMenuController,
                              public DownloadUIModel::Observer {
 public:
  METADATA_HEADER(DownloadBubbleRowView);

  explicit DownloadBubbleRowView(DownloadUIModel::DownloadUIModelPtr model,
                                 DownloadBubbleRowListView* row_list_view,
                                 DownloadBubbleUIController* bubble_controller);
  DownloadBubbleRowView(const DownloadBubbleRowView&) = delete;
  DownloadBubbleRowView& operator=(const DownloadBubbleRowView&) = delete;
  ~DownloadBubbleRowView() override;
  // Overrides views::View:
  void AddedToWidget() override;

  // Overrides DownloadUIModel::Observer:
  void OnDownloadOpened() override;
  void OnDownloadUpdated() override;
  void OnDownloadDestroyed() override;

  // Overrides views::ContextMenuController:
  void ShowContextMenuForViewImpl(View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

 protected:
  // Overrides ui::LayerDelegate:
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

 private:
  // Load the icon, from the cache or from IconManager::LoadIcon.
  void LoadIcon();

  // Called when icon has been loaded by IconManager::LoadIcon.
  void SetIcon(gfx::Image icon);

  // Called when cancel button is pressed for an in progress download.
  void OnCancelButtonPressed();

  // TODO(bhatiarohit): Add platform-independent icons.
  // The icon for the file. We get platform-specific icons from IconLoader.
  raw_ptr<views::ImageView> icon_ = nullptr;

  // The primary label.
  raw_ptr<views::Label> primary_label_ = nullptr;

  // The secondary label.
  raw_ptr<views::Label> secondary_label_ = nullptr;

  // The cancel button for in-progress downloads.
  raw_ptr<views::MdTextButton> cancel_button_ = nullptr;

  // The progress bar for in-progress downloads.
  raw_ptr<views::ProgressBar> progress_bar_ = nullptr;

  // Device scale factor, used to load icons.
  float current_scale_ = 1.0f;

  // Tracks tasks requesting file icons.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // The model controlling this object's state.
  const DownloadUIModel::DownloadUIModelPtr model_;

  // Reuse the download shelf context menu in the bubble.
  std::unique_ptr<DownloadShelfContextMenuView> context_menu_;

  // Parent row list view.
  raw_ptr<DownloadBubbleRowListView> row_list_view_ = nullptr;

  // Controller for keeping track of downloads.
  raw_ptr<DownloadBubbleUIController> bubble_controller_ = nullptr;

  base::WeakPtrFactory<DownloadBubbleRowView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ROW_VIEW_H_
