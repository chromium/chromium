// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_WM_DESKS_CHROMEOS_DESKS_HISTOGRAM_ENUMS_H_
#define CHROMEOS_UI_WM_DESKS_CHROMEOS_DESKS_HISTOGRAM_ENUMS_H_

namespace chromeos {

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// DesksAssignToAllDesksSource in src/tools/metrics/histograms/enums.xml.
enum class DesksAssignToAllDesksSource {
  kMoveToDeskMenu = 0,
  kKeyboardShortcut = 1,
  kMaxValue = kKeyboardShortcut,
};

static constexpr char kDesksAssignToAllDesksSourceHistogramName[] =
    "Ash.Desks.AssignToAllDesksSource";

}  // namespace chromeos

#endif  // CHROMEOS_UI_WM_DESKS_CHROMEOS_DESKS_HISTOGRAM_ENUMS_H_
