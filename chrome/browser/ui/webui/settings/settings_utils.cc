// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_utils.h"

#include "chrome/browser/extensions/extension_tab_util.h"
#include "components/url_formatter/url_fixer.h"
#include "url/gurl.h"

namespace settings_utils {

bool FixupAndValidateStartupPage(const std::string& url_string,
                                 GURL* fixed_url) {
  GURL url = url_formatter::FixupURL(url_string);
  bool valid = url.is_valid() && !extensions::ExtensionTabUtil::IsKillURL(url);
  if (valid && fixed_url) {
    fixed_url->Swap(&url);
  }
  return valid;
}

}  // namespace settings_utils
