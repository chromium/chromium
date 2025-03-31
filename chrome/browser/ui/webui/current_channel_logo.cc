// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/current_channel_logo.h"

#include "base/version_info/channel.h"
#include "chrome/common/channel_info.h"
#include "chrome/grit/theme_resources.h"

namespace webui {

int CurrentChannelLogoResourceId() {
  switch (chrome::GetChannel()) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case version_info::Channel::CANARY:
      return IDR_PRODUCT_LOGO_32_CANARY;
    case version_info::Channel::DEV:
      return IDR_PRODUCT_LOGO_32_DEV;
    case version_info::Channel::BETA:
      return IDR_PRODUCT_LOGO_32_BETA;
    case version_info::Channel::STABLE:
      return IDR_PRODUCT_LOGO_32;
#else
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
    case version_info::Channel::STABLE:
      NOTREACHED();
#endif
    case version_info::Channel::UNKNOWN:
      return IDR_PRODUCT_LOGO_32;
  }
  return -1;
}

}  // namespace webui
