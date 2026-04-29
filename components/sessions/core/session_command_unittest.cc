// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/session_command.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/numerics/byte_conversions.h"
#include "base/pickle.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sessions {
namespace {

using id_type = SessionCommand::id_type;
using size_type = SessionCommand::size_type;

TEST(SessionCommandTest, BasicConstructor) {
  const id_type id = 42;
  const size_type size = 10;
  SessionCommand command(id, size);

  EXPECT_EQ(id, command.id());
  EXPECT_EQ(size, command.contents().size());

  // Check that contents are initialized to 0.
  for (uint8_t byte : command.contents()) {
    EXPECT_EQ(0u, byte);
  }
}

TEST(SessionCommandTest, PickleConstructor) {
  const id_type id = 42;
  base::Pickle pickle;
  pickle.WriteString("hello");
  pickle.WriteInt(123);

  SessionCommand command(id, pickle);

  EXPECT_EQ(id, command.id());
  EXPECT_EQ(pickle.size(), command.contents().size());
  // Verify contents match pickle data.
  EXPECT_EQ(pickle.AsBytes(), command.contents());
}

TEST(SessionCommandTest, GetContents) {
  const id_type id = 42;
  const size_type size = 5;
  SessionCommand command(id, size);
  command.contents().copy_from(base::span<const uint8_t>({1, 2, 3, 4, 5}));

  std::vector<uint8_t> dest2(size);
  EXPECT_TRUE(command.GetContents(dest2.data(), dest2.size()));

  EXPECT_EQ(1u, dest2[0]);
  EXPECT_EQ(2u, dest2[1]);
  EXPECT_EQ(3u, dest2[2]);
  EXPECT_EQ(4u, dest2[3]);
  EXPECT_EQ(5u, dest2[4]);
}

class SessionCommandParamTest : public testing::TestWithParam<bool> {
 protected:
  bool encrypted() const { return GetParam(); }
  os_crypt_async::Encryptor* encryptor() {
    if (encrypted()) {
      if (!encryptor_) {
        encryptor_ = std::make_unique<os_crypt_async::TestEncryptor>(
            os_crypt_async::GetTestEncryptorForTesting());
      }
      return encryptor_.get();
    } else {
      return nullptr;
    }
  }

