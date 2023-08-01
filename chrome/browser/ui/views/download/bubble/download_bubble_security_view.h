// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_SECURITY_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_SECURITY_VIEW_H_

#include "base/gtest_prod_util.h"
#include "chrome/browser/download/download_ui_model.h"
#include "components/download/public/common/download_danger_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/view.h"

namespace views {
class Checkbox;
class Label;
class ImageView;
class StyledLabel;
class ImageButton;
class BubbleDialogDelegate;
class LabelButton;
}  // namespace views

class DownloadBubbleUIController;
class DownloadBubbleNavigationHandler;
class DownloadBubbleRowView;
class ParagraphsView;

class DownloadBubbleSecurityView : public views::View,
                                   public DownloadUIModel::Delegate {
 public:
  METADATA_HEADER(DownloadBubbleSecurityView);
  DownloadBubbleSecurityView(
      base::WeakPtr<DownloadBubbleUIController> bubble_controller,
      base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler,
      views::BubbleDialogDelegate* bubble_delegate);
  DownloadBubbleSecurityView(const DownloadBubbleSecurityView&) = delete;
  DownloadBubbleSecurityView& operator=(const DownloadBubbleSecurityView&) =
      delete;
  ~DownloadBubbleSecurityView() override;

  // Update the security view when a subpage is opened for a particular
  // download. If the argument is nullptr, this view will be reset to a default
  // state that is safe to destroy, and will no longer be initialized.
  void UpdateSecurityView(DownloadBubbleRowView* download_row_view);

  // Update the view after it is visible, in particular asking for focus and
  // announcing accessibility text.
  void UpdateAccessibilityTextAndFocus();

  // Whether this view is properly associated with a download row. Method calls
  // on this view do not make sense if not initialized.
  bool IsInitialized() const { return download_row_view_ != nullptr; }

 private:
  FRIEND_TEST_ALL_PREFIXES(DownloadBubbleSecurityViewTest,
                           VerifyLogWarningActions);

  // Following method calls require this view to be initialized.

  // Convenience for obtaining UI info from download_row_view_.
  DownloadUIModel::BubbleUIInfo& GetUiInfo();

  void BackButtonPressed();
  void AddHeader();
  void CloseBubble();
  void OnCheckboxClicked();
  void AddIconAndText();
  void AddSecondaryIconAndText();
  void AddProgressBar();

  void UpdateViews();
  void UpdateHeader();
  void UpdateIconAndText();
  void UpdateSecondaryIconAndText();
  // Updates the subpage button. Setting initial state and color for enabled
  // state, if it is a secondary button.
  void UpdateButton(DownloadUIModel::BubbleUIInfo::SubpageButton button,
                    bool is_secondary_button,
                    bool has_checkbox);
  void UpdateButtons();
  void UpdateProgressBar();

  // DownloadUIModel::Delegate implementation
  void OnDownloadUpdated() override;

  // Reset fields that increase the width of the bubble.
  void ClearWideFields();

  // |is_secondary_button| checks if the command/action originated from the
  // secondary button. Returns whether the dialog should close due to this
  // command.
  bool ProcessButtonClick(DownloadCommands::Command command,
                          bool is_secondary_button);
  void RecordWarningActionTime(bool is_secondary_button);

  int GetMinimumBubbleWidth() const;
  // Minimum width for the filename in the title.
  int GetMinimumTitleWidth() const;
  // Minimum width for the subpage summary.
  int GetMinimumLabelWidth() const;

  raw_ptr<DownloadBubbleRowView> download_row_view_ = nullptr;
  DownloadUIModel::DownloadUIModelPtr model_;
  base::WeakPtr<DownloadBubbleUIController> bubble_controller_ = nullptr;
  base::WeakPtr<DownloadBubbleNavigationHandler> navigation_handler_ = nullptr;
  raw_ptr<views::BubbleDialogDelegate, DanglingUntriaged> bubble_delegate_ =
      nullptr;
  // The secondary button is the one that may be protected by the checkbox.
  raw_ptr<views::LabelButton, DanglingUntriaged> secondary_button_ = nullptr;
  raw_ptr<views::Checkbox> checkbox_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<ParagraphsView> paragraphs_ = nullptr;
  raw_ptr<views::ImageView> secondary_icon_ = nullptr;
  raw_ptr<views::StyledLabel> secondary_styled_label_ = nullptr;
  raw_ptr<views::ImageButton> back_button_ = nullptr;
  // TODO(chlily): Implement deep_scanning_link_ as a learn_more_link_.
  raw_ptr<views::StyledLabel> deep_scanning_link_ = nullptr;
  raw_ptr<views::StyledLabel> learn_more_link_ = nullptr;
  raw_ptr<views::ProgressBar> progress_bar_ = nullptr;
  absl::optional<base::Time> warning_time_;
  bool did_log_action_ = false;

  download::DownloadDangerType cached_danger_type_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_SECURITY_VIEW_H_
