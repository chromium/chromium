// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CONTEXT_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CONTEXT_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "chromeos/ash/services/secure_channel/secure_context.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash::secure_channel {

class FakeSecureContext : public SecureContext {
 public:
  FakeSecureContext();

  FakeSecureContext(const FakeSecureContext&) = delete;
  FakeSecureContext& operator=(const FakeSecureContext&) = delete;

  ~FakeSecureContext() override;

  // SecureContext:
  ProtocolVersion GetProtocolVersion() const override;
  std::string GetChannelBindingData() const override;
  void Encode(const std::string& message,
              EncodeMessageCallback callback) override;
  void DecodeAndDequeue(const std::string& encoded_message,
                        DecodeMessageCallback callback) override;

  void set_protocol_version(ProtocolVersion protocol_version) {
    protocol_version_ = protocol_version;
  }

  void set_channel_binding_data(const std::string channel_binding_data) {
    channel_binding_data_ = channel_binding_data;
  }

 private:
  ProtocolVersion protocol_version_;
  std::optional<std::string> channel_binding_data_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CONTEXT_H_
