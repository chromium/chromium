// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/cdm_factory_daemon/mojom/decrypt_config_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

using MojomDecryptStatus = chromeos::cdm::mojom::DecryptStatus;
using NativeDecryptStatus = media::Decryptor::Status;
using MojomEncryptionScheme = chromeos::cdm::mojom::EncryptionScheme;
using NativeEncryptionScheme = media::EncryptionScheme;

// static
MojomDecryptStatus EnumTraits<MojomDecryptStatus, NativeDecryptStatus>::ToMojom(
    NativeDecryptStatus input) {
  switch (input) {
    case NativeDecryptStatus::kSuccess:
      return MojomDecryptStatus::kSuccess;
    case NativeDecryptStatus::kNoKey:
      return MojomDecryptStatus::kNoKey;
    case NativeDecryptStatus::kNeedMoreData:
      return MojomDecryptStatus::kFailure;
    case NativeDecryptStatus::kError:
      return MojomDecryptStatus::kFailure;
  }
  NOTREACHED_IN_MIGRATION();
  return MojomDecryptStatus::kFailure;
}

// static
bool EnumTraits<MojomDecryptStatus, NativeDecryptStatus>::FromMojom(
    MojomDecryptStatus input,
    NativeDecryptStatus* out) {
  switch (input) {
    case MojomDecryptStatus::kSuccess:
      *out = NativeDecryptStatus::kSuccess;
      return true;
    case MojomDecryptStatus::kNoKey:
      *out = NativeDecryptStatus::kNoKey;
      return true;
    case MojomDecryptStatus::kFailure:
      *out = NativeDecryptStatus::kError;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
MojomEncryptionScheme
EnumTraits<MojomEncryptionScheme, NativeEncryptionScheme>::ToMojom(
    NativeEncryptionScheme input) {
  switch (input) {
    // We should never encounter the unencrypted value.
    case NativeEncryptionScheme::kUnencrypted:
      NOTREACHED_IN_MIGRATION();
      return MojomEncryptionScheme::kCenc;
    case NativeEncryptionScheme::kCenc:
      return MojomEncryptionScheme::kCenc;
    case NativeEncryptionScheme::kCbcs:
      return MojomEncryptionScheme::kCbcs;
  }
  NOTREACHED_IN_MIGRATION();
  return MojomEncryptionScheme::kCenc;
}

// static
bool EnumTraits<MojomEncryptionScheme, NativeEncryptionScheme>::FromMojom(
    MojomEncryptionScheme input,
    NativeEncryptionScheme* out) {
  switch (input) {
    case MojomEncryptionScheme::kCenc:
      *out = NativeEncryptionScheme::kCenc;
      return true;
    case MojomEncryptionScheme::kCbcs:
      *out = NativeEncryptionScheme::kCbcs;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
bool StructTraits<chromeos::cdm::mojom::EncryptionPatternDataView,
                  media::EncryptionPattern>::
    Read(chromeos::cdm::mojom::EncryptionPatternDataView input,
         media::EncryptionPattern* output) {
  *output = media::EncryptionPattern(input.crypt_byte_block(),
                                     input.skip_byte_block());
  return true;
}

// static
bool StructTraits<chromeos::cdm::mojom::SubsampleEntryDataView,
                  media::SubsampleEntry>::
    Read(chromeos::cdm::mojom::SubsampleEntryDataView input,
         media::SubsampleEntry* output) {
  *output = media::SubsampleEntry(input.clear_bytes(), input.cipher_bytes());
  return true;
}

// static
bool StructTraits<chromeos::cdm::mojom::DecryptConfigDataView,
                  std::unique_ptr<media::DecryptConfig>>::
    Read(chromeos::cdm::mojom::DecryptConfigDataView input,
         std::unique_ptr<media::DecryptConfig>* output) {
  media::EncryptionScheme encryption_scheme;
  if (!input.ReadEncryptionScheme(&encryption_scheme))
    return false;

  std::string key_id;
  if (!input.ReadKeyId(&key_id))
    return false;

  std::string iv;
  if (!input.ReadIv(&iv))
    return false;

  std::vector<media::SubsampleEntry> subsamples;
  if (!input.ReadSubsamples(&subsamples))
    return false;

  std::optional<media::EncryptionPattern> encryption_pattern;
  if (!input.ReadEncryptionPattern(&encryption_pattern))
    return false;

  *output = std::make_unique<media::DecryptConfig>(
      encryption_scheme, key_id, iv, subsamples, encryption_pattern);
  return true;
}

}  // namespace mojo