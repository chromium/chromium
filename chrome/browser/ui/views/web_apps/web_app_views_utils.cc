// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_views_utils.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/style/typography.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

constexpr int kMaxLinesForNameLabel = 2;

std::unique_ptr<views::Label> CreateNameLabel(const std::u16string& name) {
  auto name_label = std::make_unique<views::Label>(
      name, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::TextStyle::STYLE_PRIMARY);
  name_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  name_label->SetMultiLine(kMaxLinesForNameLabel > 1);
  name_label->SetMaxLines(kMaxLinesForNameLabel);
  name_label->SetElideBehavior(gfx::ELIDE_TAIL);
  return name_label;
}

std::unique_ptr<views::Label> CreateOriginLabelFromStartUrl(
    const GURL& start_url,
    bool is_primary_text) {
  auto origin_label = std::make_unique<views::Label>(
      url_formatter::FormatOriginForSecurityDisplay(
          url::Origin::Create(start_url),
          url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS),
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

std::unique_ptr<views::Label> CreateVersionLabel(const base::Version& version) {
  std::u16string version_u16 = base::UTF8ToUTF16(version.GetString());
  auto version_label = std::make_unique<views::Label>(
      l10n_util::GetStringFUTF16(
          IDS_IWA_INSTALLER_SHOW_METADATA_APP_VERSION_LABEL, version_u16),
      CONTEXT_DIALOG_BODY_TEXT_SMALL, views::style::STYLE_SECONDARY);
  version_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  version_label->SetMultiLine(false);
  return version_label;
}

std::unique_ptr<views::Label> CreateParentNameLabel(
    const std::u16string& name) {
  auto parent_app_label = std::make_unique<views::Label>(
      l10n_util::GetStringFUTF16(IDS_IWA_SUB_APPS_INSTALLER_PARENT_APP_NAME,
                                 name),
      CONTEXT_DIALOG_BODY_TEXT_SMALL, views::style::STYLE_SECONDARY);
  parent_app_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  parent_app_label->SetMultiLine(false);
  return parent_app_label;
}

}  // namespace web_app
