// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_TEST_HELPER_H_
#define COMPONENTS_USER_MANAGER_TEST_HELPER_H_

#include <string_view>

#include "base/memory/raw_ref.h"
#include "components/user_manager/user_type.h"

class AccountId;
class PrefService;

namespace user_manager {

class User;
class UserManager;

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

  // Similar to RegisterPersistedUser, records `account_id` as a persisted
  // child user.
  static void RegisterPersistedChildUser(PrefService& local_state,
                                         const AccountId& account_id);

  // Records the `user_id` as a Kiosk app user to the given `local_state`.
  static void RegisterKioskAppUser(PrefService& local_state,
                                   std::string_view user_id);

  // Records the `user_id` as a Web-Kiosk app user to the given `local_state`.
  static void RegisterWebKioskAppUser(PrefService& local_state,
                                      std::string_view user_id);

  // Records the `user_id` as a Public Account user to the given `local_state`.
  static void RegisterPublicAccountUser(PrefService& local_state,
                                        std::string_view user_id);

  // Returns the fake username hash for testing.
  // Valid AccountId must be used, otherwise CHECKed.
  static std::string GetFakeUsernameHash(const AccountId& account_id);

  // `user_manager` must outlive the instance of the TestHelper.
  explicit TestHelper(UserManager* user_manager);
  ~TestHelper();

  // Creates and adds a regular (persisted) user, and returns it.
  // On failure, returns nullptr.
  [[nodiscard]] User* AddRegularUser(const AccountId& account_id);

  // Creates and adds a child user, and returns it.
  [[nodiscard]] User* AddChildUser(const AccountId& account_id);

  // Creates and adds a guest user, and returns it.
  // On failure, returns nullptr.
  [[nodiscard]] User* AddGuestUser();

  // Creates and adds a new PublicAccount user, and returns it.
  // On failure, returns nullptr.
  [[nodiscard]] User* AddPublicAccountUser(std::string_view user_id);

  // Creates and adds a new Kiosk user, and returns it.
  // On failure, returns nullptr.
  [[nodiscard]] User* AddKioskAppUser(std::string_view user_id);

  // Creates and adds a new web Kiosk user, and returns it.
  // On failure, returns nullptr.
  [[nodiscard]] User* AddWebKioskAppUser(std::string_view user_id);

 private:
  User* AddUserInternal(const AccountId& account_id, UserType user_type);
  User* AddDeviceLocalAccountUserInternal(std::string_view user_id,
                                          UserType user_type);

  raw_ref<UserManager> user_manager_;
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_TEST_HELPER_H_
