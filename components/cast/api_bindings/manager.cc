// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast/api_bindings/manager.h"

#include <string_view>
#include <utility>

#include "base/logging.h"

namespace cast_api_bindings {

Manager::Manager() = default;

Manager::~Manager() {
  DCHECK(port_handlers_.empty());
}

void Manager::RegisterPortHandler(std::string_view port_name,
                                  MessagePortConnectedHandler handler) {
  auto result = port_handlers_.try_emplace(port_name, std::move(handler));
  DCHECK(result.second);
}

void Manager::UnregisterPortHandler(std::string_view port_name) {
  size_t deleted = port_handlers_.erase(port_name);
  DCHECK_EQ(deleted, 1u);
}

bool Manager::OnPortConnected(
    std::string_view port_name,
    std::unique_ptr<cast_api_bindings::MessagePort> port) {
  if (!port)
    return false;

  auto handler = port_handlers_.find(port_name);
  if (handler == port_handlers_.end()) {
    LOG(ERROR) << "No handler found for port " << port_name << ".";
    return false;
  }

  handler->second.Run(std::move(port));
  return true;
}

}  // namespace cast_api_bindings
