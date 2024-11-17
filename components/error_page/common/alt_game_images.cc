// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/error_page/common/alt_game_images.h"

#include <memory>
#include <string>

#include "base/base64url.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "components/error_page/common/alt_game_image_data.h"
#include "components/error_page/common/error_page_switches.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"

namespace error_page {
namespace {

std::string DecryptImage(const std::string& image_ciphertext,
                         crypto::SymmetricKey* key) {
  constexpr int kBlockSize = 16;
  crypto::Encryptor encryptor;
  // *Never* use this pattern for encrypting anything that matters! There are
  // many problems with this, but it's just obfuscation so it doesn't matter.
  if (!encryptor.Init(key, crypto::Encryptor::Mode::CBC,
                      /*iv=*/key->key().substr(0, kBlockSize))) {
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

BASE_FEATURE(kNetErrorAltGameMode,
             "NetErrorAltGameMode",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kNetErrorAltGameModeKey{
    &kNetErrorAltGameMode, "Key", ""};

bool EnableAltGameMode() {
  return base::FeatureList::IsEnabled(kNetErrorAltGameMode) &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             error_page::switches::kEnableDinosaurEasterEggAltGameImages);
}

std::string GetAltGameImage(int image_id, int scale) {
  std::string encrypted_image;
  if (!::error_page::LookupObfuscatedImage(image_id, scale, &encrypted_image)) {
    DLOG(ERROR) << "Couldn't find alt game image with ID=" << image_id
                << " and scale=" << scale << "x.";
    return std::string();
  }

  std::unique_ptr<crypto::SymmetricKey> key = GetKey();
  if (!key) {
    DLOG(ERROR) << "Failed to load alt game image encryption key.";
    return std::string();
  }

  std::string image = DecryptImage(encrypted_image, key.get());
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

int ChooseAltGame() {
  // Image 0 should be "common".
  return base::RandInt(1, CountAlternateImages() - 1);
}

}  // namespace error_page