 private:
  std::unique_ptr<os_crypt_async::TestEncryptor> encryptor_;
};

TEST_P(SessionCommandParamTest, GetSerializedSize) {
  const id_type id = 42;
  const std::string contents = "session_data";
  SessionCommand command(id, contents.size());
  command.contents().copy_from(base::as_byte_span(contents));
  std::vector<uint8_t> serialized = command.Serialize(encryptor());

  // Not enough data for size (we need sizeof(uint32_t) for encrypted)
  const size_t size_field_size =
      encrypted() ? sizeof(uint32_t) : sizeof(uint16_t);
  EXPECT_EQ(std::nullopt, SessionCommand::GetSerializedSize(
                              base::span(serialized).first(size_field_size - 1),
                              encryptor()));

  // Enough data for size
  std::optional<size_t> size =
      SessionCommand::GetSerializedSize(serialized, encrypted());
  ASSERT_TRUE(size.has_value());
  EXPECT_EQ(serialized.size(), *size);

  // Exact size even if more data is available
  std::vector<uint8_t> padded_serialized = serialized;
  padded_serialized.push_back(0u);
  size = SessionCommand::GetSerializedSize(padded_serialized, encrypted());
  ASSERT_TRUE(size.has_value());
  EXPECT_EQ(serialized.size(), *size);
}

TEST(SessionCommandTest, GetContentsWithWrongSizeFeturnsFalse) {
  const id_type id = 42;
  const size_type size = 5;
  SessionCommand command(id, size);

  // Fill with known data.
  command.contents().copy_from(base::span<const uint8_t>({1, 2, 3, 4, 5}));

  // Wrong size should return false.
  std::vector<uint8_t> dest(size + 1);
  EXPECT_FALSE(command.GetContents(dest.data(), dest.size()));
}

TEST(SessionCommandTest, ContentsAsPickle) {
  base::Pickle pickle;
  pickle.WriteString("test_string");
  pickle.WriteInt(456);
  SessionCommand command(1, pickle);

  base::PickleIterator iter = command.ContentsAsPickle();
  std::string read_string;
  EXPECT_TRUE(iter.ReadString(&read_string));
  EXPECT_EQ("test_string", read_string);

  int read_int = 0;
  EXPECT_TRUE(iter.ReadInt(&read_int));
  EXPECT_EQ(456, read_int);
}

TEST_P(SessionCommandParamTest, SerializeAndDeserialize) {
  const id_type id = 42;
  const std::string contents = "session_data";
  SessionCommand command(id, contents.size());
  command.contents().copy_from(base::as_byte_span(contents));

  std::vector<uint8_t> serialized = command.Serialize(encryptor());

  if (encrypted()) {
    // Size should be: sizeof(encrypted_size_type) + encrypted_contents_size.
    // We don't know the exact size, but it should be larger than
    // contents.size().
    EXPECT_GT(serialized.size(),
              sizeof(uint32_t) + sizeof(id_type) + contents.size());
  } else {
    // Size should be: sizeof(size_type) + sizeof(id_type) + contents_size.
    EXPECT_EQ(sizeof(size_type) + sizeof(id_type) + contents.size(),
              serialized.size());
  }

  std::unique_ptr<SessionCommand> deserialized =
      SessionCommand::Deserialize(serialized, encryptor());
  ASSERT_TRUE(deserialized);
  EXPECT_EQ(id, deserialized->id());
  EXPECT_EQ(contents.size(), deserialized->contents().size());
  EXPECT_EQ(base::as_byte_span(contents), deserialized->contents());
}

TEST_P(SessionCommandParamTest, SerializeAndDeserializeEmpty) {
  const id_type id = 42;
  SessionCommand command(id, 0);

  std::vector<uint8_t> serialized = command.Serialize(encryptor());
  std::unique_ptr<SessionCommand> deserialized =
      SessionCommand::Deserialize(serialized, encryptor());
  ASSERT_TRUE(deserialized);
  EXPECT_EQ(id, deserialized->id());
  EXPECT_EQ(0u, deserialized->contents().size());
}

TEST(SessionCommandTest, SerializeTruncatesLargeContents) {
  const id_type id = 42;
  const size_type exceeding_size = std::numeric_limits<size_type>::max();
  SessionCommand command(id, exceeding_size);
  EXPECT_EQ(exceeding_size, command.contents().size());

  std::vector<uint8_t> serialized = command.Serialize(nullptr);

  // contents should be truncated to kMaxContentSize
  const size_t expected_size =
      sizeof(size_type) + sizeof(id_type) + SessionCommand::kMaxContentSize;
  EXPECT_EQ(expected_size, serialized.size());

  // Deserializing it should yield kMaxContentSize bytes of content.
  std::unique_ptr<SessionCommand> deserialized =
      SessionCommand::Deserialize(serialized, nullptr);
  ASSERT_TRUE(deserialized);
  EXPECT_EQ(SessionCommand::kMaxContentSize, deserialized->contents().size());
}

std::string TestParamNameGenerator(
    const testing::TestParamInfo<bool>& param_info) {
  return param_info.param ? "Encrypted" : "Cleartext";
}

INSTANTIATE_TEST_SUITE_P(All,
                         SessionCommandParamTest,
                         ::testing::Bool(),
                         TestParamNameGenerator);

TEST(SessionCommandTest, DeserializeCleartextErrors) {
  // Too small to contain size_type.
  EXPECT_FALSE(
      SessionCommand::Deserialize(base::span<const uint8_t>({1}), nullptr));

  // Size field indicates more data than available.
  // size field = 10 (little endian), but only 2 bytes available.
  const uint8_t bad_size_data[] = {10, 0, 0};
  EXPECT_FALSE(SessionCommand::Deserialize(bad_size_data, nullptr));

  // Size field is smaller than sizeof(id_type).
  const uint8_t bad_size_data2[] = {0, 0};
  EXPECT_FALSE(SessionCommand::Deserialize(bad_size_data2, nullptr));
}

TEST(SessionCommandTest, DeserializeEncryptedErrors) {
  os_crypt_async::TestEncryptor test_encryptor =
      os_crypt_async::GetTestEncryptorForTesting();
  os_crypt_async::Encryptor* encryptor = &test_encryptor;

  // Too small to contain encrypted_size_type.
  EXPECT_FALSE(SessionCommand::Deserialize(base::span<const uint8_t>({1, 2, 3}),
                                           encryptor));

  // Size field indicates more data than available.
  // size field = 10 (little endian), but only 2 bytes available.
  const uint8_t bad_size_data[] = {10, 0, 0, 0, 0, 0};
  EXPECT_FALSE(SessionCommand::Deserialize(bad_size_data, encryptor));

  // Not decryptable data (valid size, but invalid ciphertext).
  const uint8_t bad_crypto_data[] = {4, 0, 0, 0, 1, 2, 3, 4};
  EXPECT_FALSE(SessionCommand::Deserialize(bad_crypto_data, encryptor));
}

}  // namespace
}  // namespace sessions
