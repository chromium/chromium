// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ash/system_web_apps/color_helpers.h"
#include "chrome/browser/ui/webui/theme_source.h"

namespace ash {
// Implementation of GetThemeSource from
// chrome/browser/ash/system_web_apps/color_helpers.h. Implemented here since
// it needs access to chrome/browser/ui which chrome/browser/ash is disallowed
// from depending on directly.
std::unique_ptr<content::URLDataSource> GetThemeSource(Profile* profile,
                                                       bool untrusted) {
  return std::make_unique<ThemeSource>(profile, untrusted);
}

}  // namespace ash
