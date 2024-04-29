// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BROWSER_CONTEXT_HELPER_BROWSER_CONTEXT_HELPER_H_
#define CHROMEOS_ASH_COMPONENTS_BROWSER_CONTEXT_HELPER_BROWSER_CONTEXT_HELPER_H_

#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/files/file_path.h"

class AccountId;

namespace content {
class BrowserContext;
}  // namespace content

namespace user_manager {
class User;
}  // namespace user_manager

namespace ash {

class ProfileHelperImpl;

// This helper class is used to keep tracking the relationship between User
// and BrowserContext (a.k.a. Profile).
class COMPONENT_EXPORT(ASH_BROWSER_CONTEXT_HELPER) BrowserContextHelper {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns a BrowserContext object corresponding to the given path if fully
    // initialized. Otherwise returns nullptr. If the system is not
    // initialized, also returns nullptr (for unittests).
    virtual content::BrowserContext* GetBrowserContextByPath(
        const base::FilePath& path) = 0;

    // Returns a BrowserCotnext object that the specified `account_id` is
    // annotated. Returns nullptr if not found.
    virtual content::BrowserContext* GetBrowserContextByAccountId(
        const AccountId& account_id) = 0;

    // DEPRECATED. Please do not use this in the new code, and instead use
    // GetProfileByPath().
    // Similar to GetBrowserContextByPath, but synchronously create a
    // BrowserContext instance if it is not initialized.
    // If the system is not initialized, still returns nullptr (for unittests).
    // TODO(crbug.com/40225390): Remove this later.
    virtual content::BrowserContext* DeprecatedGetBrowserContext(
        const base::FilePath& path) = 0;

    // Returns the primary off-the-record BrowserContext instance corresponding
    // to the given `browser_context`. If there is not, creates the one.
    virtual content::BrowserContext* GetOrCreatePrimaryOTRBrowserContext(
        content::BrowserContext* browser_context) = 0;

    // Returns the original BrowserContext instance. If the given
    // `browser_context` is not an off-the-record browser context, itself will
    // be returned.
    virtual content::BrowserContext* GetOriginalBrowserContext(
        content::BrowserContext* browser_context) = 0;

    // Returns the path to the user data directory.
    // If the system is not initialized, returns nullptr (for unittests).
    virtual const base::FilePath* GetUserDataDir() = 0;
  };

  // Legacy profile dir that was used when only one cryptohome has been mounted.
  static const char kLegacyBrowserContextDirName[];

  // This must be kept in sync with TestingProfile::kTestUserProfileDir.
  static const char kTestUserBrowserContextDirName[];

  explicit BrowserContextHelper(std::unique_ptr<Delegate> delegate);
  BrowserContextHelper(const BrowserContextHelper&) = delete;
  BrowserContextHelper& operator=(const BrowserContextHelper&) = delete;
  ~BrowserContextHelper();

  // BrowserContextHelper is effectively a singleton in the system.
  // This returns the pointer if already initialized.
  static BrowserContextHelper* Get();

  // Returns user id hash for |browser_context|, or empty string if the hash
  // could not be extracted from the |browser_context|.
  static std::string GetUserIdHashFromBrowserContext(
      content::BrowserContext* browser_context);

  // Returns BrowserContext instance of the user associated with |account_id|
  // if it is created and fully initialized. Otherwise, returns nullptr.
  content::BrowserContext* GetBrowserContextByAccountId(
      const AccountId& account_id);

  // Returns BrowserContext instance of the |user| if it is created and fully
  // initialized. Otherwise, returns nullptr.
  content::BrowserContext* GetBrowserContextByUser(
      const user_manager::User* user);

  // Returns User instance of the given |browser_context|. If not found,
  // returns nullptr.
  const user_manager::User* GetUserByBrowserContext(
      content::BrowserContext* browser_context);

  // Returns user browser context dir in a format of "u-${user_id_hash}".
  static std::string GetUserBrowserContextDirName(
      std::string_view user_id_hash);

  // Returns browser context path that corresponds to the given |user_id_hash|.
  base::FilePath GetBrowserContextPathByUserIdHash(
      std::string_view user_id_hash);

  // Returns the path of signin browser context.
  base::FilePath GetSigninBrowserContextPath() const;

  // Returns signin browser context instance. If not yet created, returns
  // nullptr. Note that returned instance is off-the-record one.
  content::BrowserContext* GetSigninBrowserContext();

  // DEPRECATED. Please use GetSinginBrowserContext() instead.
  // Similar to GetSigninBrowserContext, but if not yet created,
  // this loads the BrowserContext instance, instead of returning nullptr.
  content::BrowserContext* DeprecatedGetOrCreateSigninBrowserContext();

  // Returns the path of lock-screen-app browser context.
  base::FilePath GetLockScreenAppBrowserContextPath() const;

  // Returns the path of lock-screen browser context.
  base::FilePath GetLockScreenBrowserContextPath() const;

  // Returns lock-screen browser context instance. If not yet created,
  // returns nullptr. Note that returned instance is off-the-record one.
  content::BrowserContext* GetLockScreenBrowserContext();

  // Returns the path of shimless-rma-app browser context.
  base::FilePath GetShimlessRmaAppBrowserContextPath() const;

  // TODO(b/40225390): forcibly enables mapping by annotated AccountId.
  // This is a workaround for the transition period. Remove once it's
  // completed.
  void SetUseAnnotatedAccountIdForTesting() {
    use_annotated_account_id_for_testing_ = true;
  }

 private:
  // This is only for graceful migration.
  // TODO(crbug.com/40225390): Remove this when migration is done.
  friend class ash::ProfileHelperImpl;
  Delegate* delegate() { return delegate_.get(); }

  bool UseAnnotatedAccountId();

  std::unique_ptr<Delegate> delegate_;
  bool use_annotated_account_id_for_testing_ = false;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_BROWSER_CONTEXT_HELPER_BROWSER_CONTEXT_HELPER_H_
