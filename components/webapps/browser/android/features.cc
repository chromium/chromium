// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/features.h"

#include "base/feature_list.h"

namespace webapps {
namespace features {

const base::Feature kAddToHomescreenMessaging{
    "AddToHomescreenMessaging", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the installable ambient badge infobar.
const base::Feature kInstallableAmbientBadgeInfoBar{
    "InstallableAmbientBadgeInfoBar", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables the installable ambient badge message.
const base::Feature kInstallableAmbientBadgeMessage{
    "InstallableAmbientBadgeMessage", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace webapps
