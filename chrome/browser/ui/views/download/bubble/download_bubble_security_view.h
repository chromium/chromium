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
class ImageButton;
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

  // Update the security view when a subpage is opened for a particular
  // download.
  void UpdateSecurityView(DownloadBubbleRowView* download_row_view);

  // Update the view after it is visible, in particular asking for focus and
  // announcing accessibility text.
  void UpdateAccessibilityTextAndFocus();

  raw_ptr<views::MdTextButton> keep_button_ = nullptr;
  raw_ptr<views::MdTextButton> discard_button_ = nullptr;
  raw_ptr<views::MdTextButton> bypass_deep_scan_button_ = nullptr;
  raw_ptr<views::MdTextButton> deep_scan_button_ = nullptr;

 private:
  void BackButtonPressed();
  void UpdateHeader();
  void AddHeader();
  void CloseBubble();
  void OnCheckboxClicked();
  void UpdateIconAndText();
  void AddIconAndText();
  // Updates the subpage button. Setting initial state and color for enabled
  // state, if it is a secondary button.
  void UpdateButton(DownloadUIModel::BubbleUIInfo::SubpageButton button,
                    bool is_secondary_button,
                    bool has_checkbox,
                    SkColor color);
  void UpdateButtons();
  void AddButtons();

  // |is_secondary_button| checks if the command/action originated from the
  // secondary button.
  void ProcessButtonClick(DownloadCommands::Command command,
                          bool is_secondary_button);
  views::MdTextButton* GetButtonForCommand(DownloadCommands::Command command);
  void RecordWarningActionTime(bool is_secondary_button);

  raw_ptr<DownloadBubbleRowView> download_row_view_;
  raw_ptr<DownloadBubbleUIController> bubble_controller_ = nullptr;
  raw_ptr<DownloadBubbleNavigationHandler> navigation_handler_ = nullptr;
  // The secondary button is the one that may be protected by the checkbox.
  raw_ptr<views::MdTextButton> secondary_button_ = nullptr;
  raw_ptr<views::Checkbox> checkbox_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<views::StyledLabel> styled_label_ = nullptr;
  raw_ptr<views::ImageButton> back_button_ = nullptr;
  absl::optional<base::Time> warning_time_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_SECURITY_VIEW_H_
