// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_TABLE_ENCRYPTOR_FACTORY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_TABLE_ENCRYPTOR_FACTORY_H_

#include <memory>

#include "base/sequence_checker.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace autofill {

class AutofillTableEncryptor;

// Factory for creating Autofill table encryptor.
// If |delegate_| is set, then |delegate_| is used to create encryptor,
// else default encrytor (SystemEncryptor) is returned.
class AutofillTableEncryptorFactory {
 public:
  // Embedders are recommended to use this delegate to inject
  // their encryptor into Autofill table.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual std::unique_ptr<AutofillTableEncryptor> Create() = 0;
  };

  static AutofillTableEncryptorFactory* GetInstance();

  AutofillTableEncryptorFactory(const AutofillTableEncryptorFactory&) = delete;
  AutofillTableEncryptorFactory& operator=(
      const AutofillTableEncryptorFactory&) = delete;

  std::unique_ptr<AutofillTableEncryptor> Create();

  void SetDelegate(std::unique_ptr<Delegate> delegate);

 private:
  AutofillTableEncryptorFactory();
  ~AutofillTableEncryptorFactory();

  std::unique_ptr<Delegate> delegate_;
  SEQUENCE_CHECKER(sequence_checker_);

  friend struct base::DefaultSingletonTraits<AutofillTableEncryptorFactory>;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_TABLE_ENCRYPTOR_FACTORY_H_
