// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FCM_ENGINE_SYSTEM_ENCRYPTOR_H_
#define COMPONENTS_FCM_ENGINE_SYSTEM_ENCRYPTOR_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "google_apis/gcm/base/encryptor.h"

namespace fcm {

// Encryptor that uses os_crypt_async::Encryptor to implement gcm::Encryptor.
class SystemEncryptor : public gcm::Encryptor {
 public:
  explicit SystemEncryptor(scoped_refptr<os_crypt_async::Encryptor> encryptor);

  ~SystemEncryptor() override;

  bool EncryptString(const std::string& plaintext,
                     std::string* ciphertext) override;

  bool DecryptString(const std::string& ciphertext,
                     std::string* plaintext) override;

 private:
  scoped_refptr<os_crypt_async::Encryptor> encryptor_;
};

}  // namespace fcm

#endif  // COMPONENTS_FCM_ENGINE_SYSTEM_ENCRYPTOR_H_
