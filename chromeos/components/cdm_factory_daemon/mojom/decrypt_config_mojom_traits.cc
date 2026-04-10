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
  NOTREACHED();
}

// static
std::optional<NativeDecryptStatus>
EnumTraits<MojomDecryptStatus, NativeDecryptStatus>::FromMojom(
    MojomDecryptStatus input) {
  switch (input) {
    case MojomDecryptStatus::kSuccess:
      return NativeDecryptStatus::kSuccess;
    case MojomDecryptStatus::kNoKey:
      return NativeDecryptStatus::kNoKey;
    case MojomDecryptStatus::kFailure:
      return NativeDecryptStatus::kError;
  }
  NOTREACHED();
}

// static
MojomEncryptionScheme
EnumTraits<MojomEncryptionScheme, NativeEncryptionScheme>::ToMojom(
    NativeEncryptionScheme input) {
  switch (input) {
    // We should never encounter the unencrypted value.
    case NativeEncryptionScheme::kUnencrypted:
      NOTREACHED();
    case NativeEncryptionScheme::kCenc:
      return MojomEncryptionScheme::kCenc;
    case NativeEncryptionScheme::kCbcs:
      return MojomEncryptionScheme::kCbcs;
  }
  NOTREACHED();
}

// static
std::optional<NativeEncryptionScheme>
EnumTraits<MojomEncryptionScheme, NativeEncryptionScheme>::FromMojom(
    MojomEncryptionScheme input) {
  switch (input) {
    case MojomEncryptionScheme::kCenc:
      return NativeEncryptionScheme::kCenc;
    case MojomEncryptionScheme::kCbcs:
      return NativeEncryptionScheme::kCbcs;
  }
  NOTREACHED();
}

// static
bool StructTraits<chromeos::cdm::mojom::EncryptionPatternDataView,
                  media::EncryptionPattern>::
    Read(chromeos::cdm::mojom::EncryptionPatternDataView input,
         media::EncryptionPattern* output) {
  auto pattern = media::EncryptionPattern::Create(input.crypt_byte_block(),
                                                  input.skip_byte_block());
  if (!pattern) {
    return false;
  }
  *output = *pattern;
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
