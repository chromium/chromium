// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/common/icon_constants.h"

#include "ash/constants/ash_features.h"

namespace app_list {

int GetAnswerCardIconDimension() {
  return ash::features::IsProductivityLauncherEnabled() ? 28 : 24;
}

int GetAppIconDimension() {
  return ash::features::IsProductivityLauncherEnabled() ? 32 : 20;
}

int GetImageIconDimension() {
  return ash::features::IsProductivityLauncherEnabled() ? 28 : 32;
}

}  // namespace app_list
