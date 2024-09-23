// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_ENCLAVE_MANAGER_INTERFACE_H_
#define CHROME_BROWSER_WEBAUTHN_ENCLAVE_MANAGER_INTERFACE_H_

#include "base/functional/callback_forward.h"
#include "components/keyed_service/core/keyed_service.h"

class EnclaveManager;

class EnclaveManagerInterface : public KeyedService {
 public:
  // Many actions report results using a `Callback`. The boolean argument
  // is true if the operation is successful and false otherwise.
  // These callbacks never hairpin. (I.e. are never called before the function
  // that they were passed to returns.)
  using Callback = base::OnceCallback<void(bool)>;

  EnclaveManagerInterface() = default;
  EnclaveManagerInterface(const EnclaveManagerInterface&) = delete;
  EnclaveManagerInterface& operator=(const EnclaveManagerInterface&) = delete;
  ~EnclaveManagerInterface() override = default;

  // Return the full `EnclaveManager` interface. This will crash the address
  // space if run on an `EnclaveManagerInterface` instance that is not backed
  // by a real `EnclaveManager`, i.e. when it's a mock.
  virtual EnclaveManager* GetEnclaveManager();

  // Returns true if the current user has been registered with the enclave.
  virtual bool is_registered() const = 0;

  // Send a request to the enclave to delete the registration for the current
  // user, erase local keys, and erase local state for the user. Safe to call in
  // any state and is a no-op if no registration exists.
  virtual void Unenroll(Callback callback) = 0;
};

#endif  // CHROME_BROWSER_WEBAUTHN_ENCLAVE_MANAGER_INTERFACE_H_
