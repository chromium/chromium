// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace variations {

#if defined(OS_ANDROID)
void RecordFirstRunSeedImportResult(FirstRunSeedImportResult result) {
  UMA_HISTOGRAM_ENUMERATION("Variations.FirstRunResult", result,
                            FirstRunSeedImportResult::ENUM_SIZE);
}
#endif  // OS_ANDROID

void RecordLoadSeedResult(LoadSeedResult state) {
  base::UmaHistogramEnumeration("Variations.SeedLoadResult", state);
}

void RecordLoadSafeSeedResult(LoadSeedResult state) {
  base::UmaHistogramEnumeration("Variations.SafeMode.LoadSafeSeed.Result",
                                state);
}

void RecordStoreSeedResult(StoreSeedResult result) {
  UMA_HISTOGRAM_ENUMERATION("Variations.SeedStoreResult", result,
                            StoreSeedResult::ENUM_SIZE);
}

void ReportUnsupportedSeedFormatError() {
  RecordStoreSeedResult(StoreSeedResult::FAILED_UNSUPPORTED_SEED_FORMAT);
}

void RecordStoreSafeSeedResult(StoreSeedResult result) {
  UMA_HISTOGRAM_ENUMERATION("Variations.SafeMode.StoreSafeSeed.Result", result,
                            StoreSeedResult::ENUM_SIZE);
}

void RecordSeedInstanceManipulations(const InstanceManipulations& im) {
  if (im.delta_compressed && im.gzip_compressed) {
    RecordStoreSeedResult(StoreSeedResult::GZIP_DELTA_COUNT);
  } else if (im.delta_compressed) {
    RecordStoreSeedResult(StoreSeedResult::NON_GZIP_DELTA_COUNT);
  } else if (im.gzip_compressed) {
    RecordStoreSeedResult(StoreSeedResult::GZIP_FULL_COUNT);
  } else {
    RecordStoreSeedResult(StoreSeedResult::NON_GZIP_FULL_COUNT);
  }
}

}  // namespace variations
