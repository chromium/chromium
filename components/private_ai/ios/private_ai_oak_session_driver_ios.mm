// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/ios/private_ai_oak_session_driver_ios.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/private_ai/oak_session_service/oak_session_service.h"
#include "components/private_ai/oak_session_service/oak_tls_session_service.h"

namespace private_ai {

PrivateAiOakSessionDriverIOS::PrivateAiOakSessionDriverIOS() = default;
PrivateAiOakSessionDriverIOS::~PrivateAiOakSessionDriverIOS() = default;

mojo::Remote<mojom::OakSession>
PrivateAiOakSessionDriverIOS::BindOakSessionService() {
  mojo::Remote<mojom::OakSession> remote;
  auto service =
      std::make_unique<OakSessionService>(remote.BindNewPipeAndPassReceiver());

  OakSessionService* service_ptr = service.get();
  service_ptr->set_disconnect_handler(
      base::BindOnce(&PrivateAiOakSessionDriverIOS::OnServiceDisconnected,
                     weak_ptr_factory_.GetWeakPtr(), service_ptr));

  services_[service_ptr] = std::move(service);
  return remote;
}

mojo::Remote<mojom::OakTlsSession>
PrivateAiOakSessionDriverIOS::BindOakTlsSessionService() {
  mojo::Remote<mojom::OakTlsSession> remote;
  auto service = std::make_unique<OakTlsSessionService>(
      remote.BindNewPipeAndPassReceiver());

  OakTlsSessionService* service_ptr = service.get();
  service_ptr->set_disconnect_handler(
      base::BindOnce(&PrivateAiOakSessionDriverIOS::OnTlsServiceDisconnected,
                     weak_ptr_factory_.GetWeakPtr(), service_ptr));

  tls_services_[service_ptr] = std::move(service);
  return remote;
}

void PrivateAiOakSessionDriverIOS::OnServiceDisconnected(
    OakSessionService* service) {
  services_.erase(service);
}

void PrivateAiOakSessionDriverIOS::OnTlsServiceDisconnected(
    OakTlsSessionService* service) {
  tls_services_.erase(service);
}

}  // namespace private_ai
