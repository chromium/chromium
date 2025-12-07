// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/multimodal_message.h"

#include <optional>

#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/request_builder.h"
#include "components/optimization_guide/proto/features/example_for_testing.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

using RequestProto = ::optimization_guide::proto::ExampleForTestingRequest;
using NestedProto = ::optimization_guide::proto::ExampleForTestingMessage;

TEST(MultimodalMessageTest, InvalidField) {
  auto field = ProtoField({999});
  MultimodalMessage builder{RequestProto()};
  EXPECT_FALSE(builder.read().GetValue(field));
  EXPECT_FALSE(builder.read().GetImage(field));
  EXPECT_EQ(builder.read().GetRepeated(field)->Size(), 0);
}

TEST(MultimodalMessageTest, InvalidNestedField) {
  auto field = ProtoField({999, 1});
  MultimodalMessage builder{RequestProto()};
  EXPECT_FALSE(builder.read().GetValue(field));
  EXPECT_FALSE(builder.read().GetImage(field));
  EXPECT_FALSE(builder.read().GetRepeated(field));
}

TEST(MultimodalMessageTest, StringField) {
  auto field = ProtoField({RequestProto::kStringValueFieldNumber});
  MultimodalMessage builder{RequestProto()};
  EXPECT_EQ(builder.read().GetValue(field).value().string_value(), "");
  EXPECT_FALSE(builder.read().GetImage(field));
  EXPECT_EQ(builder.read().GetRepeated(field)->Size(), 0);
  builder.edit().Set(RequestProto::kStringValueFieldNumber, "my_string_value");
  EXPECT_EQ(builder.read().GetValue(field).value().string_value(),
            "my_string_value");
  EXPECT_FALSE(builder.read().GetImage(field));
  EXPECT_EQ(builder.read().GetRepeated(field)->Size(), 0);
}

TEST(MultimodalMessageTest, ImageField) {
  auto field = ProtoField(
      {RequestProto::kNested1FieldNumber, NestedProto::kMediaFieldNumber});
  MultimodalMessage builder{RequestProto()};
  EXPECT_FALSE(builder.read().GetValue(field));
  EXPECT_FALSE(builder.read().GetImage(field));
  EXPECT_EQ(builder.read().GetRepeated(field)->Size(), 0);
  builder.edit()
      .GetMutableMessage(RequestProto::kNested1FieldNumber)
      .Set(NestedProto::kMediaFieldNumber, CreateBlackSkBitmap(1, 1));
  EXPECT_FALSE(builder.read().GetValue(field));
  EXPECT_TRUE(builder.read().GetImage(field));
  EXPECT_EQ(builder.read().GetRepeated(field)->Size(), 0);
  EXPECT_EQ(builder.read().GetImage(field)->width(), 1);
  builder.edit()
      .GetMutableMessage(RequestProto::kNested1FieldNumber)
      .Set(NestedProto::kMediaFieldNumber, CreateBlackSkBitmap(2, 2));
  EXPECT_EQ(builder.read().GetImage(field)->width(), 2);
}

TEST(MultimodalMessageTest, ImageFieldInRepeated) {
  auto repeated = ProtoField({RequestProto::kRepeatedFieldFieldNumber});
  auto field = ProtoField({NestedProto::kMediaFieldNumber});
  MultimodalMessage builder{RequestProto()};
  EXPECT_EQ(builder.read().GetRepeated(repeated)->Size(), 0);
  builder.edit()
      .MutableRepeatedField(RequestProto::kRepeatedFieldFieldNumber)
      .Add()
      .Set(NestedProto::kMediaFieldNumber, CreateBlackSkBitmap(1, 1));
  EXPECT_EQ(builder.read().GetRepeated(repeated)->Size(), 1);
  EXPECT_TRUE(builder.read().GetRepeated(repeated)->Get(0).GetImage(field));
}

// Test that fields of nested messages can be initialized and read.
TEST(MultimodalMessageTest, NestedMessage) {
  auto nested_field = ProtoField({RequestProto::kNested1FieldNumber});
  auto bool_field = ProtoField(
      {RequestProto::kNested1FieldNumber, NestedProto::kBoolValueFieldNumber});
  auto string_field = ProtoField({RequestProto::kNested1FieldNumber,
                                  NestedProto::kStringValueFieldNumber});
  auto enum_field = ProtoField(
      {RequestProto::kNested1FieldNumber, NestedProto::kEnumValueFieldNumber});
  MultimodalMessage builder{RequestProto()};

  EXPECT_FALSE(builder.read().GetValue(nested_field));
  EXPECT_FALSE(builder.read().GetImage(nested_field));
  EXPECT_EQ(builder.read().GetRepeated(nested_field)->Size(), 0);
  EXPECT_EQ(builder.read().GetValue(bool_field).value().boolean_value(), false);
  EXPECT_EQ(builder.read().GetValue(string_field).value().string_value(), "");
  EXPECT_EQ(builder.read().GetValue(enum_field).value().int32_value(),
            proto::ExampleForTestingMessage::VALUE0);

  proto::ExampleForTestingMessage nested;
  nested.set_enum_value(proto::ExampleForTestingMessage::VALUE1);

  MultimodalMessageEditView nested_view =
      builder.edit().GetMutableMessage(RequestProto::kNested1FieldNumber);
  nested_view.CheckTypeAndMergeFrom(nested);
  nested_view.Set(NestedProto::kStringValueFieldNumber, "my_string_value");

  EXPECT_EQ(builder.read().GetValue(bool_field).value().boolean_value(), false);
  EXPECT_EQ(builder.read().GetValue(string_field).value().string_value(),
            "my_string_value");
  EXPECT_EQ(builder.read().GetValue(enum_field).value().int32_value(),
            proto::ExampleForTestingMessage::VALUE1);
}

