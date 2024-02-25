// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_views_utils.h"

#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/url_formatter/elide_url.h"
#include "url/gurl.h"

namespace web_app {

std::unique_ptr<views::Label> CreateNameLabel(const std::u16string& name) {
  auto name_label = std::make_unique<views::Label>(
      name, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::TextStyle::STYLE_PRIMARY);
  name_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  name_label->SetElideBehavior(gfx::ELIDE_TAIL);
  return name_label;
}

std::unique_ptr<views::Label> CreateOriginLabelFromStartUrl(
    const GURL& start_url,
    bool is_primary_text) {
  auto origin_label = std::make_unique<views::Label>(
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          start_url),
      CONTEXT_DIALOG_BODY_TEXT_SMALL,
      is_primary_text ? views::style::STYLE_PRIMARY
                      : views::style::STYLE_SECONDARY);

  origin_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Elide from head to prevent origin spoofing.
  origin_label->SetElideBehavior(gfx::ELIDE_HEAD);

  // Multiline breaks elision, so explicitly disable multiline.
  origin_label->SetMultiLine(false);

  return origin_label;
}

}  // namespace web_app
