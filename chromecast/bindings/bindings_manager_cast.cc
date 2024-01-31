// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/bindings/bindings_manager_cast.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "components/cast/message_port/cast/message_port_cast.h"

namespace chromecast {
namespace bindings {

BindingsManagerCast::BindingsManagerCast() = default;

BindingsManagerCast::~BindingsManagerCast() = default;

mojo::PendingRemote<mojom::ApiBindings> BindingsManagerCast::CreateRemote() {
  DCHECK(!receiver_.is_bound());

  mojo::PendingRemote<mojom::ApiBindings> pending_remote =
      receiver_.BindNewPipeAndPassRemote();
  receiver_.set_disconnect_handler(base::BindOnce(
      &BindingsManagerCast::OnClientDisconnected, base::Unretained(this)));

  return pending_remote;
}

void BindingsManagerCast::AddBinding(std::string_view binding_name,
                                     std::string_view binding_script) {
  std::pair<std::string, std::string> new_entry = {std::string(binding_name),
                                                   std::string(binding_script)};
  for (auto it = bindings_.begin(); it != bindings_.end(); ++it) {
    if (it->first == new_entry.first) {
      *it = std::move(new_entry);
      return;
    }
  }

  bindings_.emplace_back(std::move(new_entry));
}

void BindingsManagerCast::OnClientDisconnected() {
  receiver_.reset();
}

void BindingsManagerCast::GetAll(GetAllCallback callback) {
  std::vector<chromecast::mojom::ApiBindingPtr> bindings_vector;
  for (const auto& entry : bindings_) {
    bindings_vector.emplace_back(
        chromecast::mojom::ApiBinding::New(entry.second));
  }
  std::move(callback).Run(std::move(bindings_vector));
}

void BindingsManagerCast::Connect(const std::string& port_name,
                                  blink::MessagePortDescriptor port) {
  OnPortConnected(port_name,
                  cast_api_bindings::MessagePortCast::Create(std::move(port)));
}

}  // namespace bindings
}  // namespace chromecast
