// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/decoder_buffer_adapter.h"

#include "base/memory/scoped_refptr.h"
#include "chromecast/public/media/cast_decrypt_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {

using ::testing::NotNull;

constexpr uint8_t kBufferData[] = "hello";
constexpr size_t kBufferDataSize = std::size(kBufferData);
constexpr int64_t kBufferTimestampUs = 31;
// This has to be DecryptConfig::kDecryptionKeySize=16 bytes.
constexpr char kIv[] = "0123456789ABCDEF";

scoped_refptr<::media::DecoderBuffer> MakeDecoderBuffer() {
  scoped_refptr<::media::DecoderBuffer> buffer =
      ::media::DecoderBuffer::CopyFrom(kBufferData);
  buffer->set_timestamp(base::Microseconds(kBufferTimestampUs));
  return buffer;
}

TEST(DecoderBufferAdapterTest, Default) {
  scoped_refptr<::media::DecoderBuffer> buffer = MakeDecoderBuffer();
  scoped_refptr<DecoderBufferAdapter> buffer_adapter(
      new DecoderBufferAdapter(buffer));

  EXPECT_EQ(kPrimary, buffer_adapter->stream_id());
  EXPECT_EQ(kBufferTimestampUs, buffer_adapter->timestamp());
  EXPECT_EQ(0, memcmp(buffer_adapter->data(), kBufferData, kBufferDataSize));
  EXPECT_EQ(kBufferDataSize, buffer_adapter->data_size());
  EXPECT_EQ(nullptr, buffer_adapter->decrypt_config());
  EXPECT_FALSE(buffer_adapter->end_of_stream());
}

TEST(DecoderBufferAdapterTest, Secondary) {
  scoped_refptr<DecoderBufferAdapter> buffer_adapter(
      new DecoderBufferAdapter(kSecondary, MakeDecoderBuffer()));
  EXPECT_EQ(kSecondary, buffer_adapter->stream_id());
}

TEST(DecoderBufferAdapterTest, Timestamp) {
  scoped_refptr<DecoderBufferAdapter> buffer_adapter(
      new DecoderBufferAdapter(MakeDecoderBuffer()));
  EXPECT_EQ(kBufferTimestampUs, buffer_adapter->timestamp());

  const int64_t kTestTimestampUs = 62;
  buffer_adapter->set_timestamp(base::Microseconds(kTestTimestampUs));
  EXPECT_EQ(kTestTimestampUs, buffer_adapter->timestamp());
}

TEST(DecoderBufferAdapterTest, Data) {
  scoped_refptr<DecoderBufferAdapter> buffer_adapter(
      new DecoderBufferAdapter(MakeDecoderBuffer()));
  EXPECT_EQ(0, memcmp(buffer_adapter->data(), kBufferData, kBufferDataSize));
  EXPECT_EQ(kBufferDataSize, buffer_adapter->data_size());

  const uint8_t kTestBufferData[] = "world";
  const size_t kTestBufferDataSize = std::size(kTestBufferData);
  memcpy(buffer_adapter->writable_data(), kTestBufferData, kTestBufferDataSize);
  EXPECT_EQ(
      0, memcmp(buffer_adapter->data(), kTestBufferData, kTestBufferDataSize));
  EXPECT_EQ(kTestBufferDataSize, buffer_adapter->data_size());
}

