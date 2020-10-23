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

TEST(ProtoConversionTest, OnBlurToProto) {
  ime::PublicMessage expected_message;
  expected_message.set_seq_id(42);
  *expected_message.mutable_on_blur() = ime::OnBlur();

  ime::PublicMessage actual_message = OnBlurToProto(/*seq_id=*/42);

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

TEST(ProtoConversionTest, OnSurroundingTextChangedToProto) {
  const auto selection = mojom::SelectionRange::New(/*anchor=*/3, /*focus=*/2);

  ime::PublicMessage expected_message;
  expected_message.set_seq_id(42);
  expected_message.mutable_on_surrounding_text_changed()->set_text("hello");
  expected_message.mutable_on_surrounding_text_changed()->set_offset(1);
  expected_message.mutable_on_surrounding_text_changed()
      ->mutable_selection_range()
      ->set_anchor(3);
  expected_message.mutable_on_surrounding_text_changed()
      ->mutable_selection_range()
      ->set_focus(2);

  ime::PublicMessage actual_message = OnSurroundingTextChangedToProto(
      /*seq_id=*/42, "hello", /*offset=*/1, selection->Clone());

  EXPECT_EQ(actual_message.SerializeAsString(),
            expected_message.SerializeAsString());
}

}  // namespace ime
}  // namespace chromeos
