// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SCANNING_SCANNING_UMA_H_
#define CHROMEOS_COMPONENTS_SCANNING_SCANNING_UMA_H_

namespace chromeos {
namespace scanning {

// The enums below are used in histograms, do not remove/renumber entries. If
// you're adding to any of these enums, update the corresponding enum listing in
// tools/metrics/histograms/enums.xml.

enum class ScanAppEntryPoint {
  kSettings = 0,
  kLauncher = 1,
  kMaxValue = kLauncher,
};

enum class ScanJobFailureReason {
  kUnknownScannerError = 0,
  kScannerNotFound = 1,
  kUnsupportedScanToPath = 2,
  kSaveToDiskFailed = 3,
  kMaxValue = kSaveToDiskFailed,
};

enum class ScanJobSettingsResolution {
  kUnexpectedDpi = 0,
  k75Dpi = 1,
  k100Dpi = 2,
  k150Dpi = 3,
  k200Dpi = 4,
  k300Dpi = 5,
  k600Dpi = 6,
  kMaxValue = k600Dpi,
};

// Records ScanAppEntryPoint histogram value.
void RecordScanAppEntryPoint(ScanAppEntryPoint entry_point);

// Converts resolution integer value to a ScanJobSettingsResolution enum value.
ScanJobSettingsResolution GetResolutionEnumValue(const int resolution);

}  // namespace scanning
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SCANNING_SCANNING_UMA_H_
