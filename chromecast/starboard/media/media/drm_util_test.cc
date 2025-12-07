// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/media/drm_util.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"
#include "chromecast/starboard/media/media/test_matchers.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/encryption_pattern.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {

using ::testing::IsNull;
using ::testing::Pointee;

constexpr auto kDefaultBufferData =
    std::to_array<uint8_t>({1, 2, 3, 4, 5, 6, 7});

// Creates a chromium buffer from the given decrypt config and data.
scoped_refptr<::media::DecoderBuffer> CreateChromiumBuffer(
    std::unique_ptr<::media::DecryptConfig> decrypt_config,
    base::span<const uint8_t> data = kDefaultBufferData) {
  scoped_refptr<::media::DecoderBuffer> buffer =
      ::media::DecoderBuffer::CopyFrom(data);
  CHECK(buffer);
  buffer->set_decrypt_config(std::move(decrypt_config));
  return buffer;
}

TEST(DrmUtilTest, UnencryptedBufferHasNullDrmSampleInfo) {
  scoped_refptr<::media::DecoderBuffer> chromium_buffer =
      CreateChromiumBuffer(/*decrypt_config=*/nullptr);
  CHECK(chromium_buffer);

  EXPECT_THAT(DrmInfoWrapper::Create(*chromium_buffer).GetDrmSampleInfo(),
              IsNull());

  // Check the version that uses CastDecoderBuffer.
  auto cast_buffer =
      base::MakeRefCounted<DecoderBufferAdapter>(chromium_buffer);
  CHECK(cast_buffer);
  EXPECT_THAT(DrmInfoWrapper::Create(*cast_buffer).GetDrmSampleInfo(),
              IsNull());
}

TEST(DrmUtilTest, CreatesCencDrmInfo) {
  constexpr std::string_view kId = "drm_id";
  constexpr std::string_view kIv = "0123456789abcdef";
  CHECK_EQ(kIv.size(),
           static_cast<size_t>(::media::DecryptConfig::kDecryptionKeySize));
  const ::media::SubsampleEntry subsample(2, 5);
  StarboardDrmSubSampleMapping sb_subsample;
  sb_subsample.clear_byte_count = subsample.clear_bytes;
  sb_subsample.encrypted_byte_count = subsample.cypher_bytes;

  std::unique_ptr<::media::DecryptConfig> decrypt_config =
      ::media::DecryptConfig::CreateCencConfig(std::string(kId),
                                               std::string(kIv), {subsample});
  CHECK(decrypt_config);

  scoped_refptr<::media::DecoderBuffer> chromium_buffer =
      CreateChromiumBuffer(std::move(decrypt_config));
  CHECK(chromium_buffer);

  DrmInfoWrapper wrapper = DrmInfoWrapper::Create(*chromium_buffer);

  StarboardDrmSampleInfo expected_drm_info;
  expected_drm_info.encryption_scheme =
      StarboardDrmEncryptionScheme::kStarboardDrmEncryptionSchemeAesCtr;
  expected_drm_info.encryption_pattern.crypt_byte_block = 0;
  expected_drm_info.encryption_pattern.skip_byte_block = 0;
  base::span<uint8_t>(expected_drm_info.initialization_vector)
      .copy_from_nonoverlapping(base::as_byte_span(kIv));
  expected_drm_info.initialization_vector_size = kIv.size();
  base::span<uint8_t>(expected_drm_info.identifier)
      .first<kId.size()>()
      .copy_from_nonoverlapping(base::as_byte_span(kId));
  expected_drm_info.identifier_size = kId.size();
  expected_drm_info.subsample_mapping = base::span_from_ref(sb_subsample);

  EXPECT_THAT(DrmInfoWrapper::Create(*chromium_buffer).GetDrmSampleInfo(),
              Pointee(MatchesDrmInfo(expected_drm_info)));

  // Check the version that uses CastDecoderBuffer.
  auto cast_buffer =
      base::MakeRefCounted<DecoderBufferAdapter>(chromium_buffer);
  CHECK(cast_buffer);
  EXPECT_THAT(DrmInfoWrapper::Create(*cast_buffer).GetDrmSampleInfo(),
              Pointee(MatchesDrmInfo(expected_drm_info)));
}

