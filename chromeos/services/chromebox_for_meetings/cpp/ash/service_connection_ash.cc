// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/component_export.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {
namespace cfm {

namespace {

namespace mojom = ::chromeos::cfm::mojom;

// Real Impl of ServiceConnection for Ash
// Uses CrOSAPI to pass calls to the ash implementation
// Note: This implementation is the default implementation for ServiceConnection
// and is essentially a NoOp.
class COMPONENT_EXPORT(CHROMEOS_CFMSERVICE) ServiceConnectionAshImpl
    : public chromeos::cfm::ServiceConnection {
 public:
  ServiceConnectionAshImpl() = default;
  ServiceConnectionAshImpl(const ServiceConnectionAshImpl&) = delete;
  ServiceConnectionAshImpl& operator=(const ServiceConnectionAshImpl&) = delete;
  ~ServiceConnectionAshImpl() override = default;

  // Binds a |CfMServiceContext| receiver to this implementation in order to
  // forward requests to the underlying daemon connected by a single remote.
  void BindServiceContext(
      mojo::PendingReceiver<mojom::CfmServiceContext> receiver) override;
};

void ServiceConnectionAshImpl::BindServiceContext(
    mojo::PendingReceiver<mojom::CfmServiceContext> receiver) {
  // Note: Mainstream Ash impl is a noop as there is no implementation
  // associated for the non chromebox for meeting devices.

  // Cast to uint32_t to ensure correct value is sent in order to inform
  // the client outside of chrome that the implementation is unavailable.
  // TODO(b/341417390): Investigate casting safely.
  uint32_t reason = static_cast<uint32_t>(
      mojom::DisconnectReason::kServiceUnavailableCode);
  std::string description = mojom::DisconnectReason::kServiceUnavailableMessage;

  receiver.ResetWithReason(std::move(reason), std::move(description));
}

}  // namespace

}  // namespace cfm
}  // namespace ash

namespace chromeos {
namespace cfm {

ServiceConnection* ServiceConnection::GetInstanceForCurrentPlatform() {
  static base::NoDestructor<ash::cfm::ServiceConnectionAshImpl>
      service_connection;
  return service_connection.get();
}

}  // namespace cfm
}  // namespace chromeos
