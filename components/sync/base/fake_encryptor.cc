// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/fake_encryptor.h"

#include "base/base64.h"

namespace syncer {

FakeEncryptor::~FakeEncryptor() {}

bool FakeEncryptor::EncryptString(const std::string& plaintext,
                                  std::string* ciphertext) const {
  base::Base64Encode(plaintext, ciphertext);
  return true;
}

bool FakeEncryptor::DecryptString(const std::string& ciphertext,
                                  std::string* plaintext) const {
  return base::Base64Decode(ciphertext, plaintext);
}

}  // namespace syncer
