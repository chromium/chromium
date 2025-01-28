// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_TEST_HELPER_H_
#define COMPONENTS_USER_MANAGER_TEST_HELPER_H_

class AccountId;
class PrefService;

namespace user_manager {

// Utilities to set up UserManager related environment.
class TestHelper {
 public:
  // Records the `account_id` as a persisted user to the given `local_state`.
  // `local_state` must be properly set up, specifically it needs UserManager
  // related registration.
  // In most cases, this registration needs to be done before UserManager
  // is created. Specifically, for browser_tests, SetUpLocalStatePrefService()
  // is a recommended function to call this.
  static void RegisterPersistedUser(PrefService& local_state,
                                    const AccountId& account_id);

 private:
  // Currently, this has only static methods.
  TestHelper() = delete;
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_TEST_HELPER_H_