TEST(DrmUtilTest, CreatesCbcsDrmInfo) {
  constexpr std::string_view kId = "drm_id_2";
  constexpr std::string_view kIv = "abcdefghijklmnop";
  CHECK_EQ(kIv.size(),
           static_cast<size_t>(::media::DecryptConfig::kDecryptionKeySize));
  const ::media::EncryptionPattern encryption_pattern(10, 20);
  const ::media::SubsampleEntry subsample(1, 6);
  StarboardDrmSubSampleMapping sb_subsample;
  sb_subsample.clear_byte_count = subsample.clear_bytes;
  sb_subsample.encrypted_byte_count = subsample.cypher_bytes;

  std::unique_ptr<::media::DecryptConfig> decrypt_config =
      ::media::DecryptConfig::CreateCbcsConfig(
          std::string(kId), std::string(kIv), {subsample}, encryption_pattern);
  CHECK(decrypt_config);

  scoped_refptr<::media::DecoderBuffer> chromium_buffer =
      CreateChromiumBuffer(std::move(decrypt_config));
  CHECK(chromium_buffer);

  DrmInfoWrapper wrapper = DrmInfoWrapper::Create(*chromium_buffer);

  StarboardDrmSampleInfo expected_drm_info;
  expected_drm_info.encryption_scheme =
      StarboardDrmEncryptionScheme::kStarboardDrmEncryptionSchemeAesCbc;
  expected_drm_info.encryption_pattern.crypt_byte_block =
      encryption_pattern.crypt_byte_block();
  expected_drm_info.encryption_pattern.skip_byte_block =
      encryption_pattern.skip_byte_block();
  base::span<uint8_t>(expected_drm_info.initialization_vector)
      .copy_from_nonoverlapping(base::as_byte_span(kIv));
  expected_drm_info.initialization_vector_size = kIv.size();
  base::span<uint8_t>(expected_drm_info.identifier)
      .first<kId.size()>()
      .copy_from_nonoverlapping(base::as_byte_span(kId));
  expected_drm_info.identifier_size = kId.size();
  expected_drm_info.subsample_mapping = base::span_from_ref(sb_subsample);

  EXPECT_THAT(DrmInfoWrapper::Create(*chromium_buffer).GetDrmSampleInfo(),
              Pointee(MatchesDrmInfo(expected_drm_info)));

  // Check the version that uses CastDecoderBuffer.
  auto cast_buffer =
      base::MakeRefCounted<DecoderBufferAdapter>(chromium_buffer);
  CHECK(cast_buffer);
  EXPECT_THAT(DrmInfoWrapper::Create(*cast_buffer).GetDrmSampleInfo(),
              Pointee(MatchesDrmInfo(expected_drm_info)));
}

TEST(DrmUtilTest, HandlesEmptySubsampleMappings) {
  // Chromium buffers might not specify a subsample mapping. We should assume
  // that the entire buffer is encrypted, in that case.
  constexpr auto kBufferData = std::to_array<uint8_t>({7, 8, 9});
  constexpr std::string_view kId = "drm_id";
  constexpr std::string_view kIv = "0123456789abcdef";
  CHECK_EQ(kIv.size(),
           static_cast<size_t>(::media::DecryptConfig::kDecryptionKeySize));

  std::unique_ptr<::media::DecryptConfig> decrypt_config =
      ::media::DecryptConfig::CreateCencConfig(
          std::string(kId), std::string(kIv), /*subsamples=*/{});
  CHECK(decrypt_config);

  scoped_refptr<::media::DecoderBuffer> chromium_buffer =
      CreateChromiumBuffer(std::move(decrypt_config), /*data=*/kBufferData);
  CHECK(chromium_buffer);

  DrmInfoWrapper wrapper = DrmInfoWrapper::Create(*chromium_buffer);

  StarboardDrmSampleInfo expected_drm_info;
  expected_drm_info.encryption_scheme =
      StarboardDrmEncryptionScheme::kStarboardDrmEncryptionSchemeAesCtr;
  expected_drm_info.encryption_pattern.crypt_byte_block = 0;
  expected_drm_info.encryption_pattern.skip_byte_block = 0;
  base::span<uint8_t>(expected_drm_info.initialization_vector)
      .copy_from_nonoverlapping(base::as_byte_span(kIv));
  expected_drm_info.initialization_vector_size = kIv.size();
  base::span<uint8_t>(expected_drm_info.identifier)
      .first<kId.size()>()
      .copy_from_nonoverlapping(base::as_byte_span(kId));
  expected_drm_info.identifier_size = kId.size();

  // There should be a single subsample mapping specifying that the entire
  // buffer is encrypted.
  StarboardDrmSubSampleMapping sb_subsample;
  sb_subsample.clear_byte_count = 0;
  sb_subsample.encrypted_byte_count = kBufferData.size();
  expected_drm_info.subsample_mapping = base::span_from_ref(sb_subsample);

  EXPECT_THAT(DrmInfoWrapper::Create(*chromium_buffer).GetDrmSampleInfo(),
              Pointee(MatchesDrmInfo(expected_drm_info)));

  // Check the version that uses CastDecoderBuffer. The cast code that converts
  // from chromium structs -> cast structs should have performed the same logic
  // of creating a single subsample mapping.
  auto cast_buffer =
      base::MakeRefCounted<DecoderBufferAdapter>(chromium_buffer);
  CHECK(cast_buffer);
  EXPECT_THAT(DrmInfoWrapper::Create(*cast_buffer).GetDrmSampleInfo(),
              Pointee(MatchesDrmInfo(expected_drm_info)));
}

}  // namespace
}  // namespace media
}  // namespace chromecast
