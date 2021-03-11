// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/error_page/common/alt_game_images.h"

#include "base/base64.h"
#include "base/base64url.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"

namespace error_page {
namespace {

// TODO(iwells): Put the image data here and update kAltGameImagesCount.
const char* kAltGameImages1x[] = {};
const char* kAltGameImages2x[] = {};
constexpr int kAltGameImagesCount = 0;

std::string DecryptImage(const char* b64_data, crypto::SymmetricKey* key) {
  std::string image_ciphertext;
  if (!base::Base64Decode(b64_data, &image_ciphertext)) {
    DLOG(ERROR) << "Failed to base64-decode encrypted image.";
    return std::string();
  }

  crypto::Encryptor encryptor;
  // *Never* use this pattern for encrypting anything that matters! There are
  // many problems with this, but it's just obfuscation so it doesn't matter.
  if (!encryptor.Init(key, crypto::Encryptor::Mode::CBC,
                      /*iv=*/key->key().substr(0, 16))) {
    DLOG(ERROR) << "Failed to initialize encryptor.";
    return std::string();
  }

  std::string image_plaintext;
  if (!encryptor.Decrypt(image_ciphertext, &image_plaintext)) {
    DLOG(ERROR) << "Failed to decrypt image.";
    return std::string();
  }
  return image_plaintext;
}

const char* LookupImage(int id, int scale) {
  if (id < 0 || id >= kAltGameImagesCount)
    return nullptr;

  if (scale == 1)
    return kAltGameImages1x[id];
  else if (scale == 2)
    return kAltGameImages2x[id];
  return nullptr;
}

std::unique_ptr<crypto::SymmetricKey> GetKey() {
  std::string b64_key = kNetErrorAltGameModeKey.Get();
  if (b64_key.empty()) {
    DLOG(ERROR) << "No image encryption key present.";
    return nullptr;
  }

  // Note: key is base64url-encoded because the default base64 format includes
  // the '/' character which interferes with setting the param from the command
  // line for testing.
  std::string raw_key;
  if (!base::Base64UrlDecode(
          b64_key, base::Base64UrlDecodePolicy::IGNORE_PADDING, &raw_key)) {
    DLOG(ERROR) << "Failed to base64-decode image encryption key.";
    return nullptr;
  }

  std::unique_ptr<crypto::SymmetricKey> key = crypto::SymmetricKey::Import(
      crypto::SymmetricKey::Algorithm::AES, raw_key);
  if (!key) {
    DLOG(ERROR) << "Invalid image encryption key: wrong length?";
    return nullptr;
  }
  return key;
}

}  // namespace

const base::Feature kNetErrorAltGameMode{"NetErrorAltGameMode",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<std::string> kNetErrorAltGameModeKey{
    &kNetErrorAltGameMode, "Key", ""};

bool EnableAltGameMode() {
  return base::FeatureList::IsEnabled(kNetErrorAltGameMode);
}

std::string GetAltGameImage(int image_id, int scale) {
  const char* encrypted_image_b64 = LookupImage(image_id, scale);
  if (!encrypted_image_b64) {
    DLOG(ERROR) << "Couldn't find alt game image with ID=" << image_id
                << " and scale=" << scale << "x.";
    return std::string();
  }

  std::unique_ptr<crypto::SymmetricKey> key = GetKey();
  if (!key) {
    DLOG(ERROR) << "Failed to load alt game image encryption key.";
    return std::string();
  }

  std::string image = DecryptImage(encrypted_image_b64, key.get());
  if (image.empty()) {
    DLOG(ERROR) << "Failed to decrypt alt game image " << image_id << " at "
                << scale << "x scale.";
    return std::string();
  }

  if (!base::StartsWith(image, "data:image/png;base64,")) {
    DLOG(ERROR) << "Decrypted data doesn't look like an image data URL.";
    return std::string();
  }

  return image;
}

}  // namespace error_page
