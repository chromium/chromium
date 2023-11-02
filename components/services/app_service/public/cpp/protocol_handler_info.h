// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PROTOCOL_HANDLER_INFO_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PROTOCOL_HANDLER_INFO_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "url/gurl.h"

namespace apps {

// Contains information about a protocol handler for an app.
struct ProtocolHandlerInfo {
  ProtocolHandlerInfo();
  ProtocolHandlerInfo(const ProtocolHandlerInfo& other);
  ~ProtocolHandlerInfo();

  base::Value AsDebugValue() const;

  std::string protocol;
  GURL url;
};
using ProtocolHandlers = std::vector<ProtocolHandlerInfo>;

bool operator==(const ProtocolHandlerInfo& handler1,
                const ProtocolHandlerInfo& handler2);

bool operator!=(const ProtocolHandlerInfo& handler1,
                const ProtocolHandlerInfo& handler2);

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PROTOCOL_HANDLER_INFO_H_
