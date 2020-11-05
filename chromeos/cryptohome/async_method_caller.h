// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CRYPTOHOME_ASYNC_METHOD_CALLER_H_
#define CHROMEOS_CRYPTOHOME_ASYNC_METHOD_CALLER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "chromeos/dbus/constants/attestation_constants.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace cryptohome {

// Note: This file is placed in ::cryptohome instead of ::chromeos::cryptohome
// since there is already a namespace ::cryptohome which holds the error code
// enum (MountError) and referencing ::chromeos::cryptohome and ::cryptohome
// within the same code is confusing.

// This class manages calls to Cryptohome service's 'async' methods.
class COMPONENT_EXPORT(CHROMEOS_CRYPTOHOME) AsyncMethodCaller {
 public:
  // A callback type which is called back on the UI thread when the results of
  // method calls are ready.
  using Callback =
      base::OnceCallback<void(bool success, MountError return_code)>;
  using DataCallback =
      base::OnceCallback<void(bool success, const std::string& data)>;

  virtual ~AsyncMethodCaller() {}

  // Creates the global AsyncMethodCaller instance.
  static void Initialize();

  // Similar to Initialize(), but can inject an alternative
  // AsyncMethodCaller such as MockAsyncMethodCaller for testing.
  // The injected object will be owned by the internal pointer and deleted
  // by Shutdown().
  static void InitializeForTesting(AsyncMethodCaller* async_method_caller);

  // Destroys the global AsyncMethodCaller instance if it exists.
  static void Shutdown();

  // Returns a pointer to the global AsyncMethodCaller instance.
  // Initialize() should already have been called.
  static AsyncMethodCaller* GetInstance();
};

}  // namespace cryptohome

#endif  // CHROMEOS_CRYPTOHOME_ASYNC_METHOD_CALLER_H_
