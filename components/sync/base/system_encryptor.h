// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_SYSTEM_ENCRYPTOR_H_
#define COMPONENTS_SYNC_BASE_SYSTEM_ENCRYPTOR_H_

#include <string>

#include "base/compiler_specific.h"
#include "components/sync/base/encryptor.h"

namespace syncer {

// Encryptor that uses the Chrome password manager's encryptor.
class SystemEncryptor : public Encryptor {
 public:
  ~SystemEncryptor() override;

  bool EncryptString(const std::string& plaintext,
                     std::string* ciphertext) const override;

  bool DecryptString(const std::string& ciphertext,
                     std::string* plaintext) const override;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_SYSTEM_ENCRYPTOR_H_
