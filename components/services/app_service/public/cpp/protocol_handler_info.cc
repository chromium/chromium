// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/protocol_handler_info.h"

namespace apps {

ProtocolHandlerInfo::ProtocolHandlerInfo() = default;

ProtocolHandlerInfo::ProtocolHandlerInfo(const ProtocolHandlerInfo& other) =
    default;

ProtocolHandlerInfo::~ProtocolHandlerInfo() = default;

base::Value ProtocolHandlerInfo::AsDebugValue() const {
  base::Value::Dict root;
  root.Set("protocol", protocol);
  root.Set("url", url.spec());
  return base::Value(std::move(root));
}

}  // namespace apps
