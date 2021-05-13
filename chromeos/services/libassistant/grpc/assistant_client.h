// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_H_

namespace chromeos {
namespace libassistant {

// The Libassistant customer class which establishes a gRPC connection to
// Libassistant and provides an interface for interacting with gRPC Libassistant
// client. It helps to build request/response proto messages internally for each
// specific method below and call the appropriate gRPC (IPC) client method.
class AssistantClient {
 public:
  virtual ~AssistantClient() = default;

  // 1. Start a gRPC server which hosts the services that Libassistant depends
  // on (maybe called by Libassistant) or receive events from Libassistant.
  // 2. Register this client as a customer of Libassistant by sending
  // RegisterCustomerRequest to Libassistant periodically. All supported
  // services should be registered first before calling this method.
  virtual bool StartGrpcServices() = 0;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_H_
