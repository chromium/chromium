// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/media/drm_util.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gtest_util.h"
#include "chromecast/media/cma/base/cast_decrypt_config_impl.h"
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
  const auto encryption_pattern = ::media::EncryptionPattern::Create(10, 5);
  CHECK(encryption_pattern.has_value());
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
      encryption_pattern->crypt_byte_block();
  expected_drm_info.encryption_pattern.skip_byte_block =
      encryption_pattern->skip_byte_block();
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

TEST(DrmUtilTest, EmptySubsamplesLargeBufferCrashes) {
  constexpr size_t kLargeSize = size_t{0x80000000};  // 2 GiB
  static_assert(kLargeSize > static_cast<size_t>(INT32_MAX));

  constexpr std::string_view kId = "drm_id";
  constexpr std::string_view kIv = "0123456789abcdef";
  CHECK_EQ(kIv.size(),
           static_cast<size_t>(::media::DecryptConfig::kDecryptionKeySize));

  std::unique_ptr<::media::DecryptConfig> decrypt_config =
      ::media::DecryptConfig::CreateCencConfig(std::string(kId),
                                               std::string(kIv),
                                               /*subsamples=*/{});
  CHECK(decrypt_config);

  class DummyExternalMemory : public ::media::DecoderBuffer::ExternalMemory {
   public:
    explicit DummyExternalMemory(size_t size) : size_(size) {}
    ~DummyExternalMemory() override = default;
    const base::span<const uint8_t> Span() const override {
      return UNSAFE_BUFFERS(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(0x1000), size_));
    }

   private:
    size_t size_;
  };

  auto chromium_buffer = ::media::DecoderBuffer::FromExternalMemory(
      std::make_unique<DummyExternalMemory>(kLargeSize));
  CHECK_EQ(chromium_buffer->size(), kLargeSize);
  chromium_buffer->set_decrypt_config(std::move(decrypt_config));

  EXPECT_CHECK_DEATH(DrmInfoWrapper::Create(*chromium_buffer));
}

class FakeCastDecoderBuffer : public CastDecoderBuffer {
 public:
  FakeCastDecoderBuffer(base::span<const uint8_t> data,
                        std::unique_ptr<CastDecryptConfig> decrypt_config)
      : data_(data), decrypt_config_(std::move(decrypt_config)) {}
  ~FakeCastDecoderBuffer() override = default;

  StreamId stream_id() const override { return kPrimary; }
  int64_t timestamp() const override { return 0; }
  const uint8_t* data() const override { return data_.data(); }
  size_t data_size() const override { return data_.size(); }
  const CastDecryptConfig* decrypt_config() const override {
    return decrypt_config_.get();
  }
  DecryptContext* decrypt_context() const override { return nullptr; }
  bool end_of_stream() const override { return false; }
  bool is_key_frame() const override { return true; }

 private:
  base::span<const uint8_t> data_;
  std::unique_ptr<CastDecryptConfig> decrypt_config_;
};

TEST(DrmUtilTest, VerifySubsamplesMatchSizeDetectsInvalidSizes) {
  constexpr auto kBufferData = std::to_array<uint8_t>({1, 2, 3, 4, 5});
  constexpr std::string_view kId = "drm_id";
  constexpr std::string_view kIv = "0123456789abcdef";

  // Total size (3+3=6) > buffer size (5).
  FakeCastDecoderBuffer buffer(
      kBufferData, std::make_unique<CastDecryptConfigImpl>(
                       std::string(kId), std::string(kIv), EncryptionPattern(),
                       std::vector<SubsampleEntry>{SubsampleEntry(3, 3)},
                       EncryptionScheme::kAesCtr));

  EXPECT_FALSE(DrmInfoWrapper::VerifySubsamplesMatchSize(buffer));

  // Total size (1+1=2) < buffer size (5).
  FakeCastDecoderBuffer buffer_2(
      kBufferData, std::make_unique<CastDecryptConfigImpl>(
                       std::string(kId), std::string(kIv), EncryptionPattern(),
                       std::vector<SubsampleEntry>{SubsampleEntry(1, 1)},
                       EncryptionScheme::kAesCtr));

  EXPECT_FALSE(DrmInfoWrapper::VerifySubsamplesMatchSize(buffer_2));

  // Correct size (2+3=5) == buffer size (5).
  const ::media::SubsampleEntry valid_subsample(2, 3);
  std::unique_ptr<::media::DecryptConfig> decrypt_config_3 =
      ::media::DecryptConfig::CreateCencConfig(
          std::string(kId), std::string(kIv), {valid_subsample});
  auto buffer_3 = base::MakeRefCounted<DecoderBufferAdapter>(
      CreateChromiumBuffer(std::move(decrypt_config_3), kBufferData));

  EXPECT_TRUE(DrmInfoWrapper::VerifySubsamplesMatchSize(*buffer_3));
}

}  // namespace
}  // namespace media
}  // namespace chromecast
