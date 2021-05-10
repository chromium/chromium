// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LACROS_LACROS_TEST_HELPER_H_
#define CHROMEOS_LACROS_LACROS_TEST_HELPER_H_

#include "base/auto_reset.h"
#include "chromeos/lacros/lacros_service.h"

namespace chromeos {

// Disables crosapi while this instance is alive.
// This must be instantiate before LacrosService is instantiated.
// Used only for testing purposes.
class ScopedDisableCrosapiForTesting {
 public:
  ScopedDisableCrosapiForTesting();
  ScopedDisableCrosapiForTesting(const ScopedDisableCrosapiForTesting&) =
      delete;
  ScopedDisableCrosapiForTesting& operator=(
      const ScopedDisableCrosapiForTesting&) = delete;
  ~ScopedDisableCrosapiForTesting();

 private:
  base::AutoReset<bool> disable_crosapi_resetter_;
};

// Helper for tests to instantiate LacrosService. This should only be
// used for unit tests, not browser tests.
// Instantiated LacrosService is expected to be accessed via
// LacrosService::Get().
class ScopedLacrosServiceTestHelper {
 public:
  ScopedLacrosServiceTestHelper();
  ScopedLacrosServiceTestHelper(const ScopedLacrosServiceTestHelper&) = delete;
  ScopedLacrosServiceTestHelper& operator=(
      const ScopedLacrosServiceTestHelper&) = delete;
  ~ScopedLacrosServiceTestHelper();

 private:
  ScopedDisableCrosapiForTesting disable_crosapi_;
  LacrosService lacros_service_;
};

}  // namespace chromeos

#endif  // CHROMEOS_LACROS_LACROS_TEST_HELPER_H_
