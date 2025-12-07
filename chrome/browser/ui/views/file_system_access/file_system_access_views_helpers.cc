// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_system_access/file_system_access_views_helpers.h"

#include <string>

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/file_system_access/file_system_access_ui_helpers.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"

namespace file_system_access_ui_helper {

std::unique_ptr<views::View> CreateOriginLabel(
    content::WebContents* web_contents,
    int message_id,
    const url::Origin& origin,
    int text_context,
    bool show_emphasis) {
  Profile* profile =
      web_contents
          ? Profile::FromBrowserContext(web_contents->GetBrowserContext())
          : nullptr;
  std::u16string origin_identity_name =
      file_system_access_ui_helper::GetUrlIdentityName(profile,
                                                       origin.GetURL());

  size_t offset;
  auto label = std::make_unique<views::StyledLabel>();
  label->SetText(
      l10n_util::GetStringFUTF16(message_id, origin_identity_name, &offset));
  label->SetTextContext(text_context);
  label->SetDefaultTextStyle(show_emphasis ? views::style::STYLE_SECONDARY
                                           : views::style::STYLE_PRIMARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  if (show_emphasis) {
    views::StyledLabel::RangeStyleInfo origin_style;
    origin_style.text_style = views::style::STYLE_EMPHASIZED;
    label->AddStyleRange(
        gfx::Range(offset, offset + origin_identity_name.length()),
        origin_style);
  }
  return label;
}

std::unique_ptr<views::View> CreateOriginPathLabel(
    content::WebContents* web_contents,
    int message_id,
    const url::Origin& origin,
    const base::FilePath& path,
    int text_context,
    bool show_emphasis) {
  std::u16string formatted_path =
      GetPathForDisplayAsParagraph(content::PathInfo(path));

  Profile* profile =
      web_contents
          ? Profile::FromBrowserContext(web_contents->GetBrowserContext())
          : nullptr;
  std::u16string origin_identity_name =
      file_system_access_ui_helper::GetUrlIdentityName(profile,
                                                       origin.GetURL());

  std::vector<size_t> offsets;
  auto label = std::make_unique<views::StyledLabel>();
  label->SetText(l10n_util::GetStringFUTF16(message_id, origin_identity_name,
                                            formatted_path, &offsets));
  DCHECK_GE(offsets.size(), 2u);

  label->SetTextContext(text_context);
  label->SetDefaultTextStyle(show_emphasis ? views::style::STYLE_SECONDARY
                                           : views::style::STYLE_PRIMARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  if (show_emphasis) {
    views::StyledLabel::RangeStyleInfo origin_style;
    origin_style.text_style = views::style::STYLE_EMPHASIZED;
    // All but the last offset should be the origin.
    for (size_t i = 0; i < offsets.size() - 1; ++i) {
      label->AddStyleRange(
          gfx::Range(offsets[i], offsets[i] + origin_identity_name.length()),
          origin_style);
    }
  }

  views::StyledLabel::RangeStyleInfo path_style;
  if (show_emphasis) {
    path_style.text_style = views::style::STYLE_EMPHASIZED;
  }
  path_style.tooltip = path.LossyDisplayName();
  label->AddStyleRange(
      gfx::Range(offsets.back(), offsets.back() + formatted_path.length()),
      path_style);

  return label;
}

}  // namespace file_system_access_ui_helper
