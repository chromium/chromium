// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/parse_jspb.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "components/autofill_assistant/browser/parse_jspb_test.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::ElementsAre;
using ::testing::FloatEq;

namespace autofill_assistant {
namespace {

bool ParseTestProto(const std::string& json, testing::TestProto* proto) {
  absl::optional<base::Value> value = base::JSONReader::Read(json);
  if (!value) {
    LOG(ERROR) << "Invalid JSON: " << json;
    return false;
  }
  absl::optional<std::string> bytes =
      ParseJspb("test.", *value, /* error_message= */ nullptr);
  if (!bytes) {
    LOG(ERROR) << "Cannot transform to binary: " << json;
    return false;
  }
  if (!proto->ParseFromString(*bytes)) {
    LOG(ERROR) << "Cannot parse from binary: " << json;
    return false;
  }
  return true;
}

TEST(ParseJspbTest, ParseEmptyMessage) {
  testing::TestProto message;
  EXPECT_TRUE(ParseTestProto(R"(["test.Test"])", &message));
  EXPECT_TRUE(ParseTestProto(R"(["test.Test", {}])", &message));
}

TEST(ParseJspbTest, ParseNumbers) {
  testing::TestProto message;
  EXPECT_TRUE(ParseTestProto(R"(["test.Test", -12, 2000])", &message));
  EXPECT_EQ(-12, message.int32_field());
  EXPECT_EQ(2000, message.int64_field());
}

TEST(ParseJspbTest, ParseEnum) {
  testing::TestProto message;
  EXPECT_TRUE(ParseTestProto(R"(["test.Test", null, null, 1])", &message));
  EXPECT_EQ(testing::TestProto::MY_ENUM_1, message.enum_field());
}

TEST(ParseJspbTest, ParseTrue) {
  testing::TestProto message;
  EXPECT_TRUE(
      ParseTestProto(R"(["test.Test", null, null, null, true])", &message));
  EXPECT_TRUE(message.bool_field());
}

TEST(ParseJspbTest, ParseFalse) {
  testing::TestProto message;
  EXPECT_TRUE(
      ParseTestProto(R"(["test.Test", null, null, null, false])", &message));
  EXPECT_TRUE(message.has_bool_field());
  EXPECT_FALSE(message.bool_field());
}

TEST(ParseJspbTest, ParseNonzeroAsBool) {
  testing::TestProto message;
  EXPECT_TRUE(
      ParseTestProto(R"(["test.Test", null, null, null, 12])", &message));
  EXPECT_TRUE(message.bool_field());
}

TEST(ParseJspbTest, ParseZeroAsBool) {
  testing::TestProto message;
  EXPECT_TRUE(
      ParseTestProto(R"(["test.Test", null, null, null, 0])", &message));
  EXPECT_TRUE(message.has_bool_field());
  EXPECT_FALSE(message.bool_field());
}

TEST(ParseJspbTest, ParseString) {
  testing::TestProto message;
  EXPECT_TRUE(ParseTestProto(
      R"(["test.Test", null, null, null, null, "foobar"])", &message));
  EXPECT_EQ("foobar", message.string_field());
}

TEST(ParseJspbTest, ParseInner) {
  testing::TestProto message;
  EXPECT_TRUE(ParseTestProto(
      R"(["test.Test", null, null, null, null, null, ["test.Test", 2]])",
      &message));
  EXPECT_TRUE(message.has_inner());
  EXPECT_EQ(2, message.inner().int32_field());
}

TEST(ParseJspbTest, ParseInvalidInner) {
  // The inner message is missing a message id. It will be parsed as an array
  // and skipped during proto parsing.
  testing::TestProto message;
  ParseTestProto(R"(["test.Test", null, null, null, null, null, [1, 2]])",
                 &message);
  EXPECT_FALSE(message.has_inner());
}

TEST(ParseJspbTest, ParseFromDict) {
  testing::TestProto message;
  EXPECT_TRUE(ParseTestProto(R"(["test.Test", {"5": "foobar"}])", &message));
  EXPECT_EQ("foobar", message.string_field());
}

TEST(ParseJspbTest, ParseRepeated) {
  testing::TestProto message;
  EXPECT_TRUE(ParseTestProto(R"(["test.Test", {"7": ["one", "two", "three"]}])",
                             &message));
  EXPECT_THAT(message.repeated_string_field(),
              ElementsAre("one", "two", "three"));
}

TEST(ParseJspbTest, ParseFloat) {
  testing::TestProto message;
  EXPECT_TRUE(ParseTestProto(R"(["test.Test", {"8": 3.14}])", &message));
  EXPECT_THAT(message.float_field(), FloatEq(3.14f));
}

TEST(ParseJspbTest, DoubleNotSupported) {
  testing::TestProto message;
  ParseTestProto(R"(["test.Test", {"9": 3.14}])", &message);
  EXPECT_FALSE(message.has_float_field());
}

TEST(ParseJspbTest, ParseBytes) {
  testing::TestProto message;
  ParseTestProto(R"(["test.Test", {"16": "dGVzdAo="}])", &message);

  // This isn't correct: it should have decoded the base64 and the bytes field
  // should contain 'test', but the parser has no way of knowing that.
  EXPECT_EQ("dGVzdAo=", message.bytes_field());
}

TEST(ParseJspbTest, UnsupportedFixedNumberTypes) {
  testing::TestProto message;
  ParseTestProto(
      R"(["test.Unsupported", {"10": 1, "11": 2, "12": 3, "13": 4}])",
      &message);
  EXPECT_FALSE(message.has_fixed32_field());
  EXPECT_FALSE(message.has_sfixed32_field());
  EXPECT_FALSE(message.has_fixed64_field());
  EXPECT_FALSE(message.has_sfixed64_field());
}

TEST(ParseJspbTest, UnsignedVarints) {
  testing::TestProto message;
  EXPECT_TRUE(
      ParseTestProto(R"(["test.Unsupported", {"14": 1, "15": 2}])", &message));
  EXPECT_EQ(1u, message.uint32_field());
  EXPECT_EQ(2u, message.uint64_field());
}

TEST(ParseJspbTest, UnsignedVarintsTooLarge) {
  testing::TestProto message;
  ParseTestProto(
      R"(["test.Unsupported", {"14": 4294967296, "15": 9223372036854775808 }])",
      &message);
  EXPECT_FALSE(message.has_uint32_field());
  EXPECT_FALSE(message.has_uint64_field());
}

// Makes sure that |json| contains JSON data that's rejected by ParseJspb().
bool InUnparseable(const std::string& json) {
  absl::optional<base::Value> value = base::JSONReader::Read(json);
  if (!value) {
    LOG(ERROR) << "Invalid JSON: " << json;
    return false;
  }
  absl::optional<std::string> bytes =
      ParseJspb("test.", *value, /* error_message= */ nullptr);
  // This should return absl::nullopt
  return !bytes;
}

TEST(ParseJspbTest, InvalidJsonRepresentation) {
  EXPECT_TRUE(InUnparseable(R"("invalid")"));
  EXPECT_TRUE(InUnparseable("{}"));
  EXPECT_TRUE(InUnparseable("[]"));
  EXPECT_TRUE(InUnparseable("[1, 2]"));
  EXPECT_TRUE(InUnparseable(R"([{"invalid": 1}])"));
  EXPECT_TRUE(InUnparseable(R"(["invalid"])"));
}

}  // namespace
}  // namespace autofill_assistant
