// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/media/media/drm_util.h"

#include <utility>

#include "base/check.h"
#include "base/logging.h"

namespace chromecast {
namespace media {

// Returns the length of an array.
template <typename T, size_t n>
static constexpr size_t ArrayLength(const T (&)[n]) {
  return n;
}

// Rather than hard-coding values here, we simply read the length of the
// relevant arrays in StarboardDrmSampleInfo.
constexpr int kMaxIvLength = ArrayLength(
    static_cast<StarboardDrmSampleInfo*>(nullptr)->initialization_vector);
constexpr int kMaxIdLength =
    ArrayLength(static_cast<StarboardDrmSampleInfo*>(nullptr)->identifier);

DrmInfoWrapper::DrmInfoWrapper() = default;

DrmInfoWrapper::DrmInfoWrapper(
    StarboardDrmSampleInfo drm_sample_info,
    std::vector<StarboardDrmSubSampleMapping> mappings) {
  drm_sample_info_ = std::move(drm_sample_info);
  subsample_mappings_ = std::move(mappings);

  UpdateSubsampleInfo();
}

DrmInfoWrapper::DrmInfoWrapper(DrmInfoWrapper&& other) {
  drm_sample_info_ = std::move(other.drm_sample_info_);
  subsample_mappings_ = std::move(other.subsample_mappings_);

  UpdateSubsampleInfo();
  other.drm_sample_info_ = std::nullopt;
}

DrmInfoWrapper& DrmInfoWrapper::operator=(DrmInfoWrapper&& other) {
  drm_sample_info_ = std::move(other.drm_sample_info_);
  subsample_mappings_ = std::move(other.subsample_mappings_);

  UpdateSubsampleInfo();
  other.drm_sample_info_ = std::nullopt;
  return *this;
}

DrmInfoWrapper::~DrmInfoWrapper() = default;

StarboardDrmSampleInfo* DrmInfoWrapper::GetDrmSampleInfo() {
  return drm_sample_info_ ? &*drm_sample_info_ : nullptr;
}

void DrmInfoWrapper::UpdateSubsampleInfo() {
  if (!drm_sample_info_) {
    return;
  }

  drm_sample_info_->subsample_count = subsample_mappings_.size();
  drm_sample_info_->subsample_mapping =
      subsample_mappings_.empty() ? nullptr : subsample_mappings_.data();
}

DrmInfoWrapper GetDrmInfo(const CastDecoderBuffer& buffer) {
  const CastDecryptConfig* decrypt_config = buffer.decrypt_config();
  if (!decrypt_config) {
    return DrmInfoWrapper();
  }

  // Populate drm_sample_info.
  StarboardDrmSampleInfo drm_info;

  switch (decrypt_config->encryption_scheme()) {
    case EncryptionScheme::kUnencrypted:
      return DrmInfoWrapper();
    case EncryptionScheme::kAesCtr:
      drm_info.encryption_scheme = kStarboardDrmEncryptionSchemeAesCtr;
      break;
    case EncryptionScheme::kAesCbc:
      drm_info.encryption_scheme = kStarboardDrmEncryptionSchemeAesCbc;
      break;
  }

  drm_info.encryption_pattern.crypt_byte_block =
      decrypt_config->pattern().encrypt_blocks;
  drm_info.encryption_pattern.skip_byte_block =
      decrypt_config->pattern().skip_blocks;

  int iv_size = decrypt_config->iv().size();
  if (iv_size > kMaxIvLength) {
    LOG(ERROR)
        << "Encrypted buffer contained too many initialization vector values "
           "(max supported by Starboard is "
        << kMaxIvLength << "): " << iv_size;
    iv_size = kMaxIvLength;
  }
  for (int i = 0; i < iv_size; ++i) {
    drm_info.initialization_vector[i] =
        static_cast<uint8_t>(decrypt_config->iv().at(i));
  }
  drm_info.initialization_vector_size = iv_size;

  int id_size = decrypt_config->key_id().size();
  if (id_size > kMaxIdLength) {
    LOG(ERROR) << "Encrypted buffer contained too many key ID vector values "
                  "(max supported by Starboard is "
               << kMaxIdLength << "): " << id_size;
    id_size = kMaxIdLength;
  }
  for (int i = 0; i < id_size; ++i) {
    drm_info.identifier[i] =
        static_cast<uint8_t>(decrypt_config->key_id().at(i));
  }
  drm_info.identifier_size = id_size;

  // Populate subsample_mappings.
  std::vector<StarboardDrmSubSampleMapping> subsample_mappings;
  subsample_mappings.reserve(decrypt_config->subsamples().size());
  for (const SubsampleEntry& subsample : decrypt_config->subsamples()) {
    StarboardDrmSubSampleMapping mapping;
    mapping.clear_byte_count = subsample.clear_bytes;
    mapping.encrypted_byte_count = subsample.cypher_bytes;
    subsample_mappings.push_back(std::move(mapping));
  }

  return DrmInfoWrapper(std::move(drm_info), std::move(subsample_mappings));
}

}  // namespace media
}  // namespace chromecast
