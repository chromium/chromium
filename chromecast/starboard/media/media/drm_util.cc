// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/media/drm_util.h"

#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/logging.h"

namespace chromecast {
namespace media {

// Rather than hard-coding values here, we simply read the length of the
// relevant arrays in StarboardDrmSampleInfo.
constexpr size_t kMaxIvLength =
    std::size(StarboardDrmSampleInfo{}.initialization_vector);
constexpr size_t kMaxIdLength = std::size(StarboardDrmSampleInfo{}.identifier);

DrmInfoWrapper::DrmInfoWrapper() = default;

DrmInfoWrapper::DrmInfoWrapper(
    std::unique_ptr<StarboardDrmSampleInfo> drm_sample_info,
    std::unique_ptr<std::vector<StarboardDrmSubSampleMapping>> mappings)
    : drm_sample_info_(std::move(drm_sample_info)),
      subsample_mappings_(std::move(mappings)) {}

DrmInfoWrapper::DrmInfoWrapper(DrmInfoWrapper&& other) = default;

DrmInfoWrapper& DrmInfoWrapper::operator=(DrmInfoWrapper&& other) = default;

DrmInfoWrapper::~DrmInfoWrapper() = default;

StarboardDrmSampleInfo* DrmInfoWrapper::GetDrmSampleInfo() {
  return drm_sample_info_.get();
}

DrmInfoWrapper DrmInfoWrapper::Create(const CastDecoderBuffer& buffer) {
  const CastDecryptConfig* decrypt_config = buffer.decrypt_config();
  if (!decrypt_config) {
    return DrmInfoWrapper();
  }

  auto drm_info = std::make_unique<StarboardDrmSampleInfo>();
  switch (decrypt_config->encryption_scheme()) {
    case EncryptionScheme::kUnencrypted:
      return DrmInfoWrapper();
    case EncryptionScheme::kAesCtr:
      drm_info->encryption_scheme = kStarboardDrmEncryptionSchemeAesCtr;
      break;
    case EncryptionScheme::kAesCbc:
      drm_info->encryption_scheme = kStarboardDrmEncryptionSchemeAesCbc;
      break;
  }

  drm_info->encryption_pattern.crypt_byte_block =
      decrypt_config->pattern().encrypt_blocks;
  drm_info->encryption_pattern.skip_byte_block =
      decrypt_config->pattern().skip_blocks;

  size_t iv_size = decrypt_config->iv().size();
  if (iv_size > kMaxIvLength) {
    LOG(ERROR)
        << "Encrypted buffer contained too many initialization vector values "
           "(max supported by Starboard is "
        << kMaxIvLength << "): " << iv_size;
    iv_size = kMaxIvLength;
  }

  // Populate drm_info->initialization_vector.
  base::span<uint8_t>(drm_info->initialization_vector)
      .first(iv_size)
      .copy_from_nonoverlapping(
          base::as_byte_span(decrypt_config->iv()).first(iv_size));
  drm_info->initialization_vector_size = iv_size;

  size_t id_size = decrypt_config->key_id().size();
  if (id_size > kMaxIdLength) {
    LOG(ERROR) << "Encrypted buffer contained too many key ID vector values "
                  "(max supported by Starboard is "
               << kMaxIdLength << "): " << id_size;
    id_size = kMaxIdLength;
  }

  // Populate drm_info->identifier.
  base::span<uint8_t>(drm_info->identifier)
      .first(id_size)
      .copy_from_nonoverlapping(
          base::as_byte_span(decrypt_config->key_id()).first(id_size));
  drm_info->identifier_size = id_size;

  // Populate subsample_mappings.
  auto subsample_mappings =
      std::make_unique<std::vector<StarboardDrmSubSampleMapping>>();
  subsample_mappings->reserve(decrypt_config->subsamples().size());
  for (const SubsampleEntry& subsample : decrypt_config->subsamples()) {
    StarboardDrmSubSampleMapping mapping;
    mapping.clear_byte_count = subsample.clear_bytes;
    mapping.encrypted_byte_count = subsample.cypher_bytes;
    subsample_mappings->push_back(std::move(mapping));
  }

  if (subsample_mappings->empty()) {
    LOG(ERROR) << "At least one subsample must be present for DRM info. DRM "
                  "playback will likely not work";
    return DrmInfoWrapper();
  }

  drm_info->subsample_mapping =
      base::span<const StarboardDrmSubSampleMapping>(*subsample_mappings);

  return DrmInfoWrapper(std::move(drm_info), std::move(subsample_mappings));
}

DrmInfoWrapper DrmInfoWrapper::Create(const ::media::DecoderBuffer& buffer) {
  if (buffer.decrypt_config() == nullptr) {
    return DrmInfoWrapper();
  }

  // Populate drm_sample_info.
  auto drm_info = std::make_unique<StarboardDrmSampleInfo>();

  const ::media::DecryptConfig& decrypt_config = *buffer.decrypt_config();
  switch (decrypt_config.encryption_scheme()) {
    case ::media::EncryptionScheme::kUnencrypted:
      return DrmInfoWrapper();
    case ::media::EncryptionScheme::kCenc:
      drm_info->encryption_scheme =
          StarboardDrmEncryptionScheme::kStarboardDrmEncryptionSchemeAesCtr;
      break;
    case ::media::EncryptionScheme::kCbcs:
      drm_info->encryption_scheme =
          StarboardDrmEncryptionScheme::kStarboardDrmEncryptionSchemeAesCbc;
      break;
    default:
      LOG(ERROR) << "Unsupported DRM encryption scheme: "
                 << decrypt_config.encryption_scheme();
      return DrmInfoWrapper();
  }

  // Populate drm_sample_info.
  if (decrypt_config.HasPattern()) {
    drm_info->encryption_pattern.crypt_byte_block =
        decrypt_config.encryption_pattern()->crypt_byte_block();
    drm_info->encryption_pattern.skip_byte_block =
        decrypt_config.encryption_pattern()->skip_byte_block();
  }

  size_t iv_size = decrypt_config.iv().size();
  if (iv_size > kMaxIvLength) {
    LOG(ERROR)
        << "Encrypted buffer contained too many initialization vector values "
           "(max supported by Starboard is "
        << kMaxIvLength << "): " << iv_size;
    iv_size = kMaxIvLength;
  }

  // Populate drm_info->initialization_vector.
  base::span<uint8_t>(drm_info->initialization_vector)
      .first(iv_size)
      .copy_from_nonoverlapping(
          base::as_byte_span(decrypt_config.iv()).first(iv_size));
  drm_info->initialization_vector_size = iv_size;

  size_t id_size = decrypt_config.key_id().size();
  if (id_size > kMaxIdLength) {
    LOG(ERROR) << "Encrypted buffer contained too many key ID vector values "
                  "(max supported by Starboard is "
               << kMaxIdLength << "): " << id_size;
    id_size = kMaxIdLength;
  }

  // Populate drm_info->identifier.
  base::span<uint8_t>(drm_info->identifier)
      .first(id_size)
      .copy_from_nonoverlapping(
          base::as_byte_span(decrypt_config.key_id()).first(id_size));
  drm_info->identifier_size = id_size;

  // Populate subsample_mappings.
  auto subsample_mappings =
      std::make_unique<std::vector<StarboardDrmSubSampleMapping>>();
  subsample_mappings->reserve(decrypt_config.subsamples().size());
  for (const ::media::SubsampleEntry& subsample : decrypt_config.subsamples()) {
    StarboardDrmSubSampleMapping mapping;
    mapping.clear_byte_count = subsample.clear_bytes;
    mapping.encrypted_byte_count = subsample.cypher_bytes;
    subsample_mappings->push_back(std::move(mapping));
  }

  if (subsample_mappings->empty()) {
    // DecryptConfig may contain 0 subsamples if all content is encrypted. Map
    // this case to a single fully-encrypted "subsample", since Starboard
    // requires at least one subsample.
    subsample_mappings->push_back(
        {.clear_byte_count = 0,
         .encrypted_byte_count = static_cast<int32_t>(buffer.size())});
  }
  drm_info->subsample_mapping =
      base::span<const StarboardDrmSubSampleMapping>(*subsample_mappings);

  return DrmInfoWrapper(std::move(drm_info), std::move(subsample_mappings));
}

}  // namespace media
}  // namespace chromecast
