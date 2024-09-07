// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_test_environment.h"

#include "components/plus_addresses/plus_address_prefs.h"

namespace plus_addresses::test {

PlusAddressTestEnvironment::PlusAddressTestEnvironment() {
  prefs::RegisterProfilePrefs(pref_service_.registry());
}

PlusAddressTestEnvironment::~PlusAddressTestEnvironment() = default;

}  // namespace plus_addresses::test
