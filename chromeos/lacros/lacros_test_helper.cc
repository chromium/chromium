// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/lacros/lacros_test_helper.h"

#include "base/check.h"

namespace chromeos {

ScopedDisableCrosapiForTesting::ScopedDisableCrosapiForTesting()
    : disable_crosapi_resetter_(&LacrosService::disable_crosapi_for_testing_,
                                true) {
  // Ensure that no instance exist, to prevent interference.
  CHECK(!LacrosService::Get());
}

// TODO(crbug.com/1196314): Ensure that no instance exist on destruction, too.
// Currently, browser_tests' shutdown is an exception.
ScopedDisableCrosapiForTesting::~ScopedDisableCrosapiForTesting() = default;

ScopedLacrosServiceTestHelper::ScopedLacrosServiceTestHelper() = default;

ScopedLacrosServiceTestHelper::~ScopedLacrosServiceTestHelper() = default;

}  // namespace chromeos
