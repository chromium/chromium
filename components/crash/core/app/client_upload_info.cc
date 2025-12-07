// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/client_upload_info.h"

#include "build/build_config.h"
#include "components/crash/core/app/crash_reporter_client.h"

#if BUILDFLAG(IS_WIN)
#include "components/crash/core/app/crash_export_thunks.h"
#endif

namespace crash_reporter {

bool GetClientCollectStatsConsent() {
#if BUILDFLAG(IS_WIN)
  // On Windows, the CrashReporterClient lives in chrome_elf.dll and needs to
  // be accessed via a thunk.
  return GetUploadConsent_ExportThunk();
#else
  return GetCrashReporterClient()->GetCollectStatsConsent();
#endif
}

void GetClientProductInfo(ProductInfo* product_info) {
#if BUILDFLAG(IS_WIN)
  // On Windows, the CrashReporterClient lives in chrome_elf.dll and needs to
  // be accessed via a thunk.
  GetProductInfo_ExportThunk(product_info);
#else
  GetCrashReporterClient()->GetProductInfo(product_info);
#endif
}

}  // namespace crash_reporter
