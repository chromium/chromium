// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/public/cpp/features.h"

#include "ash/constants/ash_features.h"
#include "base/command_line.h"
#include "base/feature_list.h"

namespace ash::assistant::features {

BASE_FEATURE(kEnableNewEntryPoint,
             "ChromeOSEnableNewEntryPoint",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsNewEntryPointEnabled() {
  return base::FeatureList::IsEnabled(kEnableNewEntryPoint);
}

}  // namespace ash::assistant::features
