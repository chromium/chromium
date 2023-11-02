// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proxy_config/proxy_config_pref_names.h"

namespace proxy_config {
namespace prefs {

// Preference to store proxy settings.
const char kProxy[] = "proxy";

// A boolean pref that controls whether proxy settings from shared network
// settings (accordingly from device policy) are applied or ignored.
const char kUseSharedProxies[] = "settings.use_shared_proxies";

}  // namespace prefs
}  // namespace proxy_config
