// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_GRPC_SERVICES_STATUS_PROVIDER_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_GRPC_SERVICES_STATUS_PROVIDER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/event_handler_interface.pb.h"
#include "chromeos/services/libassistant/grpc/external_services/grpc_services_observer.h"
#include "chromeos/services/libassistant/grpc/services_status_observer.h"

namespace chromeos {
namespace libassistant {

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

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_GRPC_SERVICES_STATUS_PROVIDER_H_
