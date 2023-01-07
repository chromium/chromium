// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/channel_info.h"

#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/install_static/install_util.h"

namespace chrome {

std::string GetChannelName(WithExtendedStable with_extended_stable) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return base::WideToASCII(
      install_static::GetChromeChannelName(with_extended_stable.value()));
#else
  return std::string();
#endif
}

version_info::Channel GetChannel() {
  return install_static::GetChromeChannel();
}

bool IsExtendedStableChannel() {
  return install_static::IsExtendedStableChannel();
}

}  // namespace chrome
