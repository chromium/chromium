// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_INTERNALS_HELPER_H_
#define COMPONENTS_GCM_DRIVER_GCM_INTERNALS_HELPER_H_

#include "base/values.h"
#include "components/gcm_driver/gcm_client.h"

class PrefService;

namespace gcm {
class GCMProfileService;
}

namespace gcm_driver {

// Returns the GCM infos for the gcm-internals WebUI.
base::Value::Dict SetGCMInternalsInfo(
    const gcm::GCMClient::GCMStatistics* stats,
    gcm::GCMProfileService* profile_service,
    PrefService* prefs);

}  // namespace gcm_driver

#endif  // COMPONENTS_GCM_DRIVER_GCM_INTERNALS_HELPER_H_
