// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LACROS_LACROS_TEST_HELPER_H_
#define CHROMEOS_LACROS_LACROS_TEST_HELPER_H_

#include "base/auto_reset.h"
#include "base/version.h"
#include "chromeos/lacros/lacros_service.h"

namespace chromeos {

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
  LacrosService lacros_service_;
};

// Returns "true" if the version of Ash is `required_version` or newer.
// This method was introduced in M-103. For older versions of Ash, its
// version is unknowable and assumed to be 0.0.0.0 for comparison purposes.
// Can perform a blocking async call inside.
bool IsAshVersionAtLeastForTesting(base::Version required_version);

}  // namespace chromeos

#endif  // CHROMEOS_LACROS_LACROS_TEST_HELPER_H_
