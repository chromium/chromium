// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/testing/fake_secure_channel.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/types/expected.h"
#include "components/private_ai/proto/private_ai.pb.h"

namespace private_ai {

FakeSecureChannel::FakeSecureChannel(ResponseCallback callback,
                                     DestroyCallback on_destroy)
    : response_callback_(std::move(callback)),
      on_destroy_(std::move(on_destroy)) {
  CHECK(response_callback_);
}

FakeSecureChannel::~FakeSecureChannel() {
  if (on_destroy_) {
    std::move(on_destroy_).Run(this);
  }
}

bool FakeSecureChannel::Write(const Request& request) {
  proto::PrivateAiRequest request_proto;
  if (!request_proto.ParseFromArray(request.data(), request.size())) {
    return false;
  }
  written_requests_.push_back(request_proto);
  return write_succeeds_;
}

void FakeSecureChannel::send_back_response(
    const proto::PrivateAiResponse& response) {
  std::vector<uint8_t> response_bytes(response.ByteSizeLong());
  response.SerializeToArray(response_bytes.data(), response_bytes.size());

  CHECK(response_callback_);
  response_callback_.Run(std::move(response_bytes));
}

void FakeSecureChannel::send_back_error(ErrorCode error) {
  CHECK(response_callback_);
  response_callback_.Run(base::unexpected(error));
}

FakeSecureChannelFactory::FakeSecureChannelFactory(
    OnCreatedCallback on_created_callback,
    OnDestroyedCallback on_destroyed_callback)
    : on_created_callback_(std::move(on_created_callback)),
      on_destroyed_callback_(std::move(on_destroyed_callback)) {}

FakeSecureChannelFactory::~FakeSecureChannelFactory() = default;

std::unique_ptr<SecureChannel> FakeSecureChannelFactory::Create(
    SecureChannel::ResponseCallback callback) {
  auto secure_channel = std::make_unique<FakeSecureChannel>(
      std::move(callback), on_destroyed_callback_);
  on_created_callback_.Run(secure_channel.get());
  return secure_channel;
}

}  // namespace private_ai
