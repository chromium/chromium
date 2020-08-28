// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/identity_mutator.h"

#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/device_accounts_synchronizer.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"

#if defined(OS_ANDROID)
#include "base/android/jni_string.h"
#include "components/signin/public/android/jni_headers/IdentityMutator_jni.h"
#include "components/signin/public/identity_manager/account_info.h"
#endif

namespace signin {

#if defined(OS_ANDROID)
JniIdentityMutator::JniIdentityMutator(IdentityMutator* identity_mutator)
    : identity_mutator_(identity_mutator) {}

bool JniIdentityMutator::SetPrimaryAccount(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& primary_account_id,
    jint j_consent_level) {
  PrimaryAccountMutator* primary_account_mutator =
      identity_mutator_->GetPrimaryAccountMutator();
  DCHECK(primary_account_mutator);
  // TODO(https://crbug.com/1046746): Refactor PrimaryAccountMutator API and
  //                                  pass ConsentLevel directly there.
  switch (static_cast<ConsentLevel>(j_consent_level)) {
    case ConsentLevel::kSync:
      return primary_account_mutator->SetPrimaryAccount(
          ConvertFromJavaCoreAccountId(env, primary_account_id));
    case ConsentLevel::kNotRequired:
      primary_account_mutator->SetUnconsentedPrimaryAccount(
          ConvertFromJavaCoreAccountId(env, primary_account_id));
      return true;
    default:
      NOTREACHED() << "Unknown consent level: " << j_consent_level;
      return false;
  }
}

bool JniIdentityMutator::ClearPrimaryAccount(JNIEnv* env,
                                             jint action,
                                             jint source_metric,
                                             jint delete_metric) {
  PrimaryAccountMutator* primary_account_mutator =
      identity_mutator_->GetPrimaryAccountMutator();
  DCHECK(primary_account_mutator);
  return primary_account_mutator->ClearPrimaryAccount(
      PrimaryAccountMutator::ClearAccountsAction::kDefault,
      static_cast<signin_metrics::ProfileSignout>(source_metric),
      static_cast<signin_metrics::SignoutDelete>(delete_metric));
}

void JniIdentityMutator::ReloadAllAccountsFromSystemWithPrimaryAccount(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_primary_account_id) {
  DeviceAccountsSynchronizer* device_accounts_synchronizer =
      identity_mutator_->GetDeviceAccountsSynchronizer();
  DCHECK(device_accounts_synchronizer);
  base::Optional<CoreAccountId> primary_account_id;
  if (j_primary_account_id) {
    primary_account_id =
        ConvertFromJavaCoreAccountId(env, j_primary_account_id);
  }
  device_accounts_synchronizer->ReloadAllAccountsFromSystemWithPrimaryAccount(
      primary_account_id);
}
#endif  // defined(OS_ANDROID)

IdentityMutator::IdentityMutator(
    std::unique_ptr<PrimaryAccountMutator> primary_account_mutator,
    std::unique_ptr<AccountsMutator> accounts_mutator,
    std::unique_ptr<AccountsCookieMutator> accounts_cookie_mutator,
    std::unique_ptr<DeviceAccountsSynchronizer> device_accounts_synchronizer)
    : primary_account_mutator_(std::move(primary_account_mutator)),
      accounts_mutator_(std::move(accounts_mutator)),
      accounts_cookie_mutator_(std::move(accounts_cookie_mutator)),
      device_accounts_synchronizer_(std::move(device_accounts_synchronizer)) {
  DCHECK(accounts_cookie_mutator_);
  DCHECK(!accounts_mutator_ || !device_accounts_synchronizer_)
      << "Cannot have both an AccountsMutator and a DeviceAccountsSynchronizer";

#if defined(OS_ANDROID)
  jni_identity_mutator_.reset(new JniIdentityMutator(this));
  java_identity_mutator_ = Java_IdentityMutator_Constructor(
      base::android::AttachCurrentThread(),
      reinterpret_cast<intptr_t>(jni_identity_mutator_.get()));
#endif
}

IdentityMutator::~IdentityMutator() {
#if defined(OS_ANDROID)
  if (java_identity_mutator_)
    Java_IdentityMutator_destroy(base::android::AttachCurrentThread(),
                                 java_identity_mutator_);
#endif
}

#if defined(OS_ANDROID)
base::android::ScopedJavaLocalRef<jobject> IdentityMutator::GetJavaObject() {
  DCHECK(java_identity_mutator_);
  return base::android::ScopedJavaLocalRef<jobject>(java_identity_mutator_);
}
#endif

PrimaryAccountMutator* IdentityMutator::GetPrimaryAccountMutator() {
  return primary_account_mutator_.get();
}

AccountsMutator* IdentityMutator::GetAccountsMutator() {
  return accounts_mutator_.get();
}

AccountsCookieMutator* IdentityMutator::GetAccountsCookieMutator() {
  return accounts_cookie_mutator_.get();
}

DeviceAccountsSynchronizer* IdentityMutator::GetDeviceAccountsSynchronizer() {
  return device_accounts_synchronizer_.get();
}
}  // namespace signin
