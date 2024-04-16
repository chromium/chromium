// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"

namespace variations {

#if BUILDFLAG(IS_ANDROID)
void RecordFirstRunSeedImportResult(FirstRunSeedImportResult result) {
  UMA_HISTOGRAM_ENUMERATION("Variations.FirstRunResult", result,
                            FirstRunSeedImportResult::ENUM_SIZE);
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
void RecordFirstRunSeedImportResult(FirstRunSeedImportResult result) {
  // TODO(crbug.com/40235387): Merge with Android implementation after first run
  // seed import on iOS is fully implemented.
}
#endif  // BUILDFLAG(IS_IOS)

void RecordLoadSeedResult(LoadSeedResult state) {
  base::UmaHistogramEnumeration("Variations.SeedLoadResult", state);
}

void RecordLoadSafeSeedResult(LoadSeedResult state) {
  base::UmaHistogramEnumeration("Variations.SafeMode.LoadSafeSeed.Result",
                                state);
}

void RecordStoreSeedResult(StoreSeedResult result) {
  base::UmaHistogramEnumeration("Variations.SeedStoreResult", result);
}

void ReportUnsupportedSeedFormatError() {
  RecordStoreSeedResult(StoreSeedResult::kFailedUnsupportedSeedFormat);
}

void RecordStoreSafeSeedResult(StoreSeedResult result) {
  base::UmaHistogramEnumeration("Variations.SafeMode.StoreSafeSeed.Result",
                                result);
}

void RecordSeedInstanceManipulations(const InstanceManipulations& im) {
  if (im.delta_compressed && im.gzip_compressed) {
    RecordStoreSeedResult(StoreSeedResult::kGzipDeltaCount);
  } else if (im.delta_compressed) {
    RecordStoreSeedResult(StoreSeedResult::kNonGzipDeltaCount);
  } else if (im.gzip_compressed) {
    RecordStoreSeedResult(StoreSeedResult::kGzipFullCount);
  } else {
    RecordStoreSeedResult(StoreSeedResult::kNonGzipFullCount);
  }
}

}  // namespace variations
