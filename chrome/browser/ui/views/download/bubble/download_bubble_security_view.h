// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_SECURITY_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_SECURITY_VIEW_H_

#include "chrome/browser/download/download_ui_model.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class MdTextButton;
class Checkbox;
class Label;
class ImageView;
class StyledLabel;
}  // namespace views

class DownloadBubbleUIController;
class DownloadBubbleNavigationHandler;
class DownloadBubbleRowView;

class DownloadBubbleSecurityView : public views::View {
 public:
  METADATA_HEADER(DownloadBubbleSecurityView);
  DownloadBubbleSecurityView(
      DownloadBubbleUIController* bubble_controller,
      DownloadBubbleNavigationHandler* navigation_handler);
  DownloadBubbleSecurityView(const DownloadBubbleSecurityView&) = delete;
  DownloadBubbleSecurityView& operator=(const DownloadBubbleSecurityView&) =
      delete;
  ~DownloadBubbleSecurityView() override;
  void UpdateSecurityView(DownloadBubbleRowView* download_row_view);

  raw_ptr<views::MdTextButton> keep_button_ = nullptr;
  raw_ptr<views::MdTextButton> discard_button_ = nullptr;
  raw_ptr<views::MdTextButton> bypass_deep_scan_button_ = nullptr;
  raw_ptr<views::MdTextButton> deep_scan_button_ = nullptr;

 private:
  void UpdateHeader();
  void AddHeader();
  void CloseBubble();
  void OnCheckboxClicked();
  void UpdateIconAndText();
  void AddIconAndText();
  void UpdateButtons();
  void AddButtons();
  void ProcessButtonClick(DownloadCommands::Command command,
                          bool is_first_button);
  views::MdTextButton* GetButtonForCommand(DownloadCommands::Command command);
  void RecordWarningActionTime(bool is_first_button);

  raw_ptr<DownloadBubbleRowView> download_row_view_;
  raw_ptr<DownloadBubbleUIController> bubble_controller_ = nullptr;
  raw_ptr<DownloadBubbleNavigationHandler> navigation_handler_ = nullptr;
  raw_ptr<views::MdTextButton> first_button_ = nullptr;
  raw_ptr<views::Checkbox> checkbox_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<views::StyledLabel> styled_label_ = nullptr;
  absl::optional<base::Time> warning_time_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_SECURITY_VIEW_H_
