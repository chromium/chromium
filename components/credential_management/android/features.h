// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CREDENTIAL_MANAGEMENT_ANDROID_FEATURES_H_
#define COMPONENTS_CREDENTIAL_MANAGEMENT_ANDROID_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace credential_management::features {

COMPONENT_EXPORT(CREDENTIAL_MANAGEMENT)
BASE_DECLARE_FEATURE(kCredentialManagementThirdPartyWebApiRequestForwarding);

}  // namespace credential_management::features

#endif  // COMPONENTS_CREDENTIAL_MANAGEMENT_ANDROID_FEATURES_H_
