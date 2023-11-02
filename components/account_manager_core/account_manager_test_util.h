// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_TEST_UTIL_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_TEST_UTIL_H_

#include "components/account_manager_core/account.h"

// Helper functions for Account Manager tests.
namespace account_manager {

// Generates a Gaia ID from |raw_email| and creates an account.
Account CreateTestGaiaAccount(const std::string& raw_email);

}  // namespace account_manager

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_TEST_UTIL_H_
