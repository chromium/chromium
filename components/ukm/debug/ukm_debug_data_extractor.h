// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_DEBUG_UKM_DEBUG_DATA_EXTRACTOR_H_
#define COMPONENTS_UKM_DEBUG_UKM_DEBUG_DATA_EXTRACTOR_H_

#include "base/values.h"
#include "components/ukm/ukm_service.h"

namespace ukm {

class UkmService;

namespace debug {

// Extracts UKM data as an HTML page for debugging purposes.
class UkmDebugDataExtractor {
 public:
  UkmDebugDataExtractor();

  UkmDebugDataExtractor(const UkmDebugDataExtractor&) = delete;
  UkmDebugDataExtractor& operator=(const UkmDebugDataExtractor&) = delete;

  ~UkmDebugDataExtractor();

  // Returns UKM data structured in a dictionary.
  static base::Value GetStructuredData(const UkmService* ukm_service);

  // Convert uint64 to pair of int32 to match the spec of Value. JS doesn't
  // support uint64 while most of UKM metrics are 64 bit numbers. So,
  // they will be passed as a pair of 32 bit ints. The first item is the
  // 32 bit representation of the high 32 bit and the second item is the lower
  // 32 bit of the 64 bit number.
  static base::Value UInt64AsPairOfInt(uint64_t v);
};

}  // namespace debug
}  // namespace ukm

#endif  // COMPONENTS_UKM_DEBUG_UKM_DEBUG_DATA_EXTRACTOR_H_
