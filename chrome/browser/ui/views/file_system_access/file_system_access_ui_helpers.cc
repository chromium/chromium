// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_system_access/file_system_access_ui_helpers.h"

#include "base/files/file_path.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/styled_label.h"

namespace file_system_access_ui_helper {

std::unique_ptr<views::View> CreateOriginLabel(int message_id,
                                               const url::Origin& origin,
                                               int text_context,
                                               bool show_emphasis) {
  std::u16string formatted_origin =
      url_formatter::FormatOriginForSecurityDisplay(
          origin, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
  size_t offset;
  auto label = std::make_unique<views::StyledLabel>();
  label->SetText(
      l10n_util::GetStringFUTF16(message_id, formatted_origin, &offset));
  label->SetTextContext(text_context);
  label->SetDefaultTextStyle(show_emphasis ? views::style::STYLE_SECONDARY
                                           : views::style::STYLE_PRIMARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  if (show_emphasis) {
    views::StyledLabel::RangeStyleInfo origin_style;
    origin_style.text_style = STYLE_EMPHASIZED_SECONDARY;
    label->AddStyleRange(gfx::Range(offset, offset + formatted_origin.length()),
                         origin_style);
  }
  return label;
}

std::unique_ptr<views::View> CreateOriginPathLabel(int message_id,
                                                   const url::Origin& origin,
                                                   const base::FilePath& path,
                                                   int text_context,
                                                   bool show_emphasis) {
  std::u16string formatted_origin =
      url_formatter::FormatOriginForSecurityDisplay(
          origin, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
  std::u16string formatted_path = path.BaseName().LossyDisplayName();
  std::vector<size_t> offsets;
  auto label = std::make_unique<views::StyledLabel>();
  label->SetText(l10n_util::GetStringFUTF16(message_id, formatted_origin,
                                            formatted_path, &offsets));
  DCHECK_GE(offsets.size(), 2u);

  label->SetTextContext(text_context);
  label->SetDefaultTextStyle(show_emphasis ? views::style::STYLE_SECONDARY
                                           : views::style::STYLE_PRIMARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  if (show_emphasis) {
    views::StyledLabel::RangeStyleInfo origin_style;
    origin_style.text_style = STYLE_EMPHASIZED_SECONDARY;
    // All but the last offset should be the origin.
    for (size_t i = 0; i < offsets.size() - 1; ++i) {
      label->AddStyleRange(
          gfx::Range(offsets[i], offsets[i] + formatted_origin.length()),
          origin_style);
    }
  }

  views::StyledLabel::RangeStyleInfo path_style;
  if (show_emphasis)
    path_style.text_style = STYLE_EMPHASIZED_SECONDARY;
  path_style.tooltip = path.LossyDisplayName();
  label->AddStyleRange(
      gfx::Range(offsets.back(), offsets.back() + formatted_path.length()),
      path_style);

  return label;
}

}  // namespace file_system_access_ui_helper
