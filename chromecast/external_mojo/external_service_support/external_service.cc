// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/external_mojo/external_service_support/external_service.h"

#include "base/logging.h"

namespace chromecast {
namespace external_service_support {

ExternalService::ExternalService() = default;

ExternalService::~ExternalService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

mojo::PendingRemote<external_mojo::mojom::ExternalService>
ExternalService::GetReceiver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  service_receiver_.reset();
  return service_receiver_.BindNewPipeAndPassRemote();
}

void ExternalService::AddInterface(const std::string& interface_name,
                                   std::unique_ptr<Binder> binder) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveInterface(interface_name);
  binders_.emplace(interface_name, std::move(binder));
}

void ExternalService::RemoveInterface(const std::string& interface_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  binders_.erase(interface_name);
}

void ExternalService::OnBindInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  auto it = binders_.find(interface_name);
  if (it == binders_.end()) {
    LOG(ERROR) << interface_name << " cannot be bound (not found)";
    return;
  }

  it->second->BindInterface(interface_name, std::move(interface_pipe));
}

}  // namespace external_service_support
}  // namespace chromecast
