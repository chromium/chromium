// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_ARC_SETTINGS_APP_DELEGATE_H_
#define COMPONENTS_ARC_INTENT_HELPER_ARC_SETTINGS_APP_DELEGATE_H_

#include "ash/components/arc/mojom/intent_helper.mojom.h"

namespace arc {

class ArcSettingsAppDelegate {
 public:
  virtual ~ArcSettingsAppDelegate() = default;

  virtual void HandleUpdateAndroidSettings(mojom::AndroidSetting setting,
                                           bool is_enabled) = 0;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_ARC_SETTINGS_APP_DELEGATE_H_
