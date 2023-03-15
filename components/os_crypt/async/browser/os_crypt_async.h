// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ASYNC_BROWSER_OS_CRYPT_ASYNC_H_
#define COMPONENTS_OS_CRYPT_ASYNC_BROWSER_OS_CRYPT_ASYNC_H_

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "components/os_crypt/async/common/encryptor.h"

namespace os_crypt_async {

// This class is responsible for vending Encryptor instances.
class COMPONENT_EXPORT(OS_CRYPT_ASYNC) OSCryptAsync {
 public:
  using InitCallback = base::OnceCallback<void(Encryptor, bool result)>;

  // TODO(crbug.com/1373092): add configuration parameters here, and a
  // UIThreadRunner parameter.
  OSCryptAsync();
  ~OSCryptAsync();

  // Moveable, not copyable.
  OSCryptAsync(OSCryptAsync&& other);
  OSCryptAsync& operator=(OSCryptAsync&& other);
  OSCryptAsync(const OSCryptAsync&) = delete;
  OSCryptAsync& operator=(const OSCryptAsync&) = delete;

  // Obtain an Encryptor instance. Can be called multiple times, each one will
  // get a valid instance once the initialization has completed, on the
  // `callback`. Must be called on the same sequence that the OSCryptAsync
  // object was created on. Destruction of the `base::CallbackListSubscription`
  // will cause the callback not to run, see `base/callback_list.h`.
  //
  // TODO(crbug.com/1373092): This function is currently sync, but will be made
  // async in a future CL.
  [[nodiscard]] base::CallbackListSubscription GetInstance(
      InitCallback callback);

 private:
  std::unique_ptr<Encryptor> GUARDED_BY_CONTEXT(sequence_checker_)
      encryptor_instance_;

  bool is_initialized_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace os_crypt_async

#endif  // COMPONENTS_OS_CRYPT_ASYNC_BROWSER_OS_CRYPT_ASYNC_H_