TEST(DecoderBufferAdapterTest, DecryptConfig) {
  const std::string kKeyId("foo-key");
  const std::string kIV("0123456789abcdef");

  // No DecryptConfig.
  {
    scoped_refptr<::media::DecoderBuffer> buffer = MakeDecoderBuffer();
    EXPECT_EQ(nullptr, buffer->decrypt_config());
    scoped_refptr<DecoderBufferAdapter> buffer_adapter(
        new DecoderBufferAdapter(buffer));
    // DecoderBufferAdapter ignores the decrypt config.
    EXPECT_EQ(nullptr, buffer_adapter->decrypt_config());
  }

  // Empty subsamples.
  {
    std::unique_ptr<::media::DecryptConfig> decrypt_config =
        ::media::DecryptConfig::CreateCencConfig(kKeyId, kIV, {});
    EXPECT_TRUE(decrypt_config);

    scoped_refptr<::media::DecoderBuffer> buffer = MakeDecoderBuffer();
    buffer->set_decrypt_config(std::move(decrypt_config));
    scoped_refptr<DecoderBufferAdapter> buffer_adapter(
        new DecoderBufferAdapter(buffer));
    const CastDecryptConfig* cast_decrypt_config =
        buffer_adapter->decrypt_config();
    EXPECT_NE(nullptr, cast_decrypt_config);
    EXPECT_EQ(kKeyId, cast_decrypt_config->key_id());
    EXPECT_EQ(kIV, cast_decrypt_config->iv());
    // DecoderBufferAdapter creates a single fully-encrypted subsample.
    EXPECT_EQ(1u, cast_decrypt_config->subsamples().size());
    EXPECT_EQ(0u, cast_decrypt_config->subsamples()[0].clear_bytes);
    EXPECT_EQ(kBufferDataSize,
              cast_decrypt_config->subsamples()[0].cypher_bytes);
  }

  // Regular DecryptConfig with non-empty subsamples.
  {
    uint32_t kClearBytes[] = {10, 15};
    uint32_t kCypherBytes[] = {5, 7};
    std::vector<::media::SubsampleEntry> subsamples;
    subsamples.emplace_back(kClearBytes[0], kCypherBytes[0]);
    subsamples.emplace_back(kClearBytes[1], kCypherBytes[1]);

    std::unique_ptr<::media::DecryptConfig> decrypt_config =
        ::media::DecryptConfig::CreateCencConfig(kKeyId, kIV, subsamples);
    EXPECT_TRUE(decrypt_config);

    scoped_refptr<::media::DecoderBuffer> buffer = MakeDecoderBuffer();
    buffer->set_decrypt_config(std::move(decrypt_config));
    scoped_refptr<DecoderBufferAdapter> buffer_adapter(
        new DecoderBufferAdapter(buffer));
    const CastDecryptConfig* cast_decrypt_config =
        buffer_adapter->decrypt_config();
    EXPECT_NE(nullptr, cast_decrypt_config);
    EXPECT_EQ(kKeyId, cast_decrypt_config->key_id());
    EXPECT_EQ(kIV, cast_decrypt_config->iv());
    // DecoderBufferAdapter copies all subsamples.
    EXPECT_EQ(2u, cast_decrypt_config->subsamples().size());
    EXPECT_EQ(kClearBytes[0], cast_decrypt_config->subsamples()[0].clear_bytes);
    EXPECT_EQ(kCypherBytes[0],
              cast_decrypt_config->subsamples()[0].cypher_bytes);
    EXPECT_EQ(kClearBytes[1], cast_decrypt_config->subsamples()[1].clear_bytes);
    EXPECT_EQ(kCypherBytes[1],
              cast_decrypt_config->subsamples()[1].cypher_bytes);
  }
}

TEST(DecoderBufferAdapterTest, EndOfStream) {
  scoped_refptr<DecoderBufferAdapter> buffer_adapter(
      new DecoderBufferAdapter(::media::DecoderBuffer::CreateEOSBuffer()));
  EXPECT_TRUE(buffer_adapter->end_of_stream());
  EXPECT_EQ(nullptr, buffer_adapter->decrypt_config());
}

TEST(DecoderBufferAdapterTest, SetsEncryptionSchemeOfCencDecryptConfig) {
  scoped_refptr<::media::DecoderBuffer> buffer = MakeDecoderBuffer();
  std::unique_ptr<::media::DecryptConfig> decrypt_config =
      ::media::DecryptConfig::CreateCencConfig("key_id", kIv,
                                               /*subsamples=*/{});
  buffer->set_decrypt_config(std::move(decrypt_config));

  auto buffer_adapter = base::MakeRefCounted<DecoderBufferAdapter>(buffer);
  ASSERT_THAT(buffer_adapter->decrypt_config(), NotNull());
  EXPECT_EQ(buffer_adapter->decrypt_config()->encryption_scheme(),
            EncryptionScheme::kAesCtr);
}

TEST(DecoderBufferAdapterTest, SetsEncryptionSchemeOfCbcsDecryptConfig) {
  scoped_refptr<::media::DecoderBuffer> buffer = MakeDecoderBuffer();
  std::unique_ptr<::media::DecryptConfig> decrypt_config =
      ::media::DecryptConfig::CreateCbcsConfig(
          "key_id", kIv,
          /*subsamples=*/{},
          /*encryption_pattern=*/std::nullopt);
  buffer->set_decrypt_config(std::move(decrypt_config));

  auto buffer_adapter = base::MakeRefCounted<DecoderBufferAdapter>(buffer);
  ASSERT_THAT(buffer_adapter->decrypt_config(), NotNull());
  EXPECT_EQ(buffer_adapter->decrypt_config()->encryption_scheme(),
            EncryptionScheme::kAesCbc);
}

TEST(DecoderBufferAdapterTest, HandlesIsKeyFrame) {
  scoped_refptr<::media::DecoderBuffer> buffer = MakeDecoderBuffer();
  auto buffer_adapter = base::MakeRefCounted<DecoderBufferAdapter>(buffer);

  buffer->set_is_key_frame(true);
  EXPECT_TRUE(buffer_adapter->is_key_frame());

  buffer->set_is_key_frame(false);
  EXPECT_FALSE(buffer_adapter->is_key_frame());
}

}  // namespace
}  // namespace media
}  // namespace chromecast
