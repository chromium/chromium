// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_TEST_ACCOUNTS_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_TEST_ACCOUNTS_H_

#include <string>

namespace crosier {

// Randomly picks a gaia test account from the test accounts pool.
void GetGaiaTestAccount(std::string& out_email, std::string& out_password);

}  // namespace crosier

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_TEST_ACCOUNTS_H_
