// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/system_encryptor.h"

#include "components/os_crypt/sync/os_crypt.h"

namespace gcm {

SystemEncryptor::~SystemEncryptor() = default;

bool SystemEncryptor::EncryptString(const std::string& plaintext,
                                    std::string* ciphertext) {
  return ::OSCrypt::EncryptString(plaintext, ciphertext);
}

bool SystemEncryptor::DecryptString(const std::string& ciphertext,
                                    std::string* plaintext) {
  return ::OSCrypt::DecryptString(ciphertext, plaintext);
}

}  // namespace gcm
