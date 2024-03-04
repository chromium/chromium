// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_MUTATOR_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_MUTATOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif

namespace signin {
class AccountsMutator;
class AccountsCookieMutator;
class PrimaryAccountMutator;
class DeviceAccountsSynchronizer;

#if BUILDFLAG(IS_ANDROID)
class IdentityMutator;

// This class is the JNI interface accessing IdentityMutator.
// This is created by IdentityMutator and can only be accessed by JNI generated
// code (IdentityMutator_jni.h), i.e. by IdentityMutator.java.
class JniIdentityMutator {
 public:
  // JniIdentityMutator is non-copyable, non-movable
  JniIdentityMutator(IdentityMutator&& other) = delete;
  JniIdentityMutator const& operator=(IdentityMutator&& other) = delete;

  JniIdentityMutator(const IdentityMutator& other) = delete;
  JniIdentityMutator const& operator=(const IdentityMutator& other) = delete;

  // Called by java to mark the account with |account_id| as the primary
  // account, and returns PrimaryAccountMutator::PrimaryAccountError. To
  // succeed, this requires that:
  //   - the account is known by the IdentityManager.
  //   - setting the primary account is allowed,
  //   - the account username is allowed by policy,
  //   - there is not already a primary account set.
  jint SetPrimaryAccount(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& primary_account_id,
      jint consent_level,
      jint access_point,
      const base::android::JavaParamRef<jobject>& j_prefs_committed_callback);

  // Called by java to clear the primary account, and return whether the
  // operation succeeded or not. Depending on |action|, the other accounts known
  // to the IdentityManager may be deleted.
  bool ClearPrimaryAccount(JNIEnv* env, jint source_metric);

  // Called by java to revoke sync consent for the primary account.
  void RevokeSyncConsent(JNIEnv* env, jint source_metric);

  // Called by java to reload the accounts in the token service from the system
  // accounts.
  void ReloadAllAccountsFromSystemWithPrimaryAccount(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& primary_account_id);

  void SeedAccountsThenReloadAllAccountsWithPrimaryAccount(
      JNIEnv* env,
      const base::android::JavaParamRef<jobjectArray>& j_core_account_infos,
      const base::android::JavaParamRef<jobject>& j_primary_account_id);

 private:
  friend IdentityMutator;

  JniIdentityMutator(IdentityMutator* identity_mutator);

  raw_ptr<IdentityMutator> identity_mutator_;
};
#endif

// IdentityMutator is the mutating interface for IdentityManager.
class IdentityMutator {
 public:
  IdentityMutator(
      std::unique_ptr<PrimaryAccountMutator> primary_account_mutator,
      std::unique_ptr<AccountsMutator> accounts_mutator,
      std::unique_ptr<AccountsCookieMutator> accounts_cookie_mutator,
      std::unique_ptr<DeviceAccountsSynchronizer> device_accounts_synchronizer);

  virtual ~IdentityMutator();

  // IdentityMutator is non-copyable, non-moveable.
  IdentityMutator(IdentityMutator&& other) = delete;
  IdentityMutator const& operator=(IdentityMutator&& other) = delete;

  IdentityMutator(const IdentityMutator& other) = delete;
  IdentityMutator const& operator=(const IdentityMutator& other) = delete;

#if BUILDFLAG(IS_ANDROID)
  // Get the reference on the java IdentityManager.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();
#endif

  // Returns pointer to the object used to change the signed-in state of the
  // primary account, if supported on the current platform. Otherwise, returns
  // null.
  PrimaryAccountMutator* GetPrimaryAccountMutator();

  // Returns pointer to the object used to seed accounts and mutate state of
  // accounts' refresh tokens, if supported on the current platform. Otherwise,
  // returns null.
  AccountsMutator* GetAccountsMutator();

  // Returns pointer to the object used to manipulate the cookies stored and the
  // accounts associated with them. Guaranteed to be non-null.
  AccountsCookieMutator* GetAccountsCookieMutator();

  // Returns pointer to the object used to seed accounts information from the
  // device-level accounts. May be null if the system has no such notion.
  DeviceAccountsSynchronizer* GetDeviceAccountsSynchronizer();

 private:
#if BUILDFLAG(IS_ANDROID)
  // C++ endpoint for identity mutator calls originating from java.
  std::unique_ptr<JniIdentityMutator> jni_identity_mutator_;

  // Java-side IdentityMutator object.
  base::android::ScopedJavaGlobalRef<jobject> java_identity_mutator_;
#endif

  // PrimaryAccountMutator instance. May be null if mutation of the primary
  // account state is not supported on the current platform.
  std::unique_ptr<PrimaryAccountMutator> primary_account_mutator_;

  // AccountsMutator instance. May be null if mutation of accounts is not
  // supported on the current platform.
  std::unique_ptr<AccountsMutator> accounts_mutator_;

  // AccountsCookieMutator instance. Guaranteed to be non-null, as this
  // functionality is supported on all platforms.
  std::unique_ptr<AccountsCookieMutator> accounts_cookie_mutator_;

  // DeviceAccountsSynchronizer instance.
  std::unique_ptr<DeviceAccountsSynchronizer> device_accounts_synchronizer_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_MUTATOR_H_
