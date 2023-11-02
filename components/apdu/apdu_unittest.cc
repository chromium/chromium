// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/apdu/apdu_command.h"
#include "components/apdu/apdu_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apdu {

TEST(ApduTest, TestDeserializeBasic) {
  uint8_t cla = 0xAA;
  uint8_t ins = 0xAB;
  uint8_t p1 = 0xAC;
  uint8_t p2 = 0xAD;
  std::vector<uint8_t> message({cla, ins, p1, p2});
  const auto cmd = ApduCommand::CreateFromMessage(message);
  ASSERT_TRUE(cmd);
  EXPECT_EQ(0u, cmd->response_length_);
  EXPECT_TRUE(cmd->data_.empty());
  EXPECT_EQ(cla, cmd->cla_);
  EXPECT_EQ(ins, cmd->ins_);
  EXPECT_EQ(p1, cmd->p1_);
  EXPECT_EQ(p2, cmd->p2_);
  // Invalid length.
  message = {cla, ins, p1};
  EXPECT_FALSE(ApduCommand::CreateFromMessage(message));
  message.push_back(p2);
  message.push_back(0);
  // Set APDU command data size as maximum.
  message.push_back(0xFF);
  message.push_back(0xFF);
  message.resize(message.size() + ApduCommand::kApduMaxDataLength);
  // Set maximum response size.
  message.push_back(0);
  message.push_back(0);
  // |message| is APDU encoded byte array with maximum data length.
  EXPECT_TRUE(ApduCommand::CreateFromMessage(message));
  message.push_back(0);
  // |message| encoding containing data of size  maximum data length + 1.
  EXPECT_FALSE(ApduCommand::CreateFromMessage(message));
}

TEST(ApduTest, TestDeserializeComplex) {
  uint8_t cla = 0xAA;
  uint8_t ins = 0xAB;
  uint8_t p1 = 0xAC;
  uint8_t p2 = 0xAD;
  std::vector<uint8_t> data(
      ApduCommand::kApduMaxDataLength - ApduCommand::kApduMaxHeader - 2, 0x7F);
  std::vector<uint8_t> message = {cla, ins, p1, p2, 0};
  message.push_back((data.size() >> 8) & 0xff);
  message.push_back(data.size() & 0xff);
  message.insert(message.end(), data.begin(), data.end());

  // Create a message with no response expected.
  const auto cmd_no_response = ApduCommand::CreateFromMessage(message);
  ASSERT_TRUE(cmd_no_response);
  EXPECT_EQ(0u, cmd_no_response->response_length_);
  EXPECT_THAT(data, ::testing::ContainerEq(cmd_no_response->data_));
  EXPECT_EQ(cla, cmd_no_response->cla_);
  EXPECT_EQ(ins, cmd_no_response->ins_);
  EXPECT_EQ(p1, cmd_no_response->p1_);
  EXPECT_EQ(p2, cmd_no_response->p2_);

  // Add response length to message.
  message.push_back(0xF1);
  message.push_back(0xD0);
  const auto cmd = ApduCommand::CreateFromMessage(message);
  ASSERT_TRUE(cmd);
  EXPECT_THAT(data, ::testing::ContainerEq(cmd->data_));
  EXPECT_EQ(cla, cmd->cla_);
  EXPECT_EQ(ins, cmd->ins_);
  EXPECT_EQ(p1, cmd->p1_);
  EXPECT_EQ(p2, cmd->p2_);
  EXPECT_EQ(static_cast<size_t>(0xF1D0), cmd->response_length_);
}

