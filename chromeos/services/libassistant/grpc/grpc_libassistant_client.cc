// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/grpc/grpc_libassistant_client.h"

#include <memory>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"

namespace chromeos {
namespace libassistant {

GrpcLibassistantClient::GrpcLibassistantClient(
    std::shared_ptr<grpc::Channel> channel)
    : channel_(std::move(channel)) {
  DCHECK(channel_);
}

GrpcLibassistantClient::~GrpcLibassistantClient() = default;

void GrpcLibassistantClient::RegisterCustomer() {
  NOTIMPLEMENTED();
}

}  // namespace libassistant
}  // namespace chromeos
