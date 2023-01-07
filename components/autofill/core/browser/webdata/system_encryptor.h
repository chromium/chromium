// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_SYSTEM_ENCRYPTOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_SYSTEM_ENCRYPTOR_H_

#include "components/autofill/core/browser/webdata/autofill_table_encryptor.h"

namespace autofill {
// Default encryptor used in Autofill table.
class SystemEncryptor : public AutofillTableEncryptor {
 public:
  SystemEncryptor() = default;

  SystemEncryptor(const SystemEncryptor&) = delete;
  SystemEncryptor& operator=(const SystemEncryptor&) = delete;

  ~SystemEncryptor() override = default;

  bool EncryptString16(const std::u16string& plaintext,
                       std::string* ciphertext) const override;

  bool DecryptString16(const std::string& ciphertext,
                       std::u16string* plaintext) const override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_SYSTEM_ENCRYPTOR_H_