TEST(ApduTest, TestDeserializeResponse) {
  ApduResponse::Status status;
  std::vector<uint8_t> test_vector;
  // Invalid length.
  std::vector<uint8_t> message({0xAA});
  EXPECT_FALSE(ApduResponse::CreateFromMessage(message));
  // Valid length and status.
  status = ApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED;
  message = {static_cast<uint8_t>(static_cast<uint16_t>(status) >> 8),
             static_cast<uint8_t>(status)};
  auto response = ApduResponse::CreateFromMessage(message);
  ASSERT_TRUE(response);
  EXPECT_EQ(ApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED,
            response->response_status_);
  EXPECT_THAT(response->data_, ::testing::ContainerEq(std::vector<uint8_t>()));
  // Valid length and status.
  status = ApduResponse::Status::SW_NO_ERROR;
  message = {static_cast<uint8_t>(static_cast<uint16_t>(status) >> 8),
             static_cast<uint8_t>(status)};
  test_vector = {0x01, 0x02, 0xEF, 0xFF};
  message.insert(message.begin(), test_vector.begin(), test_vector.end());
  response = ApduResponse::CreateFromMessage(message);
  ASSERT_TRUE(response);
  EXPECT_EQ(ApduResponse::Status::SW_NO_ERROR, response->response_status_);
  EXPECT_THAT(response->data_, ::testing::ContainerEq(test_vector));
}
TEST(ApduTest, TestSerializeCommand) {
  ApduCommand cmd;
  cmd.set_cla(0xA);
  cmd.set_ins(0xB);
  cmd.set_p1(0xC);
  cmd.set_p2(0xD);
  // No data, no response expected.
  std::vector<uint8_t> expected({0xA, 0xB, 0xC, 0xD});
  ASSERT_THAT(expected, ::testing::ContainerEq(cmd.GetEncodedCommand()));
  auto deserialized_cmd = ApduCommand::CreateFromMessage(expected);
  ASSERT_TRUE(deserialized_cmd);
  EXPECT_THAT(expected,
              ::testing::ContainerEq(deserialized_cmd->GetEncodedCommand()));
  // No data, response expected.
  cmd.set_response_length(0xCAFE);
  expected = {0xA, 0xB, 0xC, 0xD, 0x0, 0xCA, 0xFE};
  EXPECT_THAT(expected, ::testing::ContainerEq(cmd.GetEncodedCommand()));
  deserialized_cmd = ApduCommand::CreateFromMessage(expected);
  ASSERT_TRUE(deserialized_cmd);
  EXPECT_THAT(expected,
              ::testing::ContainerEq(deserialized_cmd->GetEncodedCommand()));
  // Data exists, response expected.
  std::vector<uint8_t> data({0x1, 0x2, 0x3, 0x4});
  cmd.set_data(data);
  expected = {0xA, 0xB, 0xC, 0xD, 0x0,  0x0, 0x4,
              0x1, 0x2, 0x3, 0x4, 0xCA, 0xFE};
  EXPECT_THAT(expected, ::testing::ContainerEq(cmd.GetEncodedCommand()));
  deserialized_cmd = ApduCommand::CreateFromMessage(expected);
  ASSERT_TRUE(deserialized_cmd);
  EXPECT_THAT(expected,
              ::testing::ContainerEq(deserialized_cmd->GetEncodedCommand()));
  // Data exists, no response expected.
  cmd.set_response_length(0);
  expected = {0xA, 0xB, 0xC, 0xD, 0x0, 0x0, 0x4, 0x1, 0x2, 0x3, 0x4};
  EXPECT_THAT(expected, ::testing::ContainerEq(cmd.GetEncodedCommand()));
  EXPECT_THAT(
      expected,
      ::testing::ContainerEq(
          ApduCommand::CreateFromMessage(expected)->GetEncodedCommand()));
}
TEST(ApduTest, TestSerializeEdgeCases) {
  ApduCommand cmd;
  cmd.set_cla(0xA);
  cmd.set_ins(0xB);
  cmd.set_p1(0xC);
  cmd.set_p2(0xD);
  // Set response length to maximum, which should serialize to 0x0000.
  cmd.set_response_length(ApduCommand::kApduMaxResponseLength);
  std::vector<uint8_t> expected({0xA, 0xB, 0xC, 0xD, 0x0, 0x0, 0x0});
  EXPECT_THAT(expected, ::testing::ContainerEq(cmd.GetEncodedCommand()));
  auto deserialized_cmd = ApduCommand::CreateFromMessage(expected);
  ASSERT_TRUE(deserialized_cmd);
  EXPECT_THAT(expected,
              ::testing::ContainerEq(deserialized_cmd->GetEncodedCommand()));
  // Maximum data size.
  std::vector<uint8_t> oversized(ApduCommand::kApduMaxDataLength);
  cmd.set_data(oversized);
  deserialized_cmd = ApduCommand::CreateFromMessage(cmd.GetEncodedCommand());
  ASSERT_TRUE(deserialized_cmd);
  EXPECT_THAT(cmd.GetEncodedCommand(),
              ::testing::ContainerEq(deserialized_cmd->GetEncodedCommand()));
}

}  // namespace apdu
