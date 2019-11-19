// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_ENCRYPTOR_H_
#define COMPONENTS_SYNC_BASE_ENCRYPTOR_H_

#include <string>

namespace syncer {

class Encryptor {
 public:
  // All methods below should be thread-safe.
  virtual bool EncryptString(const std::string& plaintext,
                             std::string* ciphertext) const = 0;

  virtual bool DecryptString(const std::string& ciphertext,
                             std::string* plaintext) const = 0;

 protected:
  virtual ~Encryptor() {}
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_ENCRYPTOR_H_
