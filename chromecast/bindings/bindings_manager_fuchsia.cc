// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/bindings/bindings_manager_fuchsia.h"

#include <utility>
#include <vector>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/base/message_port.h"

namespace chromecast {
namespace bindings {

BindingsManagerFuchsia::BindingsManagerFuchsia() = default;

BindingsManagerFuchsia::~BindingsManagerFuchsia() = default;

void BindingsManagerFuchsia::AddBinding(base::StringPiece binding_name,
                                        base::StringPiece binding_script) {
  bindings_[binding_name.as_string()] =
      cr_fuchsia::MemBufferFromString(binding_script, "cast-binding-script");
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
  OnPortConnected(port_name,
                  cr_fuchsia::MessagePortFromFidl(std::move(message_port)));
}

}  // namespace bindings
}  // namespace chromecast
