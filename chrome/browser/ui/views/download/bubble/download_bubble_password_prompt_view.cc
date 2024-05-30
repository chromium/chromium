// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_password_prompt_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view_class_properties.h"

DownloadBubblePasswordPromptView::DownloadBubblePasswordPromptView() {
  AddColumn(views::LayoutAlignment::kStart, views::LayoutAlignment::kCenter,
            0.0f, views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
  AddPaddingColumn(0.0f, ChromeLayoutProvider::Get()->GetDistanceMetric(
                             views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  AddColumn(views::LayoutAlignment::kStretch, views::LayoutAlignment::kStart,
            1.0f, views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
  AddRows(2, 1.0f);

  AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_PASSWORD_PROMPT)));
  password_field_ = AddChildView(std::make_unique<views::Textfield>());
  password_field_->SetTextInputType(
      ui::TextInputType::TEXT_INPUT_TYPE_PASSWORD);
  password_field_->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_DOWNLOAD_BUBBLE_PASSWORD_PROMPT_ACCESSIBLE_ALERT));

  // Dummy view for padding in the bottom-left
  AddChildView(std::make_unique<views::View>())->SetVisible(true);

  error_message_ = AddChildView(std::make_unique<views::Label>(
      std::u16string(), ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
      STYLE_RED));
  error_message_->SetProperty(views::kTableHorizAlignKey,
                              views::LayoutAlignment::kStart);
}

DownloadBubblePasswordPromptView::~DownloadBubblePasswordPromptView() = default;

void DownloadBubblePasswordPromptView::SetState(State state) {
  password_field_->SetText(std::u16string());
  password_field_->SetInvalid(IsError(state));
  password_field_->GetViewAccessibility().SetName(GetAccessibleName(state));
  error_message_->SetVisible(IsError(state));
  error_message_->SetText(GetErrorMessage(state));
  password_field_->RequestFocus();
}

const std::u16string& DownloadBubblePasswordPromptView::GetText() const {
  return password_field_->GetText();
}

bool DownloadBubblePasswordPromptView::IsError(State state) const {
  return state != State::kValid;
}

std::u16string DownloadBubblePasswordPromptView::GetErrorMessage(
    State state) const {
  switch (state) {
    case State::kValid:
      return std::u16string();
    case State::kInvalid:
      return l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_PASSWORD_PROMPT_INVALID);
    case State::kInvalidEmpty:
      return l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_PASSWORD_PROMPT_EMPTY);
  }
}

std::u16string DownloadBubblePasswordPromptView::GetAccessibleName(
    State state) const {
  switch (state) {
    case State::kValid:
      return l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_PASSWORD_PROMPT_ACCESSIBLE_ALERT);
    case State::kInvalid:
      return l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_PASSWORD_PROMPT_ACCESSIBLE_ALERT_INVALID);
    case State::kInvalidEmpty:
      return l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_BUBBLE_PASSWORD_PROMPT_ACCESSIBLE_ALERT_EMPTY);
  }
}

BEGIN_METADATA(DownloadBubblePasswordPromptView)
END_METADATA
