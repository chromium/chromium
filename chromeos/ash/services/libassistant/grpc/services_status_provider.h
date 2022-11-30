// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_SERVICES_STATUS_PROVIDER_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_SERVICES_STATUS_PROVIDER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/services/libassistant/grpc/external_services/grpc_services_observer.h"
#include "chromeos/ash/services/libassistant/grpc/services_status_observer.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/event_handler_interface.pb.h"

namespace ash::libassistant {

// Component monitoring Libassistant gRPC services status, exposing method to
// get current services status and notify observers on status change.
class ServicesStatusProvider
    : public GrpcServicesObserver<::assistant::api::OnHeartbeatEventRequest> {
 public:
  ServicesStatusProvider();
  ServicesStatusProvider(const ServicesStatusProvider&) = delete;
  ServicesStatusProvider& operator=(const ServicesStatusProvider&) = delete;
  ~ServicesStatusProvider() override;

  // GrpcServiceObserver implementation:
  void OnGrpcMessage(
      const ::assistant::api::OnHeartbeatEventRequest& request) override;

  void AddObserver(ServicesStatusObserver* observer);
  void RemoveObserver(ServicesStatusObserver* observer);

 private:
  ServicesStatus status_ = ServicesStatus::OFFLINE;

  base::ObserverList<ServicesStatusObserver> observers_;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_SERVICES_STATUS_PROVIDER_H_
