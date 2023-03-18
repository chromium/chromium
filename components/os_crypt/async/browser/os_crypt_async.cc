// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/os_crypt_async.h"

#include "base/callback_list.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "components/os_crypt/async/common/encryptor.h"

namespace os_crypt_async {

OSCryptAsync::OSCryptAsync() = default;
OSCryptAsync::~OSCryptAsync() = default;

OSCryptAsync::OSCryptAsync(OSCryptAsync&& other) = default;
OSCryptAsync& OSCryptAsync::operator=(OSCryptAsync&& other) = default;

base::CallbackListSubscription OSCryptAsync::GetInstance(
    InitCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_initialized_) {
    encryptor_instance_ = base::WrapUnique<Encryptor>(new Encryptor());
    is_initialized_ = true;
  }

  std::move(callback).Run(encryptor_instance_->Clone(), true);
  return base::CallbackListSubscription();
}

}  // namespace os_crypt_async
