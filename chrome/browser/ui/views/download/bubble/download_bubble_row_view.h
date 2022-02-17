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
#include "ui/views/view.h"

namespace views {
class ImageView;
}  // namespace views

class DownloadBubbleRowView : public views::View {
 public:
  METADATA_HEADER(DownloadBubbleRowView);

  explicit DownloadBubbleRowView(DownloadUIModel::DownloadUIModelPtr model);
  DownloadBubbleRowView(const DownloadBubbleRowView&) = delete;
  DownloadBubbleRowView& operator=(const DownloadBubbleRowView&) = delete;
  ~DownloadBubbleRowView() override;
  // Overrides views::View:
  void AddedToWidget() override;

 protected:
  // Overrides ui::LayerDelegate:
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

 private:
  // Load the icon, from the cache or from IconManager::LoadIcon.
  void LoadIcon();

  // Called when icon has been loaded by IconManager::LoadIcon.
  void SetIcon(gfx::Image icon);

  // TODO(bhatiarohit): Add platform-independent icons.
  // The icon for the file. We get platform-specific icons from IconLoader.
  raw_ptr<views::ImageView> icon_ = nullptr;

  // Device scale factor, used to load icons.
  float current_scale_ = 1.0f;

  // Tracks tasks requesting file icons.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // The model controlling this object's state.
  const DownloadUIModel::DownloadUIModelPtr model_;

  base::WeakPtrFactory<DownloadBubbleRowView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_ROW_VIEW_H_
