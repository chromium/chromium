// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/machine_learning/public/cpp/service_connection.h"

#include <utility>

#include "base/component_export.h"
#include "base/no_destructor.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace lacros {
namespace machine_learning {
namespace {

// Real Impl of ServiceConnection
class COMPONENT_EXPORT(CHROMEOS_MLSERVICE) ServiceConnectionLacros
    : public chromeos::machine_learning::ServiceConnection {
 public:
  ServiceConnectionLacros();
  ServiceConnectionLacros(const ServiceConnectionLacros&) = delete;
  ServiceConnectionLacros& operator=(const ServiceConnectionLacros&) = delete;

  ~ServiceConnectionLacros() override;

  chromeos::machine_learning::mojom::MachineLearningService&
  GetMachineLearningService() override;

  void BindMachineLearningService(
      mojo::PendingReceiver<
          chromeos::machine_learning::mojom::MachineLearningService> receiver)
      override;

  void Initialize() override;
};

ServiceConnectionLacros::ServiceConnectionLacros() = default;

ServiceConnectionLacros::~ServiceConnectionLacros() = default;

chromeos::machine_learning::mojom::MachineLearningService&
ServiceConnectionLacros::GetMachineLearningService() {
  // TODO(crbug.com/40872414): Determine whether it is safe to assume
  // LacrosService is always available here.
  auto* service = chromeos::LacrosService::Get();
  DCHECK(service);
  mojo::Remote<chromeos::machine_learning::mojom::MachineLearningService>&
      machine_learning_service_remote = service->GetRemote<
          chromeos::machine_learning::mojom::MachineLearningService>();
  return *machine_learning_service_remote.get();
}

void ServiceConnectionLacros::BindMachineLearningService(
    mojo::PendingReceiver<
        chromeos::machine_learning::mojom::MachineLearningService> receiver) {
  // TODO(crbug.com/40872414): Determine whether it is safe to assume
  // LacrosService is always available here.
  auto* service = chromeos::LacrosService::Get();
  DCHECK(service);
  service->BindMachineLearningService(std::move(receiver));
}

void ServiceConnectionLacros::Initialize() {}

}  // namespace

}  // namespace machine_learning
}  // namespace lacros

namespace chromeos {
namespace machine_learning {

ServiceConnection* ServiceConnection::CreateRealInstance() {
  static base::NoDestructor<lacros::machine_learning::ServiceConnectionLacros>
      service_connection;
  return service_connection.get();
}

}  // namespace machine_learning
}  // namespace chromeos
