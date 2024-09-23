// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/proxy_config/proxy_prefs.h"

#include "base/check.h"
#include "base/notreached.h"

namespace ProxyPrefs {

namespace {

// These names are exposed to the proxy extension API. They must be in sync
// with the constants of ProxyPrefs.
const char* kProxyModeNames[] = { kDirectProxyModeName,
                                  kAutoDetectProxyModeName,
                                  kPacScriptProxyModeName,
                                  kFixedServersProxyModeName,
                                  kSystemProxyModeName };

static_assert(std::size(kProxyModeNames) == kModeCount,
              "kProxyModeNames must have kModeCount elements");

}  // namespace

const char kDirectProxyModeName[] = "direct";
const char kAutoDetectProxyModeName[] = "auto_detect";
const char kPacScriptProxyModeName[] = "pac_script";
const char kFixedServersProxyModeName[] = "fixed_servers";
const char kSystemProxyModeName[] = "system";

bool IntToProxyMode(int in_value, ProxyMode* out_value) {
  DCHECK(out_value);
  if (in_value < 0 || in_value >= kModeCount)
    return false;
  *out_value = static_cast<ProxyMode>(in_value);
  return true;
}

bool StringToProxyMode(const std::string& in_value, ProxyMode* out_value) {
  DCHECK(out_value);
  for (int i = 0; i < kModeCount; i++) {
    if (in_value == kProxyModeNames[i])
      return IntToProxyMode(i, out_value);
  }
  return false;
}

const char* ProxyModeToString(ProxyMode mode) {
  return kProxyModeNames[mode];
}

std::string ConfigStateToDebugString(ConfigState state) {
  switch (state) {
    case CONFIG_POLICY:
      return "config_policy";
    case CONFIG_EXTENSION:
      return "config_extension";
    case CONFIG_OTHER_PRECEDE:
      return "config_other_precede";
    case CONFIG_SYSTEM:
      return "config_system";
    case CONFIG_FALLBACK:
      return "config_fallback";
    case CONFIG_UNSET:
      return "config_unset";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

}  // namespace ProxyPrefs
