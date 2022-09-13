// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_SYSTEM_ENCRYPTOR_H_
#define COMPONENTS_GCM_DRIVER_SYSTEM_ENCRYPTOR_H_

#include "base/compiler_specific.h"
#include "google_apis/gcm/base/encryptor.h"

namespace gcm {

// Encryptor that uses the Chrome password manager's encryptor.
class SystemEncryptor : public Encryptor {
 public:
  ~SystemEncryptor() override;

  bool EncryptString(const std::string& plaintext,
                     std::string* ciphertext) override;

  bool DecryptString(const std::string& ciphertext,
                     std::string* plaintext) override;
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_SYSTEM_ENCRYPTOR_H_
