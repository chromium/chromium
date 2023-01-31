// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_system_access/file_system_access_ui_helpers.h"

#include <string>

#include "base/files/file_path.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"

namespace {

base::FilePath GetPathForDisplayAsPath(const base::FilePath& path) {
  // Display the drive letter if the path is the root of the filesystem.
  auto dir_name = path.DirName();
  if (!path.empty() && (dir_name.empty() || path == dir_name)) {
    return path;
  }

  return path.BaseName();
}

}  // namespace

namespace file_system_access_ui_helper {

std::unique_ptr<views::View> CreateOriginLabel(Browser* browser,
                                               int message_id,
                                               const url::Origin& origin,
                                               int text_context,
                                               bool show_emphasis) {
  std::u16string origin_or_short_name =
      GetFormattedOriginOrAppShortName(browser, origin);
  size_t offset;
  auto label = std::make_unique<views::StyledLabel>();
  label->SetText(
      l10n_util::GetStringFUTF16(message_id, origin_or_short_name, &offset));
  label->SetTextContext(text_context);
  label->SetDefaultTextStyle(show_emphasis ? views::style::STYLE_SECONDARY
                                           : views::style::STYLE_PRIMARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  if (show_emphasis) {
    views::StyledLabel::RangeStyleInfo origin_style;
    origin_style.text_style = views::style::STYLE_EMPHASIZED;
    label->AddStyleRange(
        gfx::Range(offset, offset + origin_or_short_name.length()),
        origin_style);
  }
  return label;
}

std::unique_ptr<views::View> CreateOriginPathLabel(Browser* browser,
                                                   int message_id,
                                                   const url::Origin& origin,
                                                   const base::FilePath& path,
                                                   int text_context,
                                                   bool show_emphasis) {
  std::u16string formatted_path = GetPathForDisplayAsParagraph(path);
  std::u16string origin_or_short_name =
      GetFormattedOriginOrAppShortName(browser, origin);
  std::vector<size_t> offsets;
  auto label = std::make_unique<views::StyledLabel>();
  label->SetText(l10n_util::GetStringFUTF16(message_id, origin_or_short_name,
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
          gfx::Range(offsets[i], offsets[i] + origin_or_short_name.length()),
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

std::u16string GetElidedPathForDisplayAsTitle(const base::FilePath& path) {
  // TODO(crbug.com/1411723): Consider moving filename elision logic into a core
  // component, which would allow for dynamic elision based on the _actual_
  // available pixel width and font of the dialog.
  //
  // Ensure file names containing spaces won't overflow to the next line in the
  // title of a permission prompt, which is very hard to read. File names not
  // containing a space will bump to the next line if the file name + preceding
  // text in the title is too long, which is still easy to read because the file
  // name is contiguous.
  int scalar_quarters = base::Contains(GetPathForDisplayAsPath(path).value(),
                                       FILE_PATH_LITERAL(" "))
                            ? 2
                            : 3;
  // views::LayoutProvider::Get() may be null in tests.
  int available_pixel_width =
      (views::LayoutProvider::Get()
           ? views::LayoutProvider::Get()->GetDistanceMetric(
                 views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH)
           : 400) *
      scalar_quarters / 4;
  return gfx::ElideFilename(GetPathForDisplayAsPath(path), gfx::FontList(),
                            available_pixel_width);
}

std::u16string GetPathForDisplayAsParagraph(const base::FilePath& path) {
  // Paragraph text will wrap to the next line rather than overflow, so there's
  // no need to elide the file name.
  return GetPathForDisplayAsPath(path).LossyDisplayName();
}

std::u16string GetFormattedOriginOrAppShortName(Browser* browser,
                                                const url::Origin& origin) {
  bool is_isolated_web_app = browser && browser->app_controller() &&
                             browser->app_controller()->IsIsolatedWebApp();
  return is_isolated_web_app
             ? browser->app_controller()->GetAppShortName()
             : url_formatter::FormatOriginForSecurityDisplay(
                   origin, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
}

}  // namespace file_system_access_ui_helper
