// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/grpc/grpc_libassistant_client.h"

#include <memory>

#include "base/check.h"
#include "chromeos/assistant/internal/libassistant_util.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/customer_registration_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/action_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/event_notification_interface.pb.h"

namespace chromeos {
namespace libassistant {

template <>
std::string
GetLibassistGrpcMethodName<::assistant::api::RegisterCustomerRequest>() {
  // CustomerRegistrationService handles CustomerRegistrationRequest sent from
  // libassistant customers to register themselves before allowing to use
  // libassistant services.
  return chromeos::assistant::GetLibassistGrpcMethodName(
      "CustomerRegistrationService", "RegisterCustomer");
}

template <>
std::string
GetLibassistGrpcMethodName<::assistant::api::RegisterEventHandlerRequest>() {
  // EventNotificationService handles RegisterEventHandler sent from
  // libassistant customers to register themselves for events.
  return chromeos::assistant::GetLibassistGrpcMethodName(
      "EventNotificationService", "RegisterEventHandler");
}

template <>
std::string GetLibassistGrpcMethodName<::assistant::api::SendQueryRequest>() {
  // QueryService handles queries sent from libassistant customers.
  return chromeos::assistant::GetLibassistGrpcMethodName("QueryService",
                                                         "SendQuery");
}

template <>
std::string
GetLibassistGrpcMethodName<::assistant::api::RegisterActionModuleRequest>() {
  // QueryService handles RegisterActionModule sent from
  // libassistant customers to register themselves to handle actions.
  return chromeos::assistant::GetLibassistGrpcMethodName(
      "QueryService", "RegisterActionModule");
}

GrpcLibassistantClient::GrpcLibassistantClient(
    std::shared_ptr<grpc::Channel> channel)
    : channel_(std::move(channel)), client_thread_("gRPCLibassistantClient") {
  DCHECK(channel_);
}

GrpcLibassistantClient::~GrpcLibassistantClient() = default;

}  // namespace libassistant
}  // namespace chromeos
