// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/views_utils.h"

#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/view.h"

std::unique_ptr<views::View> CreateGooglePasswordManagerFooterView(
    const std::u16string& email,
    base::RepeatingClosure open_google_password_manager_closure) {
  const std::u16string link = l10n_util::GetStringUTF16(
      IDS_PASSWORD_BUBBLES_GOOGLE_PASSWORD_MANAGER_LINK_TEXT);
  std::vector<size_t> offsets;
  const std::u16string text = l10n_util::GetStringFUTF16(
      IDS_PASSWORD_BUBBLES_SIGNED_IN_FOOTER, link, email, &offsets);

  auto label = std::make_unique<views::StyledLabel>();
  label->SetText(text);
  label->SetTextContext(CONTEXT_DIALOG_BODY_TEXT_SMALL);
  label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  label->AddStyleRange(gfx::Range(offsets.at(0), offsets.at(0) + link.length()),
                       views::StyledLabel::RangeStyleInfo::CreateForLink(
                           open_google_password_manager_closure));

  return label;
}
