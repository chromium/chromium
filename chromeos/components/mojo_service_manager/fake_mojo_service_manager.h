// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MOJO_SERVICE_MANAGER_FAKE_MOJO_SERVICE_MANAGER_H_
#define CHROMEOS_COMPONENTS_MOJO_SERVICE_MANAGER_FAKE_MOJO_SERVICE_MANAGER_H_

#include "base/component_export.h"
#include "chromeos/components/mojo_service_manager/mojom/mojo_service_manager.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos::mojo_service_manager {

// Provides fake implementation of the service manager for testing. This sets
// a fake mojo remote to the |Connection|.
class COMPONENT_EXPORT(CHROMEOS_MOJO_SERVICE_MANAGER) FakeMojoServiceManager
    : public mojom::ServiceManager {
 public:
  FakeMojoServiceManager();
  FakeMojoServiceManager(FakeMojoServiceManager&) = delete;
  FakeMojoServiceManager& operator=(FakeMojoServiceManager&) = delete;
  ~FakeMojoServiceManager() override;

  // Binds a new pipe and pass the pending remote.
  mojo::PendingRemote<mojom::ServiceManager> BindNewPipeAndPassRemote();

 private:
  // mojom::ServiceManager overrides.
  void Register(
      const std::string& service_name,
      mojo::PendingRemote<mojom::ServiceProvider> service_provider) override;
  void Request(const std::string& service_name,
               absl::optional<base::TimeDelta> timeout,
               mojo::ScopedMessagePipeHandle receiver) override;
  void Query(const std::string& service_name, QueryCallback callback) override;
  void AddServiceObserver(
      mojo::PendingRemote<mojom::ServiceObserver> observer) override;

  // The receiver object to provide the fake service manager.
  mojo::Receiver<mojom::ServiceManager> receiver_;
};

}  // namespace chromeos::mojo_service_manager

#endif  // CHROMEOS_COMPONENTS_MOJO_SERVICE_MANAGER_FAKE_MOJO_SERVICE_MANAGER_H_
