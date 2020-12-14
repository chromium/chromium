// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/util/histogram_util.h"

#include "base/metrics/histogram_functions.h"

namespace chromeos {
namespace phonehub {
namespace util {

void LogFeatureOptInEntryPoint(OptInEntryPoint entry_point) {
  base::UmaHistogramEnumeration("PhoneHub.OptInEntryPoint", entry_point);
}

void LogTetherConnectionResult(TetherConnectionResult result) {
  base::UmaHistogramEnumeration(
      "PhoneHub.TaskCompletion.TetherConnection.Result", result);
}

}  // namespace util
}  // namespace phonehub
}  // namespace chromeos
