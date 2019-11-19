// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/system_encryptor.h"

#include "components/os_crypt/os_crypt.h"

namespace syncer {

SystemEncryptor::~SystemEncryptor() {}

bool SystemEncryptor::EncryptString(const std::string& plaintext,
                                    std::string* ciphertext) const {
  return ::OSCrypt::EncryptString(plaintext, ciphertext);
}

bool SystemEncryptor::DecryptString(const std::string& ciphertext,
                                    std::string* plaintext) const {
  return ::OSCrypt::DecryptString(ciphertext, plaintext);
}

}  // namespace syncer
