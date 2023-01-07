// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_SESSION_CONFIGURATOR_COMMON_NETWORK_SWITCHES_H_
#define COMPONENTS_NETWORK_SESSION_CONFIGURATOR_COMMON_NETWORK_SWITCHES_H_

#include "network_session_configurator_export.h"

namespace base {
class CommandLine;
}

namespace switches {

// These two switches are not in network_switch_list.h so that
// java_cpp_strings.py can parse them.  TODO(bnc): Remove switches after
// launching feature.
// Enable/disable "greasing" HTTP/2 SETTINGS, that is, sending SETTINGS
// parameters with reserved identifiers.  See
// https://tools.ietf.org/html/draft-bishop-httpbis-grease-00 for more detail.
NETWORK_SESSION_CONFIGURATOR_EXPORT extern const char
    kEnableHttp2GreaseSettings[];
NETWORK_SESSION_CONFIGURATOR_EXPORT extern const char
    kDisableHttp2GreaseSettings[];

#define NETWORK_SWITCH(name, value) \
  NETWORK_SESSION_CONFIGURATOR_EXPORT extern const char name[];
#include "components/network_session_configurator/common/network_switch_list.h"
#undef NETWORK_SWITCH

}  // namespace switches

namespace network_session_configurator {

// Copies all command line switches the configurator handles from the |src|
// CommandLine to the |dest| one.
NETWORK_SESSION_CONFIGURATOR_EXPORT void CopyNetworkSwitches(
    const base::CommandLine& src_command_line,
    base::CommandLine* dest_command_line);

}  // namespace network_session_configurator

#endif  // COMPONENTS_NETWORK_SESSION_CONFIGURATOR_COMMON_NETWORK_SWITCHES_H_
