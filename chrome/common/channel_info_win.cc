// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/channel_info.h"

#include "base/debug/profiler.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/install_static/install_util.h"

namespace chrome {

std::string GetChannelName() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  base::string16 channel(install_static::GetChromeChannelName());
#if defined(DCHECK_IS_CONFIGURABLE)
  // Adorn the channel when DCHECKs are baked into the build, as there will be
  // a performance hit. See https://crbug.com/812058 for details.
  channel += L"-dcheck";
#endif  // defined(DCHECK_IS_CONFIGURABLE)
  return base::UTF16ToASCII(channel);
#else
  return std::string();
#endif
}

version_info::Channel GetChannel() {
  return install_static::GetChromeChannel();
}

}  // namespace chrome