// Tests the behavior of CheckTypeAndMergeFrom on a message with a nested field.
TEST(MultimodalMessageTest, MergeWithNestedField) {
  auto nested_field = ProtoField({RequestProto::kNested1FieldNumber});
  auto bool_field = ProtoField(
      {RequestProto::kNested1FieldNumber, NestedProto::kBoolValueFieldNumber});
  auto string_field = ProtoField({RequestProto::kNested1FieldNumber,
                                  NestedProto::kStringValueFieldNumber});
  auto image_field = ProtoField(
      {RequestProto::kNested1FieldNumber, NestedProto::kMediaFieldNumber});
  MultimodalMessage builder{RequestProto()};
  builder.edit()
      .GetMutableMessage(RequestProto::kNested1FieldNumber)
      .Set(NestedProto::kStringValueFieldNumber, "old_value");
  builder.edit()
      .GetMutableMessage(RequestProto::kNested1FieldNumber)
      .Set(NestedProto::kMediaFieldNumber, CreateBlackSkBitmap(1, 1));

  proto::ExampleForTestingRequest to_merge;
  to_merge.mutable_nested1()->set_bool_value(true);
  to_merge.mutable_nested1()->set_string_value("new_value");
  builder.edit().CheckTypeAndMergeFrom(to_merge);

  EXPECT_FALSE(builder.read().GetValue(nested_field));
  EXPECT_FALSE(builder.read().GetImage(nested_field));
  EXPECT_EQ(builder.read().GetRepeated(nested_field)->Size(), 0);
  EXPECT_EQ(builder.read().GetValue(bool_field).value().boolean_value(), true);
  EXPECT_EQ(builder.read().GetValue(string_field).value().string_value(),
            "new_value");
  EXPECT_TRUE(builder.read().GetImage(image_field));
}

TEST(MultimodalMessageTest, RepeatedMessages) {
  auto repeated = ProtoField({RequestProto::kRepeatedFieldFieldNumber});
  auto string_field = ProtoField({NestedProto::kStringValueFieldNumber});
  auto image_field = ProtoField({NestedProto::kMediaFieldNumber});

  proto::ExampleForTestingRequest initial;
  initial.add_repeated_field()->set_string_value("entry0");
  MultimodalMessage builder(initial);

  EXPECT_EQ(builder.read().GetRepeated(repeated)->Size(), 1);

  proto::ExampleForTestingMessage entry1;
  entry1.set_string_value("entry1");
  builder.edit()
      .MutableRepeatedField(RequestProto::kRepeatedFieldFieldNumber)
      .Add(entry1)
      .Set(NestedProto::kMediaFieldNumber, CreateBlackSkBitmap(1, 1));

  EXPECT_EQ(builder.read().GetRepeated(repeated)->Size(), 2);

  proto::ExampleForTestingRequest to_merge;
  to_merge.add_repeated_field()->set_string_value("entry2");
  builder.edit().CheckTypeAndMergeFrom(to_merge);

  EXPECT_EQ(builder.read().GetRepeated(repeated)->Size(), 3);

  builder.edit()
      .MutableRepeatedField(RequestProto::kRepeatedFieldFieldNumber)
      .Add()
      .Set(NestedProto::kStringValueFieldNumber, "entry3");

  std::optional<RepeatedMultimodalMessageReadView> repeated_view =
      builder.read().GetRepeated(repeated);

  EXPECT_EQ(repeated_view->Size(), 4);

  EXPECT_EQ(repeated_view->Get(0).GetValue(string_field).value().string_value(),
            "entry0");
  EXPECT_FALSE(repeated_view->Get(0).GetImage(image_field));
  EXPECT_EQ(repeated_view->Get(1).GetValue(string_field).value().string_value(),
            "entry1");
  EXPECT_TRUE(repeated_view->Get(1).GetImage(image_field));
  EXPECT_EQ(repeated_view->Get(2).GetValue(string_field).value().string_value(),
            "entry2");
  EXPECT_FALSE(repeated_view->Get(2).GetImage(image_field));
  EXPECT_EQ(repeated_view->Get(3).GetValue(string_field).value().string_value(),
            "entry3");
  EXPECT_FALSE(repeated_view->Get(3).GetImage(image_field));
}

TEST(MultimodalMessageTest, EmptyMerge) {
  auto string_field = ProtoField({RequestProto::kStringValueFieldNumber});
  MultimodalMessage empty;

  proto::ExampleForTestingRequest to_merge;
  to_merge.set_string_value("my_value");

  MultimodalMessage merged = empty.Merge(to_merge);

  EXPECT_EQ(merged.read().GetValue(string_field).value().string_value(),
            "my_value");
}

TEST(MultimodalMessageTest, NonEmptyMerge) {
  auto bool_field = ProtoField({RequestProto::kBoolValueFieldNumber});
  auto string_field = ProtoField({RequestProto::kStringValueFieldNumber});

  proto::ExampleForTestingRequest initial;
  initial.set_bool_value(true);
  MultimodalMessage non_empty(initial);

  proto::ExampleForTestingRequest to_merge;
  to_merge.set_string_value("my_value");

  MultimodalMessage merged = non_empty.Merge(to_merge);

  EXPECT_EQ(merged.read().GetValue(string_field).value().string_value(),
            "my_value");
  EXPECT_EQ(merged.read().GetValue(bool_field).value().boolean_value(), true);
}

}  // namespace

}  // namespace optimization_guide
