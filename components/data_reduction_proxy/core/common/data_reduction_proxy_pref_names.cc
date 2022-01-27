// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"

namespace data_reduction_proxy {
namespace prefs {

// A boolean specifying whether the DataSaver feature is enabled for this
// client. Note that this preference key name is a legacy string for the sdpy
// proxy.
//
// WARNING: This pref is not the source of truth for determining if Data Saver
// is enabled. Use |DataReductionSettings::IsDataSaverEnabledByUser| instead or
// consult the OWNERS.
const char kDataSaverEnabled[] = "spdy_proxy.enabled";

// A boolean specifying whether the data reduction proxy was ever enabled
// before.
const char kDataReductionProxyWasEnabledBefore[] =
    "spdy_proxy.was_enabled_before";

// An integer pref that contains the time when the data reduction proxy was last
// enabled. Recorded only if the data reduction proxy was last enabled since
// this pref was added.
const char kDataReductionProxyLastEnabledTime[] =
    "data_reduction.last_enabled_time";

}  // namespace prefs
}  // namespace data_reduction_proxy
