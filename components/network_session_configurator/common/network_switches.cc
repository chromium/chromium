// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_session_configurator/common/network_switches.h"

#include "base/command_line.h"
#include "base/cxx17_backports.h"

namespace switches {

#define NETWORK_SWITCH(name, value) const char name[] = value;
#include "components/network_session_configurator/common/network_switch_list.h"
#undef NETWORK_SWITCH

}  // namespace switches

namespace network_session_configurator {

void CopyNetworkSwitches(const base::CommandLine& src_command_line,
                         base::CommandLine* dest_command_line) {
  static const char* const kSwitchNames[] = {
#define NETWORK_SWITCH(name, value) switches::name,
#include "components/network_session_configurator/common/network_switch_list.h"
#undef NETWORK_SWITCH
  };

  dest_command_line->CopySwitchesFrom(src_command_line, kSwitchNames,
                                      base::size(kSwitchNames));
}

}  // namespace network_session_configurator
