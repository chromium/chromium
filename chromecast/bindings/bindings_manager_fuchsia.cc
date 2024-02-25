// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/bindings/bindings_manager_fuchsia.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/mem_buffer_util.h"
#include "components/cast/message_port/fuchsia/message_port_fuchsia.h"

namespace chromecast {
namespace bindings {

BindingsManagerFuchsia::BindingsManagerFuchsia() = default;

BindingsManagerFuchsia::~BindingsManagerFuchsia() = default;

void BindingsManagerFuchsia::AddBinding(std::string_view binding_name,
                                        std::string_view binding_script) {
  std::pair<std::string, fuchsia::mem::Buffer> new_entry = {
      std::string(binding_name),
      base::MemBufferFromString(binding_script, "cast-binding-script")};
  for (auto it = bindings_.begin(); it != bindings_.end(); ++it) {
    if (it->first == new_entry.first) {
      *it = std::move(new_entry);
      return;
    }
  }

  bindings_.emplace_back(std::move(new_entry));
}

void BindingsManagerFuchsia::GetAll(GetAllCallback callback) {
  // Build a list of binding scripts and send it to the client.
  std::vector<chromium::cast::ApiBinding> bindings_vector;
  for (auto& entry : bindings_) {
    chromium::cast::ApiBinding binding_cloned;
    zx_status_t status;
    status = entry.second.Clone(binding_cloned.mutable_before_load_script());
    ZX_CHECK(status == ZX_OK, status) << "vmo::clone";
    bindings_vector.emplace_back(std::move(binding_cloned));
  }
  callback(std::move(bindings_vector));
}

void BindingsManagerFuchsia::Connect(
    std::string port_name,
    fidl::InterfaceHandle<::fuchsia::web::MessagePort> message_port) {
  OnPortConnected(port_name, std::unique_ptr<cast_api_bindings::MessagePort>(
                                 cast_api_bindings::MessagePortFuchsia::Create(
                                     std::move(message_port))));
}

}  // namespace bindings
}  // namespace chromecast
