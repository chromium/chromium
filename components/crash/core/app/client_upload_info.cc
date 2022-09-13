// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/client_upload_info.h"

#include "build/build_config.h"
#include "components/crash/core/app/crash_reporter_client.h"

namespace crash_reporter {

bool GetClientCollectStatsConsent() {
  return GetCrashReporterClient()->GetCollectStatsConsent();
}

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
void GetClientProductNameAndVersion(std::string* product,
                                    std::string* version,
                                    std::string* channel) {
  GetCrashReporterClient()->GetProductNameAndVersion(product, version, channel);
}
#endif

}  // namespace crash_reporter
