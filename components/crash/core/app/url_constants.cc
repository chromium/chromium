// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/url_constants.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"

namespace crash_reporter {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(OFFICIAL_BUILD)
const char kDefaultUploadUrl[] = "https://clients2.google.com/cr/report";
#else
const char kDefaultUploadUrl[] = "";
#endif

}  // namespace crash_reporter
