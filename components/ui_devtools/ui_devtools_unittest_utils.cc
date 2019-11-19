// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/ui_devtools_unittest_utils.h"

#include "base/strings/string_util.h"
#include "components/ui_devtools/devtools_protocol_encoding.h"

namespace ui_devtools {

MockUIElementDelegate::MockUIElementDelegate() {}
MockUIElementDelegate::~MockUIElementDelegate() {}

FakeFrontendChannel::FakeFrontendChannel() {}
FakeFrontendChannel::~FakeFrontendChannel() {}

int FakeFrontendChannel::CountProtocolNotificationMessageStartsWith(
    const std::string& message) {
  int count = 0;
  for (const std::string& s : protocol_notification_messages_) {
    if (base::StartsWith(s, message, base::CompareCase::SENSITIVE))
      count++;
  }
  return count;
}
int FakeFrontendChannel::CountProtocolNotificationMessage(
    const std::string& message) {
  return std::count(protocol_notification_messages_.begin(),
                    protocol_notification_messages_.end(), message);
}

namespace {
std::string SerializeToJSON(std::unique_ptr<protocol::Serializable> message) {
  std::vector<uint8_t> cbor = std::move(*message).TakeSerialized();
  std::string json;
  crdtp::Status status = ConvertCBORToJSON(crdtp::SpanFrom(cbor), &json);
  DCHECK(status.ok()) << status.ToASCIIString();
  return json;
}
}  // namespace

void FakeFrontendChannel::sendProtocolNotification(
    std::unique_ptr<protocol::Serializable> message) {
  EXPECT_TRUE(allow_notifications_);
  protocol_notification_messages_.push_back(
      SerializeToJSON(std::move(message)));
}

}  // namespace ui_devtools
