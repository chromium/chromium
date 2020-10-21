// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/decoder/proto_conversion.h"

#include "chromeos/services/ime/public/proto/messages.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace ime {

TEST(ProtoConversionTest, OnFocusToProto) {
  ime::PublicMessage expected_message;
  expected_message.set_seq_id(42);
  *expected_message.mutable_on_focus() = ime::OnFocus();

  ime::PublicMessage actual_message = OnFocusToProto(/*seq_id=*/42);

  EXPECT_EQ(actual_message.SerializeAsString(),
            expected_message.SerializeAsString());
}

TEST(ProtoConversionTest, OnKeyEventToProto) {
  auto modifier_state = mojom::ModifierState::New();
  modifier_state->shift = true;
  auto key_event = mojom::PhysicalKeyEvent::New(
      mojom::KeyEventType::kKeyDown, "KeyA", "A", std::move(modifier_state));

  ime::PublicMessage expected_message;
  expected_message.set_seq_id(42);
  ime::OnKeyEvent& args = *expected_message.mutable_on_key_event();
  args.mutable_key_event()->set_type(
      ime::PhysicalKeyEvent::EVENT_TYPE_KEY_DOWN);
  args.mutable_key_event()->set_code("KeyA");
  args.mutable_key_event()->set_key("A");
  args.mutable_key_event()->mutable_modifier_state()->set_alt(false);
  args.mutable_key_event()->mutable_modifier_state()->set_alt_graph(false);
  args.mutable_key_event()->mutable_modifier_state()->set_caps_lock(false);
  args.mutable_key_event()->mutable_modifier_state()->set_control(false);
  args.mutable_key_event()->mutable_modifier_state()->set_meta(false);
  args.mutable_key_event()->mutable_modifier_state()->set_shift(true);

  const ime::PublicMessage actual_message =
      OnKeyEventToProto(/*seq_id=*/42, std::move(key_event));

  EXPECT_EQ(actual_message.SerializeAsString(),
            expected_message.SerializeAsString());
}

}  // namespace ime
}  // namespace chromeos
