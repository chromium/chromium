// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/about_this_site_icon.h"

#include "build/branding_buildflags.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/vector_icons.h"

namespace page_info {

const gfx::VectorIcon& GetAboutThisSiteIcon() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return vector_icons::kPageInsightsIcon;
#else
  return features::IsRoundedIconsEnabled() ? views::kInfoIcon
                                           : views::kInfoChromeRefreshOldIcon;
#endif
}

}  // namespace page_info
