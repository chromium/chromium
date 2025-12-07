// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/system_encryptor.h"

namespace gcm {

SystemEncryptor::SystemEncryptor(os_crypt_async::Encryptor encryptor)
    : encryptor_(std::move(encryptor)) {}

SystemEncryptor::~SystemEncryptor() = default;

bool SystemEncryptor::EncryptString(const std::string& plaintext,
                                    std::string* ciphertext) {
  return encryptor_.EncryptString(plaintext, ciphertext);
}

bool SystemEncryptor::DecryptString(const std::string& ciphertext,
                                    std::string* plaintext) {
  return encryptor_.DecryptString(ciphertext, plaintext);
}

}  // namespace gcm
