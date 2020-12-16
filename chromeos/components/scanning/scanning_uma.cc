// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/scanning/scanning_uma.h"

#include "base/metrics/histogram_functions.h"

namespace chromeos {
namespace scanning {

void RecordScanAppEntryPoint(ScanAppEntryPoint entry_point) {
  base::UmaHistogramEnumeration("Scanning.ScanAppEntryPoint", entry_point);
}

}  // namespace scanning
}  // namespace chromeos
