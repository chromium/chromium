// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXY_CONFIG_PROXY_CONFIG_PREF_NAMES_H_
#define COMPONENTS_PROXY_CONFIG_PROXY_CONFIG_PREF_NAMES_H_

namespace proxy_config::prefs {

// Preference to store proxy settings.
inline constexpr char kProxy[] = "proxy";

// A boolean pref that controls whether proxy settings from shared network
// settings (accordingly from device policy) are applied or ignored.
inline constexpr char kUseSharedProxies[] = "settings.use_shared_proxies";

}  // namespace proxy_config::prefs

#endif  // COMPONENTS_PROXY_CONFIG_PROXY_CONFIG_PREF_NAMES_H_
