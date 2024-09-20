// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/boca/babelorca/cpp/proto_http_stream_parser.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/strings/strcat.h"
#include "chromeos/ash/components/boca/babelorca/proto/stream_body.pb.h"
#include "chromeos/ash/components/boca/babelorca/proto/testing_message.pb.h"
#include "chromeos/ash/services/boca/babelorca/mojom/tachyon_parsing_service.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::babelorca {
namespace {

constexpr char kMessage1[] = "message1";
constexpr char kMessage2[] = "message2";
constexpr char kMessage3[] = "message3";

struct ProtoHttpStreamParserTestCase {
  std::string test_name;
  std::string stream_string;
};

using ProtoHttpStreamParserTest =
    testing::TestWithParam<ProtoHttpStreamParserTestCase>;

std::vector<std::string> AppendAndCheckState(
    ProtoHttpStreamParser* stream_parser,
    std::string_view stream_string,
    mojom::ParsingState expected_state) {
  mojom::ParsingState state = stream_parser->Append(stream_string);
  std::vector<std::string> results = stream_parser->TakeParseResult();
  EXPECT_EQ(state, expected_state);
  EXPECT_TRUE(stream_parser->TakeParseResult().empty());
  return results;
}

TEST(ProtoHttpStreamParserTest, OneMessageOverOneChunk) {
  StreamBody stream_body;
  stream_body.add_messages(kMessage1);

  ProtoHttpStreamParser stream_parser;
  std::vector<std::string> results =
      AppendAndCheckState(&stream_parser, stream_body.SerializeAsString(),
                          /*expected_state=*/mojom::ParsingState::kOk);

  ASSERT_THAT(results, testing::SizeIs(1));
  EXPECT_THAT(results[0], testing::StrEq(kMessage1));
}

TEST(ProtoHttpStreamParserTest, OneMessageOverThreeChunks) {
  StreamBody stream_body;
  stream_body.add_messages(kMessage1);
  std::string stream_string = stream_body.SerializeAsString();

  ProtoHttpStreamParser stream_parser;
  std::vector<std::string> results1 =
      AppendAndCheckState(&stream_parser, stream_string.substr(0, 1),
                          /*expected_state=*/mojom::ParsingState::kOk);
  std::vector<std::string> results2 = AppendAndCheckState(
      &stream_parser, stream_string.substr(1, stream_string.length() / 2),
      /*expected_state=*/mojom::ParsingState::kOk);
  std::vector<std::string> results3 = AppendAndCheckState(
      &stream_parser, stream_string.substr(1 + (stream_string.length() / 2)),
      /*expected_state=*/mojom::ParsingState::kOk);

  EXPECT_TRUE(results1.empty());
  EXPECT_TRUE(results2.empty());
  ASSERT_THAT(results3, testing::SizeIs(1));
  EXPECT_THAT(results3[0], testing::StrEq(kMessage1));
}

TEST(ProtoHttpStreamParserTest, MultipleMessagesOverOneChunk) {
  StreamBody stream_body;
  stream_body.add_messages(kMessage1);
  stream_body.add_messages(kMessage2);
  stream_body.add_messages(kMessage3);
  std::string stream_string = stream_body.SerializeAsString();

  ProtoHttpStreamParser stream_parser;
  std::vector<std::string> results =
      AppendAndCheckState(&stream_parser, stream_body.SerializeAsString(),
                          /*expected_state=*/mojom::ParsingState::kOk);

  ASSERT_THAT(results, testing::SizeIs(3));
  EXPECT_THAT(results[0], testing::StrEq(kMessage1));
  EXPECT_THAT(results[1], testing::StrEq(kMessage2));
  EXPECT_THAT(results[2], testing::StrEq(kMessage3));
}

TEST(ProtoHttpStreamParserTest, OneBytePerChunk) {
  StreamBody stream_body;
  // Simulate 2 streamed messages separated by a noop.
  stream_body.add_messages(kMessage1);
  std::string stream_string_part1 = stream_body.SerializeAsString();
  stream_body.Clear();
  stream_body.add_noop("123456789");
  std::string stream_string_part2 = stream_body.SerializeAsString();
  stream_body.Clear();
  stream_body.add_messages(kMessage2);
  std::string stream_string_part3 = stream_body.SerializeAsString();
  std::string stream_string = base::StrCat(
      {stream_string_part1, stream_string_part2, stream_string_part3});

  ProtoHttpStreamParser stream_parser;

  for (auto stream_byte : stream_string) {
    stream_parser.Append(std::string({stream_byte}));
  }
  std::vector<std::string> results = stream_parser.TakeParseResult();

  ASSERT_THAT(results, testing::SizeIs(2));
  EXPECT_THAT(results[0], testing::StrEq(kMessage1));
  EXPECT_THAT(results[1], testing::StrEq(kMessage2));
}

TEST(ProtoHttpStreamParserTest, StatusStreamed) {
  StreamBody stream_body;
  stream_body.mutable_status()->set_code(1);
  stream_body.mutable_status()->set_message("closed");
  std::string stream_string = stream_body.SerializeAsString();

  ProtoHttpStreamParser stream_parser;
  std::vector<std::string> results1 = AppendAndCheckState(
      &stream_parser, stream_string.substr(0, stream_string.length() / 2),
      /*expected_state=*/mojom::ParsingState::kOk);
  std::vector<std::string> results2 = AppendAndCheckState(
      &stream_parser, stream_string.substr(stream_string.length() / 2),
      /*expected_state=*/mojom::ParsingState::kClosed);
  // New data is not accepted after stream is closed.
  StreamBody new_stream_body;
  new_stream_body.add_messages(kMessage1);
  std::vector<std::string> results3 =
      AppendAndCheckState(&stream_parser, new_stream_body.SerializeAsString(),
                          /*expected_state=*/mojom::ParsingState::kClosed);

  EXPECT_TRUE(results1.empty());
  ASSERT_THAT(results2, testing::SizeIs(1));
  EXPECT_THAT(results2[0],
              testing::StrEq(stream_body.status().SerializeAsString()));
  EXPECT_TRUE(results3.empty());
}

TEST(ProtoHttpStreamParserTest, StopAcceptingDataOnError) {
  ProtoHttpStreamParser stream_parser;
  // Pass an invalid field number
  std::vector<std::string> results1 =
      AppendAndCheckState(&stream_parser, std::string({1}),
                          /*expected_state=*/mojom::ParsingState::kError);
  // Pass valid data.
  StreamBody stream_body;
  stream_body.add_messages(kMessage1);
  std::vector<std::string> results2 =
      AppendAndCheckState(&stream_parser, stream_body.SerializeAsString(),
                          /*expected_state=*/mojom::ParsingState::kError);

  EXPECT_TRUE(results1.empty());
  EXPECT_TRUE(results2.empty());
}

TEST(ProtoHttpStreamParserTest, SkipUnknownFields) {
  TestingMessage unknown_fields;
  unknown_fields.set_int64_field(11);
  unknown_fields.set_fixed32_field(11);
  unknown_fields.set_fixed64_field(11);
  StreamBody stream_body;
  stream_body.add_messages(kMessage1);
  std::string stream_string = base::StrCat(
      {unknown_fields.SerializeAsString(), stream_body.SerializeAsString()});

  ProtoHttpStreamParser stream_parser;
  for (auto stream_byte : stream_string) {
    mojom::ParsingState state =
        stream_parser.Append(std::string({stream_byte}));
    ASSERT_EQ(state, mojom::ParsingState::kOk);
  }
  std::vector<std::string> results = stream_parser.TakeParseResult();

  ASSERT_THAT(results, testing::SizeIs(1));
  EXPECT_EQ(results[0], kMessage1);
}

TEST(ProtoHttpStreamParserTest, FailsIfPendingDataExceededMaxPendingSize) {
  StreamBody stream_body;
  stream_body.add_messages(kMessage1);
  stream_body.add_messages(kMessage2);
  const std::string stream_string = stream_body.SerializeAsString();

  ProtoHttpStreamParser stream_parser(/*max_pending_size=*/5);
  // Pass the data except for the last byte to cause 2nd message to be pending.
  std::vector<std::string> results = AppendAndCheckState(
      &stream_parser, stream_string.substr(0, stream_string.length() - 1),
      /*expected_state=*/mojom::ParsingState::kError);

  ASSERT_THAT(results, testing::SizeIs(1));
  EXPECT_EQ(results[0], kMessage1);
}

TEST_P(ProtoHttpStreamParserTest, ParseFailure) {
  ProtoHttpStreamParser stream_parser;

  std::vector<std::string> results =
      AppendAndCheckState(&stream_parser, GetParam().stream_string,
                          /*expected_state=*/mojom::ParsingState::kError);

  EXPECT_TRUE(results.empty());
}

INSTANTIATE_TEST_SUITE_P(
    ProtoHttpStreamParserTestSuiteInstantiation,
    ProtoHttpStreamParserTest,
    testing::ValuesIn<ProtoHttpStreamParserTestCase>({
        // Tag byte is (field_number << 3) | wire_type
        {"InvalidWireType", std::string({(1 << 3) | 6})},
        {"SgroupTypeNotSupported", std::string({(1 << 3) | 3})},
        {"EgroupTypeNotSupported", std::string({(1 << 3) | 4})},
        {"InvalidFieldNumber", std::string({2})},
        {"UnexpectedWireType", std::string({1 << 3})},
    }),
    [](const testing::TestParamInfo<ProtoHttpStreamParserTest::ParamType>&
           info) { return info.param.test_name; });

}  // namespace
}  // namespace ash::babelorca
