// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SCANNING_SCANNING_UMA_H_
#define CHROMEOS_COMPONENTS_SCANNING_SCANNING_UMA_H_

namespace chromeos {
namespace scanning {

// These are used in histograms, do not remove/renumber entries. If you're
// adding to this enum with the intention that it will be logged, update the
// ScanAppEntryPoint enum listing in
// tools/metrics/histograms/enums.xml.
enum class ScanAppEntryPoint {
  kSettings = 0,
  kLauncher = 1,
  kMaxValue = kLauncher,
};

// Records ScanAppEntryPoint histogram value.
void RecordScanAppEntryPoint(ScanAppEntryPoint entry_point);

}  // namespace scanning
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SCANNING_SCANNING_UMA_H_
