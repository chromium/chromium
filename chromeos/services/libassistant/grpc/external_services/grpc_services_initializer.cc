// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/grpc/external_services/grpc_services_initializer.h"

#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/libassistant/grpc/grpc_libassistant_client.h"
#include "chromeos/services/libassistant/grpc/grpc_util.h"
#include "third_party/grpc/src/include/grpc/grpc_security_constants.h"
#include "third_party/grpc/src/include/grpc/impl/codegen/grpc_types.h"
#include "third_party/grpc/src/include/grpcpp/create_channel.h"
#include "third_party/grpc/src/include/grpcpp/security/credentials.h"
#include "third_party/grpc/src/include/grpcpp/support/channel_arguments.h"

namespace chromeos {
namespace libassistant {

GrpcServicesInitializer::GrpcServicesInitializer(
    const std::string& libassistant_service_address)
    : libassistant_service_address_(libassistant_service_address) {
  DCHECK(!libassistant_service_address.empty());

  InitGrpcClient();
}

GrpcServicesInitializer::~GrpcServicesInitializer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

GrpcLibassistantClient& GrpcServicesInitializer::GrpcLibassistantClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return *libassistant_client_;
}

void GrpcServicesInitializer::InitGrpcClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  grpc::ChannelArguments channel_args;
  channel_args.SetInt(GRPC_ARG_INITIAL_RECONNECT_BACKOFF_MS, 200);
  channel_args.SetInt(GRPC_ARG_MIN_RECONNECT_BACKOFF_MS, 200);
  channel_args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 2000);
  grpc_local_connect_type connect_type =
      GetGrpcLocalConnectType(libassistant_service_address_);

  auto channel = grpc::CreateCustomChannel(
      libassistant_service_address_,
      ::grpc::experimental::LocalCredentials(connect_type), channel_args);

  libassistant_client_ =
      std::make_unique<chromeos::libassistant::GrpcLibassistantClient>(channel);
}

}  // namespace libassistant
}  // namespace chromeos
