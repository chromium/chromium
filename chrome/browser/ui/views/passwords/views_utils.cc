// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/views_utils.h"

#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/password_manager/core/browser/password_form.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/styled_label.h"

std::unique_ptr<views::StyledLabel> CreateGooglePasswordManagerLabel(
    int text_message_id,
    int link_message_id,
    const std::u16string& email,
    base::RepeatingClosure open_link_closure,
    int context) {
  const std::u16string link = l10n_util::GetStringUTF16(link_message_id);

  std::vector<size_t> offsets;
  std::u16string text =
      l10n_util::GetStringFUTF16(text_message_id, link, email, &offsets);

  auto label = std::make_unique<views::StyledLabel>();
  label->SetText(text);
  label->SetTextContext(context);
  label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  label->AddStyleRange(
      gfx::Range(offsets.at(0), offsets.at(0) + link.length()),
      views::StyledLabel::RangeStyleInfo::CreateForLink(open_link_closure));

  return label;
}

std::unique_ptr<views::StyledLabel> CreateGooglePasswordManagerLabel(
    int text_message_id,
    int link_message_id,
    base::RepeatingClosure open_link_closure,
    int context) {
  const std::u16string link = l10n_util::GetStringUTF16(link_message_id);

  size_t link_offset;
  std::u16string text =
      l10n_util::GetStringFUTF16(text_message_id, link, &link_offset);

  auto label = std::make_unique<views::StyledLabel>();
  label->SetText(text);
  label->SetTextContext(context);
  label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  label->AddStyleRange(
      gfx::Range(link_offset, link_offset + link.length()),
      views::StyledLabel::RangeStyleInfo::CreateForLink(open_link_closure));

  return label;
}

std::unique_ptr<views::Label> CreateUsernameLabel(
    const password_manager::PasswordForm& form) {
  auto label = std::make_unique<views::Label>(
      GetDisplayUsername(form), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return label;
}

std::unique_ptr<views::Label> CreatePasswordLabel(
    const password_manager::PasswordForm& form) {
  std::unique_ptr<views::Label> label = std::make_unique<views::Label>(
      GetDisplayPassword(form), views::style::CONTEXT_DIALOG_BODY_TEXT);
  if (form.federation_origin.opaque()) {
    label->SetTextStyle(STYLE_SECONDARY_MONOSPACED);
    label->SetObscured(true);
    label->SetElideBehavior(gfx::TRUNCATE);
  } else {
    label->SetTextStyle(views::style::STYLE_SECONDARY);
    label->SetElideBehavior(gfx::ELIDE_HEAD);
  }
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return label;
}
