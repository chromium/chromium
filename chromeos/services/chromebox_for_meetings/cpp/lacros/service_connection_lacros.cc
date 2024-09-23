// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/component_export.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace lacros {
namespace cfm {

namespace {

namespace mojom = ::chromeos::cfm::mojom;

// Real Impl of ServiceConnection
// Uses CrOSAPI to pass calls to the ash implementation
class COMPONENT_EXPORT(CHROMEOS_CFMSERVICE) ServiceConnectionLacrosImpl
    : public chromeos::cfm::ServiceConnection {
 public:
  ServiceConnectionLacrosImpl() = default;
  ServiceConnectionLacrosImpl(const ServiceConnectionLacrosImpl&) = delete;
  ServiceConnectionLacrosImpl& operator=(const ServiceConnectionLacrosImpl&) =
      delete;
  ~ServiceConnectionLacrosImpl() override = default;

  // Binds a |CfMServiceContext| receiver to this implementation in order to
  // forward requests to the underlying daemon connected by a single remote.
  void BindServiceContext(
      mojo::PendingReceiver<mojom::CfmServiceContext> receiver) override;
};

void ServiceConnectionLacrosImpl::BindServiceContext(
    mojo::PendingReceiver<mojom::CfmServiceContext> receiver) {
  // TODO(crbug.com/40872414): Determine whether it is safe to assume
  // LacrosService is always available here.
  auto* service = chromeos::LacrosService::Get();
  DCHECK(service);

  service->BindCfmServiceContext(std::move(receiver));
}

}  // namespace

}  // namespace cfm
}  // namespace lacros

namespace chromeos {
namespace cfm {

ServiceConnection* ServiceConnection::GetInstanceForCurrentPlatform() {
  static base::NoDestructor<lacros::cfm::ServiceConnectionLacrosImpl>
      service_connection;
  return service_connection.get();
}

}  // namespace cfm
}  // namespace chromeos
