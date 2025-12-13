// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_MOCK_ENCLAVE_MANAGER_H_
#define CHROME_BROWSER_WEBAUTHN_MOCK_ENCLAVE_MANAGER_H_

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chrome/browser/webauthn/enclave_manager_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockEnclaveManager : public EnclaveManagerInterface {
 public:
  MockEnclaveManager();
  ~MockEnclaveManager() override;
  MockEnclaveManager(const MockEnclaveManager&) = delete;
  MockEnclaveManager& operator=(const MockEnclaveManager&) = delete;

  MOCK_METHOD(void, Unenroll, (EnclaveManagerInterface::Callback), (override));
  MOCK_METHOD(bool, is_registered, (), (const override));
  MOCK_METHOD(bool, is_loaded, (), (const override));
  MOCK_METHOD(bool, is_ready, (), (const override));
  MOCK_METHOD(void,
              CheckGpmPinAvailability,
              (EnclaveManagerInterface::GpmPinAvailabilityCallback),
              (override));
  MOCK_METHOD(void,
              LoadAfterDelay,
              (base::TimeDelta, base::OnceClosure),
              (override));
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
};

#endif  // CHROME_BROWSER_WEBAUTHN_MOCK_ENCLAVE_MANAGER_H_
