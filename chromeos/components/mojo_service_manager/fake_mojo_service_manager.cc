// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mojo_service_manager/fake_mojo_service_manager.h"

#include "base/notreached.h"

namespace chromeos::mojo_service_manager {

FakeMojoServiceManager::FakeMojoServiceManager() : receiver_(this) {}

FakeMojoServiceManager::~FakeMojoServiceManager() {}

mojo::PendingRemote<mojom::ServiceManager>
FakeMojoServiceManager::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void FakeMojoServiceManager::Register(
    const std::string& service_name,
    mojo::PendingRemote<mojom::ServiceProvider> service_provider) {
  NOTIMPLEMENTED();
}

void FakeMojoServiceManager::Request(const std::string& service_name,
                                     absl::optional<base::TimeDelta> timeout,
                                     mojo::ScopedMessagePipeHandle receiver) {
  NOTIMPLEMENTED();
}

void FakeMojoServiceManager::Query(const std::string& service_name,
                                   QueryCallback callback) {
  NOTIMPLEMENTED();
}

void FakeMojoServiceManager::AddServiceObserver(
    mojo::PendingRemote<mojom::ServiceObserver> observer) {
  NOTIMPLEMENTED();
}

}  // namespace chromeos::mojo_service_manager
