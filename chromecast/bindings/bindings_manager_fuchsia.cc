// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/bindings/bindings_manager_fuchsia.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/mem_buffer_util.h"
#include "base/strings/string_piece.h"
#include "components/cast/message_port/fuchsia/message_port_fuchsia.h"

namespace chromecast {
namespace bindings {

BindingsManagerFuchsia::BindingsManagerFuchsia() = default;

BindingsManagerFuchsia::~BindingsManagerFuchsia() = default;

void BindingsManagerFuchsia::AddBinding(base::StringPiece binding_name,
                                        base::StringPiece binding_script) {
  bindings_[std::string(binding_name)] =
      base::MemBufferFromString(binding_script, "cast-binding-script");
}

void BindingsManagerFuchsia::GetAll(GetAllCallback callback) {
  // Build a list of binding scripts and send it to the client.
  std::vector<chromium::cast::ApiBinding> bindings_vector;
  for (auto& bindings_name_and_buffer : bindings_) {
    chromium::cast::ApiBinding binding_cloned;
    zx_status_t status;
    status = bindings_name_and_buffer.second.Clone(
        binding_cloned.mutable_before_load_script());
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
