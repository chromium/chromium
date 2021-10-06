// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_IMPL_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_IMPL_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/scoped_observation.h"
#include "chromeos/services/libassistant/grpc/assistant_client_v1.h"
#include "chromeos/services/libassistant/grpc/external_services/grpc_services_initializer.h"
#include "chromeos/services/libassistant/grpc/services_status_observer.h"
#include "chromeos/services/libassistant/grpc/services_status_provider.h"

namespace chromeos {
namespace libassistant {

class GrpcLibassistantClient;

// This class wraps the libassistant grpc client and exposes V2 APIs for
// ChromeOS to use.
class AssistantClientImpl : public AssistantClientV1,
                            public ServicesStatusObserver {
 public:
  AssistantClientImpl(
      std::unique_ptr<assistant_client::AssistantManager> assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal,
      const std::string& libassistant_service_address,
      const std::string& assistant_service_address);

  ~AssistantClientImpl() override;

  // chromeos::libassistant::AssistantClientV1 overrides:
  void StartServices(base::OnceClosure services_ready_callback) override;
  bool StartGrpcServices() override;

  // ServicesStatusObserver overrides:
  void OnServicesStatusChanged(ServicesStatus status) override;

 private:
  chromeos::libassistant::GrpcServicesInitializer grpc_services_;

  // Entry point for Libassistant V2 APIs, through which V2 methods can be
  // invoked. Created and owned by |GrpcServicesInitializer|.
  chromeos::libassistant::GrpcLibassistantClient& client_;

  // Invoked when all LibAssistant services are ready to query.
  base::OnceClosure services_ready_callback_;

  base::ScopedObservation<ServicesStatusProvider, ServicesStatusObserver>
      services_status_observation_{this};
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_IMPL_H_
