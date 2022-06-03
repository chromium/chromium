// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/ui_devtools_unittest_utils.h"

#include "base/strings/string_util.h"
#include "third_party/inspector_protocol/crdtp/json.h"

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

void FakeFrontendChannel::SendProtocolNotification(
    std::unique_ptr<protocol::Serializable> message) {
  EXPECT_TRUE(allow_notifications_);
  std::string json;
  crdtp::Status status = crdtp::json::ConvertCBORToJSON(
      crdtp::SpanFrom(message->Serialize()), &json);
  DCHECK(status.ok()) << status.ToASCIIString();
  protocol_notification_messages_.push_back(std::move(json));
}

}  // namespace ui_devtools
