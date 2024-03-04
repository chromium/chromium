// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/identity_mutator.h"

#include "build/build_config.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/device_accounts_synchronizer.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/signin/public/android/jni_headers/IdentityMutator_jni.h"
#include "components/signin/public/identity_manager/account_info.h"
#endif

namespace signin {

#if BUILDFLAG(IS_ANDROID)
JniIdentityMutator::JniIdentityMutator(IdentityMutator* identity_mutator)
    : identity_mutator_(identity_mutator) {}

jint JniIdentityMutator::SetPrimaryAccount(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& primary_account_id,
    jint j_consent_level,
    jint j_access_point,
    const base::android::JavaParamRef<jobject>& j_prefs_committed_callback) {
  PrimaryAccountMutator* primary_account_mutator =
      identity_mutator_->GetPrimaryAccountMutator();
  DCHECK(primary_account_mutator);

  PrimaryAccountMutator::PrimaryAccountError error =
      primary_account_mutator->SetPrimaryAccount(
          ConvertFromJavaCoreAccountId(env, primary_account_id),
          static_cast<ConsentLevel>(j_consent_level),
          static_cast<signin_metrics::AccessPoint>(j_access_point),
          base::BindOnce(base::android::RunRunnableAndroid,
                         base::android::ScopedJavaGlobalRef<jobject>(
                             j_prefs_committed_callback)));
  return static_cast<jint>(error);
}

bool JniIdentityMutator::ClearPrimaryAccount(JNIEnv* env, jint source_metric) {
  PrimaryAccountMutator* primary_account_mutator =
      identity_mutator_->GetPrimaryAccountMutator();
  DCHECK(primary_account_mutator);
  return primary_account_mutator->ClearPrimaryAccount(
      static_cast<signin_metrics::ProfileSignout>(source_metric));
}

void JniIdentityMutator::RevokeSyncConsent(JNIEnv* env, jint source_metric) {
  PrimaryAccountMutator* primary_account_mutator =
      identity_mutator_->GetPrimaryAccountMutator();
  DCHECK(primary_account_mutator);
  return primary_account_mutator->RevokeSyncConsent(
      static_cast<signin_metrics::ProfileSignout>(source_metric));
}

void JniIdentityMutator::ReloadAllAccountsFromSystemWithPrimaryAccount(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_primary_account_id) {
  DeviceAccountsSynchronizer* device_accounts_synchronizer =
      identity_mutator_->GetDeviceAccountsSynchronizer();
  DCHECK(device_accounts_synchronizer);
  std::optional<CoreAccountId> primary_account_id;
  if (j_primary_account_id) {
    primary_account_id =
        ConvertFromJavaCoreAccountId(env, j_primary_account_id);
  }
  device_accounts_synchronizer->ReloadAllAccountsFromSystemWithPrimaryAccount(
      primary_account_id);
}

void JniIdentityMutator::SeedAccountsThenReloadAllAccountsWithPrimaryAccount(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& j_core_account_infos,
    const base::android::JavaParamRef<jobject>& j_primary_account_id) {
  std::vector<CoreAccountInfo> core_account_infos;
  for (size_t i = 0;
       i < base::android::SafeGetArrayLength(env, j_core_account_infos); i++) {
    base::android::ScopedJavaLocalRef<jobject> core_account_info_java(
        env, env->GetObjectArrayElement(j_core_account_infos.obj(), i));
    core_account_infos.push_back(
        ConvertFromJavaCoreAccountInfo(env, core_account_info_java));
  }

  std::optional<CoreAccountId> primary_account_id;
  if (j_primary_account_id) {
    primary_account_id =
        ConvertFromJavaCoreAccountId(env, j_primary_account_id);
  } else {
    primary_account_id = std::nullopt;
  }

  DeviceAccountsSynchronizer* device_accounts_synchronizer =
      identity_mutator_->GetDeviceAccountsSynchronizer();
  CHECK(device_accounts_synchronizer);
  device_accounts_synchronizer
      ->SeedAccountsThenReloadAllAccountsWithPrimaryAccount(core_account_infos,
                                                            primary_account_id);
}
#endif  // BUILDFLAG(IS_ANDROID)

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

#if BUILDFLAG(IS_ANDROID)
  jni_identity_mutator_.reset(new JniIdentityMutator(this));
  java_identity_mutator_ = Java_IdentityMutator_Constructor(
      base::android::AttachCurrentThread(),
      reinterpret_cast<intptr_t>(jni_identity_mutator_.get()));
#endif
}

IdentityMutator::~IdentityMutator() {
#if BUILDFLAG(IS_ANDROID)
  if (java_identity_mutator_)
    Java_IdentityMutator_destroy(base::android::AttachCurrentThread(),
                                 java_identity_mutator_);
#endif
}

#if BUILDFLAG(IS_ANDROID)
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
