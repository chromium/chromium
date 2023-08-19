// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_session_configurator/common/network_switches.h"

#include "base/command_line.h"

namespace switches {

// `kEnableHttp2GreaseSettings` does not include the word "enable" for
// historical reasons.
const char kEnableHttp2GreaseSettings[] = "http2-grease-settings";
const char kDisableHttp2GreaseSettings[] = "disable-http2-grease-settings";

#define NETWORK_SWITCH(name, value) const char name[] = value;
#include "components/network_session_configurator/common/network_switch_list.h"
#undef NETWORK_SWITCH

}  // namespace switches

namespace network_session_configurator {

void CopyNetworkSwitches(const base::CommandLine& src_command_line,
                         base::CommandLine* dest_command_line) {
  static const char* const kSwitchNames[] = {
      switches::kEnableHttp2GreaseSettings,
      switches::kDisableHttp2GreaseSettings,
#define NETWORK_SWITCH(name, value) switches::name,
#include "components/network_session_configurator/common/network_switch_list.h"
#undef NETWORK_SWITCH
  };

  dest_command_line->CopySwitchesFrom(src_command_line, kSwitchNames);
}

}  // namespace network_session_configurator
