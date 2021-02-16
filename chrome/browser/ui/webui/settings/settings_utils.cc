// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_utils.h"

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/grit/theme_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/url_formatter/url_fixer.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font_list.h"
#include "url/gurl.h"

namespace settings_utils {

bool FixupAndValidateStartupPage(const std::string& url_string,
                                 GURL* fixed_url) {
  GURL url = url_formatter::FixupURL(url_string, std::string());
  bool valid = url.is_valid() && !extensions::ExtensionTabUtil::IsKillURL(url);
  if (valid && fixed_url)
    fixed_url->Swap(&url);
  return valid;
}

base::RefCountedMemory* GetFaviconResourceBytes(ui::ScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_SETTINGS_FAVICON, scale_factor);
}

base::RefCountedMemory* GetPrivacySandboxFaviconResourceBytes(
    ui::ScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_FLAGS_FAVICON, scale_factor);
}

std::string ResolveFontList(const std::string& font_name_or_list) {
  if (!font_name_or_list.empty() && font_name_or_list[0] == ',')
    return gfx::FontList::FirstAvailableOrFirst(font_name_or_list);
  return font_name_or_list;
}

#if !defined(OS_WIN)
std::string MaybeGetLocalizedFontName(const std::string& font_name_or_list) {
  return ResolveFontList(font_name_or_list);
}
#endif

}  // namespace settings_utils
