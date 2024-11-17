// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/ui_devtools_unittest_utils.h"

#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "third_party/inspector_protocol/crdtp/json.h"

namespace ui_devtools {

MockUIElementDelegate::MockUIElementDelegate() = default;
MockUIElementDelegate::~MockUIElementDelegate() = default;

FakeFrontendChannel::FakeFrontendChannel() = default;
FakeFrontendChannel::~FakeFrontendChannel() = default;

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
  return base::ranges::count(protocol_notification_messages_, message);
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

void FakeUIElement::GetBounds(gfx::Rect* bounds) const {
  *bounds = bounds_;
}
void FakeUIElement::SetBounds(const gfx::Rect& bounds) {
  bounds_ = bounds;
}
void FakeUIElement::GetVisible(bool* visible) const {
  *visible = visible_;
}
void FakeUIElement::SetVisible(bool visible) {
  visible_ = visible;
}
std::vector<std::string> FakeUIElement::GetAttributes() const {
  return {};
}
std::pair<gfx::NativeWindow, gfx::Rect>
FakeUIElement::GetNodeWindowAndScreenBounds() const {
  return {};
}
void FakeUIElement::AddSource(std::string path, int line) {
  UIElement::AddSource(path, line);
}

}  // namespace ui_devtools
