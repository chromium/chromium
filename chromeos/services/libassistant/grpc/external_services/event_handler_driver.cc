// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/grpc/external_services/event_handler_driver.h"

#include "chromeos/assistant/internal/libassistant_util.h"

namespace chromeos {
namespace libassistant {

namespace {
constexpr char kDeviceStateEventName[] = "DeviceStateEvent";
constexpr char kHandlerMethodName[] = "OnEventFromLibas";
}  // namespace

template <>
::assistant::api::RegisterEventHandlerRequest
CreateRegistrationRequest<::assistant::api::DeviceStateEventHandlerInterface>(
    const std::string& assistant_service_address) {
  ::assistant::api::RegisterEventHandlerRequest request;
  request.mutable_device_state_events_to_handle()->set_select_all(true);
  auto* external_handler = request.mutable_handler();
  external_handler->set_server_address(assistant_service_address);
  external_handler->set_service_name(
      chromeos::assistant::GetLibassistGrpcServiceName(kDeviceStateEventName));
  external_handler->set_handler_method(kHandlerMethodName);
  return request;
}

}  // namespace libassistant
}  // namespace chromeos
