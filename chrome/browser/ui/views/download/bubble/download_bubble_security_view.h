// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_SECURITY_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_SECURITY_VIEW_H_

#include "chrome/browser/download/download_ui_model.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class MdTextButton;
class Checkbox;
}  // namespace views

class DownloadBubbleUIController;
class DownloadBubbleNavigationHandler;

class DownloadBubbleSecurityView : public views::View {
 public:
  METADATA_HEADER(DownloadBubbleSecurityView);
  DownloadBubbleSecurityView(
      DownloadUIModel::DownloadUIModelPtr model,
      DownloadUIModel::BubbleUIInfo info,
      DownloadBubbleUIController* bubble_controller,
      DownloadBubbleNavigationHandler* navigation_handler);
  DownloadBubbleSecurityView(const DownloadBubbleSecurityView&) = delete;
  DownloadBubbleSecurityView& operator=(const DownloadBubbleSecurityView&) =
      delete;
  ~DownloadBubbleSecurityView() override;

 private:
  void AddHeader();
  void CloseBubble();
  void OnCheckboxClicked();
  void AddIconAndText();
  void AddButtons();
  void ProcessButtonClick(DownloadCommands::Command command);

  DownloadUIModel::DownloadUIModelPtr model_;
  DownloadUIModel::BubbleUIInfo info_;
  raw_ptr<DownloadBubbleUIController> bubble_controller_ = nullptr;
  raw_ptr<DownloadBubbleNavigationHandler> navigation_handler_ = nullptr;
  raw_ptr<views::MdTextButton> first_button_ = nullptr;
  raw_ptr<views::Checkbox> checkbox_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_SECURITY_VIEW_H_
