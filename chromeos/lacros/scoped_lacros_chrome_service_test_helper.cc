// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/lacros/scoped_lacros_chrome_service_test_helper.h"

#include <utility>

#include "chromeos/lacros/lacros_chrome_service_delegate.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"

namespace chromeos {

ScopedLacrosChromeServiceTestHelper::ScopedLacrosChromeServiceTestHelper() {
  // Ensure that no instance exist, to prevent interference.
  CHECK(!chromeos::LacrosChromeServiceImpl::Get());
  LacrosChromeServiceImpl::DisableCrosapiForTests();
  lacros_chrome_service_ = std::make_unique<LacrosChromeServiceImpl>(
      /*delegate=*/nullptr);
}

ScopedLacrosChromeServiceTestHelper::~ScopedLacrosChromeServiceTestHelper() =
    default;

}  // namespace chromeos
