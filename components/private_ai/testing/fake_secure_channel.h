// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_TESTING_FAKE_SECURE_CHANNEL_H_
#define COMPONENTS_PRIVATE_AI_TESTING_FAKE_SECURE_CHANNEL_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "components/private_ai/proto/private_ai.pb.h"
#include "components/private_ai/secure_channel.h"

namespace private_ai {

class FakeSecureChannel : public SecureChannel {
 public:
  using DestroyCallback = base::OnceCallback<void(FakeSecureChannel*)>;

  explicit FakeSecureChannel(ResponseCallback callback,
                             DestroyCallback on_destroy = {});
  ~FakeSecureChannel() override;

  // SecureChannel implementation:
  bool Write(const Request& request) override;

  // Test control methods:
  void set_write_succeeds(bool succeeds) { write_succeeds_ = succeeds; }

  const proto::PrivateAiRequest& last_written_request() const {
    return written_requests_.back();
  }

  const std::vector<proto::PrivateAiRequest>& written_requests() const {
    return written_requests_;
  }

  void send_back_response(const proto::PrivateAiResponse& response);
  void send_back_error(ErrorCode error);

 private:
  ResponseCallback response_callback_;
  DestroyCallback on_destroy_;
  std::vector<proto::PrivateAiRequest> written_requests_;
  bool write_succeeds_ = true;
};

class FakeSecureChannelFactory : public SecureChannel::Factory {
 public:
  using OnCreatedCallback = base::RepeatingCallback<void(FakeSecureChannel*)>;
  using OnDestroyedCallback = base::RepeatingCallback<void(FakeSecureChannel*)>;

  explicit FakeSecureChannelFactory(
      OnCreatedCallback on_created_callback,
      OnDestroyedCallback on_destroyed_callback = {});
  ~FakeSecureChannelFactory() override;

  // SecureChannel::Factory implementation:
  std::unique_ptr<SecureChannel> Create(
      SecureChannel::ResponseCallback callback) override;

 private:
  OnCreatedCallback on_created_callback_;
  OnDestroyedCallback on_destroyed_callback_;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_TESTING_FAKE_SECURE_CHANNEL_H_
