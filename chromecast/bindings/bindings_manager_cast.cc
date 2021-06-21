// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/bindings/bindings_manager_cast.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
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

void BindingsManagerCast::AddBinding(base::StringPiece binding_name,
                                     base::StringPiece binding_script) {
  bindings_[std::string(binding_name)] = std::string(binding_script);
}

void BindingsManagerCast::OnClientDisconnected() {
  receiver_.reset();
}

void BindingsManagerCast::GetAll(GetAllCallback callback) {
  std::vector<chromecast::mojom::ApiBindingPtr> bindings_vector;
  for (const auto& bindings_name_and_script : bindings_) {
    bindings_vector.emplace_back(
        chromecast::mojom::ApiBinding::New(bindings_name_and_script.second));
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
