// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/system_encryptor.h"

#include "components/os_crypt/os_crypt.h"

namespace autofill {

bool SystemEncryptor::EncryptString16(const std::u16string& plaintext,
                                      std::string* ciphertext) const {
  return ::OSCrypt::EncryptString16(plaintext, ciphertext);
}

bool SystemEncryptor::DecryptString16(const std::string& ciphertext,
                                      std::u16string* plaintext) const {
  return ::OSCrypt::DecryptString16(ciphertext, plaintext);
}

}  // namespace autofill
