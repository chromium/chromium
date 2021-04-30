// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_INTERNALS_HELPER_H_
#define COMPONENTS_GCM_DRIVER_GCM_INTERNALS_HELPER_H_

#include "components/gcm_driver/gcm_client.h"

class PrefService;

namespace base {
class DictionaryValue;
}

namespace gcm {
class GCMProfileService;
}

namespace gcm_driver {

// Sets the GCM infos for the gcm-internals WebUI in |results|.
void SetGCMInternalsInfo(const gcm::GCMClient::GCMStatistics* stats,
                         gcm::GCMProfileService* profile_service,
                         PrefService* prefs,
                         base::DictionaryValue* results);

}  // namespace gcm_driver

#endif  // COMPONENTS_GCM_DRIVER_GCM_INTERNALS_HELPER_H_
