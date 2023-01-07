// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_table_encryptor_factory.h"

#include <memory>

#include "base/memory/singleton.h"
#include "components/autofill/core/browser/webdata/system_encryptor.h"

namespace autofill {

AutofillTableEncryptorFactory::AutofillTableEncryptorFactory() = default;

AutofillTableEncryptorFactory::~AutofillTableEncryptorFactory() = default;

AutofillTableEncryptorFactory* AutofillTableEncryptorFactory::GetInstance() {
  return base::Singleton<AutofillTableEncryptorFactory>::get();
}

std::unique_ptr<AutofillTableEncryptor>
AutofillTableEncryptorFactory::Create() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_ ? delegate_->Create() : std::make_unique<SystemEncryptor>();
}

void AutofillTableEncryptorFactory::SetDelegate(
    std::unique_ptr<Delegate> delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_ = std::move(delegate);
}

}  // namespace autofill
