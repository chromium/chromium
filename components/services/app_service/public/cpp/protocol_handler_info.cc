// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/protocol_handler_info.h"

namespace apps {

ProtocolHandlerInfo::ProtocolHandlerInfo() = default;

ProtocolHandlerInfo::ProtocolHandlerInfo(const ProtocolHandlerInfo& other) =
    default;

ProtocolHandlerInfo::~ProtocolHandlerInfo() = default;

bool operator==(const ProtocolHandlerInfo& handler1,
                const ProtocolHandlerInfo& handler2) {
  return handler1.protocol == handler2.protocol && handler1.url == handler2.url;
}

bool operator!=(const ProtocolHandlerInfo& handler1,
                const ProtocolHandlerInfo& handler2) {
  return !(handler1 == handler2);
}

base::Value ProtocolHandlerInfo::AsDebugValue() const {
  base::Value root(base::Value::Type::DICT);
  root.SetStringKey("protocol", protocol);
  root.SetStringKey("url", url.spec());
  return root;
}

}  // namespace apps
