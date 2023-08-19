// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_SESSION_CONFIGURATOR_BROWSER_NETWORK_SESSION_CONFIGURATOR_H_
#define COMPONENTS_NETWORK_SESSION_CONFIGURATOR_BROWSER_NETWORK_SESSION_CONFIGURATOR_H_

#include "net/quic/quic_context.h"
#include "net/url_request/url_request_context_builder.h"

namespace base {
class CommandLine;
}

namespace net {
struct HttpNetworkSessionParams;
}

namespace network_session_configurator {

// Helper functions to configure HttpNetworkSessionParams based on field
// trials and command line.

// Configure |params| based on field trials and command line,
// and forcing (policy or other command line) arguments.
void ParseCommandLineAndFieldTrials(const base::CommandLine& command_line,
                                    bool is_quic_force_disabled,
                                    net::HttpNetworkSessionParams* params,
                                    net::QuicParams* quic_params);

// Returns the URLRequestContextBuilder::HttpCacheParams::Type that the disk
// cache should use.
net::URLRequestContextBuilder::HttpCacheParams::Type ChooseCacheType();

}  // namespace network_session_configurator

#endif  // COMPONENTS_NETWORK_SESSION_CONFIGURATOR_BROWSER_NETWORK_SESSION_CONFIGURATOR_H_
