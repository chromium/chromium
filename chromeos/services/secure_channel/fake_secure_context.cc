// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_secure_context.h"

#include <stddef.h>

#include "base/strings/string_util.h"

namespace chromeos {

namespace secure_channel {

namespace {

const char kFakeEncodingSuffix[] = ", but encoded";
const size_t kFakeEncodingSuffixLen = sizeof(kFakeEncodingSuffix) - 1;
const char kChannelBindingData[] = "channel binding data";

}  // namespace

FakeSecureContext::FakeSecureContext()
    : protocol_version_(SecureContext::PROTOCOL_VERSION_THREE_ONE) {}

FakeSecureContext::~FakeSecureContext() {}

std::string FakeSecureContext::GetChannelBindingData() const {
  return channel_binding_data_ ? *channel_binding_data_ : kChannelBindingData;
}

SecureContext::ProtocolVersion FakeSecureContext::GetProtocolVersion() const {
  return protocol_version_;
}

void FakeSecureContext::Encode(const std::string& message,
                               MessageCallback callback) {
  std::move(callback).Run(message + kFakeEncodingSuffix);
}

void FakeSecureContext::Decode(const std::string& encoded_message,
                               MessageCallback callback) {
  if (!EndsWith(encoded_message, kFakeEncodingSuffix,
                base::CompareCase::SENSITIVE)) {
    std::move(callback).Run(std::string());
    return;
  }

  std::string decoded_message = encoded_message;
  decoded_message.erase(decoded_message.size() - kFakeEncodingSuffixLen);
  std::move(callback).Run(decoded_message);
}

}  // namespace secure_channel

}  // namespace chromeos
