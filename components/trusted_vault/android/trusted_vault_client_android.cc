// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/android/trusted_vault_client_android.h"

#include <utility>
#include <variant>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/check_op.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "components/sync/service/sync_service_utils.h"
#include "components/trusted_vault/trusted_vault_key_and_version.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_id.h"
#include "third_party/jni_zero/default_conversions.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after headers that provide symbols used by @JniType.
#include "components/trusted_vault/android/jni_headers/TrustedVaultKeyAndVersion_jni.h"

namespace jni_zero {

template <>
inline trusted_vault::TrustedVaultKeyAndVersion
FromJniType<trusted_vault::TrustedVaultKeyAndVersion>(
    JNIEnv* env,
    const JavaRef<jobject>& j_object) {
  return trusted_vault::TrustedVaultKeyAndVersion(
      Java_TrustedVaultKeyAndVersion_getKey(env, j_object),
      Java_TrustedVaultKeyAndVersion_getVersion(env, j_object));
}

}  // namespace jni_zero

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/trusted_vault/android/jni_headers/TrustedVaultClient_jni.h"

namespace trusted_vault {

TrustedVaultClientAndroid::OngoingFetchKeys::OngoingFetchKeys(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(const std::vector<TrustedVaultKeyAndVersion>&)>
        callback)
    : account_info(account_info), callback(std::move(callback)) {}

TrustedVaultClientAndroid::OngoingFetchKeys::OngoingFetchKeys(
    OngoingFetchKeys&&) = default;

TrustedVaultClientAndroid::OngoingFetchKeys::~OngoingFetchKeys() = default;

TrustedVaultClientAndroid::OngoingMarkLocalKeysAsStale::
    OngoingMarkLocalKeysAsStale(base::OnceCallback<void(bool)> callback)
    : callback(std::move(callback)) {}

TrustedVaultClientAndroid::OngoingMarkLocalKeysAsStale::
    OngoingMarkLocalKeysAsStale(OngoingMarkLocalKeysAsStale&&) = default;

TrustedVaultClientAndroid::OngoingMarkLocalKeysAsStale::
    ~OngoingMarkLocalKeysAsStale() = default;

TrustedVaultClientAndroid::OngoingGetIsRecoverabilityDegraded::
    OngoingGetIsRecoverabilityDegraded(base::OnceCallback<void(bool)> callback)
    : callback(std::move(callback)) {}

TrustedVaultClientAndroid::OngoingGetIsRecoverabilityDegraded::
    OngoingGetIsRecoverabilityDegraded(OngoingGetIsRecoverabilityDegraded&&) =
        default;

TrustedVaultClientAndroid::OngoingGetIsRecoverabilityDegraded::
    ~OngoingGetIsRecoverabilityDegraded() = default;

TrustedVaultClientAndroid::OngoingAddTrustedRecoveryMethod::
    OngoingAddTrustedRecoveryMethod(base::OnceCallback<void(bool)> callback)
    : callback(std::move(callback)) {}

TrustedVaultClientAndroid::OngoingAddTrustedRecoveryMethod::
    OngoingAddTrustedRecoveryMethod(OngoingAddTrustedRecoveryMethod&&) =
        default;

TrustedVaultClientAndroid::OngoingAddTrustedRecoveryMethod::
    ~OngoingAddTrustedRecoveryMethod() = default;

TrustedVaultClientAndroid::OngoingCanEnroll::OngoingCanEnroll(
    base::OnceCallback<void(bool)> callback)
    : callback(std::move(callback)) {}

TrustedVaultClientAndroid::OngoingCanEnroll::OngoingCanEnroll(
    OngoingCanEnroll&&) noexcept = default;

TrustedVaultClientAndroid::OngoingCanEnroll::~OngoingCanEnroll() = default;

TrustedVaultClientAndroid::OngoingEnroll::OngoingEnroll(
    base::OnceCallback<void(const std::vector<uint8_t>&, int)> callback)
    : callback(std::move(callback)) {}

TrustedVaultClientAndroid::OngoingEnroll::OngoingEnroll(
    OngoingEnroll&&) noexcept = default;

TrustedVaultClientAndroid::OngoingEnroll::~OngoingEnroll() = default;

TrustedVaultClientAndroid::TrustedVaultClientAndroid(
    SecurityDomainId security_domain_id,
    const GetAccountInfoByGaiaIdCallback& gaia_account_info_by_gaia_id_cb)
    : security_domain_id_(security_domain_id),
      gaia_account_info_by_gaia_id_cb_(gaia_account_info_by_gaia_id_cb) {
  JNIEnv* const env = base::android::AttachCurrentThread();
  Java_TrustedVaultClient_registerNative(
      env, reinterpret_cast<intptr_t>(this),
      static_cast<int32_t>(security_domain_id_));
}

TrustedVaultClientAndroid::~TrustedVaultClientAndroid() {
  JNIEnv* const env = base::android::AttachCurrentThread();
  Java_TrustedVaultClient_unregisterNative(
      env, reinterpret_cast<intptr_t>(this),
      static_cast<int32_t>(security_domain_id_));
}

void TrustedVaultClientAndroid::FetchKeysCompleted(
    JNIEnv* env,
    int32_t request_id,
    const std::string& gaia_id,
    const std::vector<TrustedVaultKeyAndVersion>& keys) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OngoingRequest ongoing_request = GetAndUnregisterOngoingRequest(request_id);
  OngoingFetchKeys& ongoing_fetch_keys =
      std::get<OngoingFetchKeys>(ongoing_request);

  DCHECK_EQ(ongoing_fetch_keys.account_info.gaia, GaiaId(gaia_id))
      << "User mismatch in FetchKeys() response";

  std::move(ongoing_fetch_keys.callback).Run(keys);
}

void TrustedVaultClientAndroid::MarkLocalKeysAsStaleCompleted(
    JNIEnv* env,
    int32_t request_id,
    bool succeeded) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OngoingRequest ongoing_request = GetAndUnregisterOngoingRequest(request_id);

  std::move(std::get<OngoingMarkLocalKeysAsStale>(ongoing_request).callback)
      .Run(!!succeeded);
}

void TrustedVaultClientAndroid::GetIsRecoverabilityDegradedCompleted(
    JNIEnv* env,
    int32_t request_id,
    bool is_degraded) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OngoingRequest ongoing_request = GetAndUnregisterOngoingRequest(request_id);

  std::move(
      std::get<OngoingGetIsRecoverabilityDegraded>(ongoing_request).callback)
      .Run(!!is_degraded);
}

void TrustedVaultClientAndroid::AddTrustedRecoveryMethodCompleted(
    JNIEnv* env,
    int32_t request_id,
    bool succeeded) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OngoingRequest ongoing_request = GetAndUnregisterOngoingRequest(request_id);

  std::move(std::get<OngoingAddTrustedRecoveryMethod>(ongoing_request).callback)
      .Run(!!succeeded);
}

void TrustedVaultClientAndroid::CanEnrollCompleted(JNIEnv* env,
                                                   int32_t request_id,
                                                   bool can_enroll) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OngoingRequest ongoing_request = GetAndUnregisterOngoingRequest(request_id);

  std::move(std::get<OngoingCanEnroll>(ongoing_request).callback)
      .Run(!!can_enroll);
}

void TrustedVaultClientAndroid::EnrollCompleted(
    JNIEnv* env,
    int32_t request_id,
    const base::android::JavaRef<jbyteArray>& key,
    int32_t key_version) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OngoingRequest ongoing_request = GetAndUnregisterOngoingRequest(request_id);

  std::vector<uint8_t> converted_key;
  base::android::JavaByteArrayToByteVector(env, key, &converted_key);
  std::move(std::get<OngoingEnroll>(ongoing_request).callback)
      .Run(converted_key, key_version);
}

void TrustedVaultClientAndroid::NotifyKeysChanged(
    JNIEnv* env,
    std::optional<int32_t> trigger) {
  for (Observer& observer : observer_list_) {
    observer.OnTrustedVaultKeysChanged(
        trigger
            ? std::optional(
                  static_cast<TrustedVaultUserActionTriggerForUMA>(*trigger))
            : std::nullopt);
  }
}

void TrustedVaultClientAndroid::NotifyRecoverabilityChanged(JNIEnv* env) {
  for (Observer& observer : observer_list_) {
    observer.OnTrustedVaultRecoverabilityChanged();
  }
}

void TrustedVaultClientAndroid::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void TrustedVaultClientAndroid::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void TrustedVaultClientAndroid::FetchKeys(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)> cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Store for later completion when Java invokes FetchKeysCompleted().
  // TODO(crbug.com/588274186): Remove this adapter once
  // `TrustedVaultClient::FetchKeys()` returns `TrustedVaultKeyAndVersion`.
  const RequestId request_id = RegisterNewOngoingRequest(OngoingFetchKeys(
      account_info,
      base::BindOnce([](const std::vector<TrustedVaultKeyAndVersion>& keys) {
        return base::ToVector(keys, &TrustedVaultKeyAndVersion::key);
      }).Then(std::move(cb))));

  // Trigger the fetching keys from the implementation in Java, which will
  // eventually call FetchKeysCompleted().
  Java_TrustedVaultClient_fetchKeys(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      static_cast<int32_t>(security_domain_id_), request_id, account_info);
}

void TrustedVaultClientAndroid::StoreKeys(
    const GaiaId& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version,
    std::optional<TrustedVaultUserActionTriggerForUMA> trigger) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JNIEnv* const env = base::android::AttachCurrentThread();
  const base::android::ScopedJavaLocalRef<jobjectArray> java_keys =
      base::android::ToJavaArrayOfByteArray(env, keys);
  Java_TrustedVaultClient_storeKeys(env, reinterpret_cast<intptr_t>(this),
                                    static_cast<int32_t>(security_domain_id_),
                                    gaia_id, java_keys, last_key_version);
}

void TrustedVaultClientAndroid::MarkLocalKeysAsStale(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(cb);

  // Store for later completion when Java invokes
  // MarkLocalKeysAsStaleCompleted().
  const RequestId request_id =
      RegisterNewOngoingRequest(OngoingMarkLocalKeysAsStale(std::move(cb)));

  // The Java implementation will eventually call
  // MarkLocalKeysAsStaleCompleted().
  Java_TrustedVaultClient_markLocalKeysAsStale(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      static_cast<int32_t>(security_domain_id_), request_id, account_info);
}

void TrustedVaultClientAndroid::GetIsRecoverabilityDegraded(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(cb);

  // Store for later completion when Java invokes
  // GetIsRecoverabilityDegradedCompleted().
  const RequestId request_id = RegisterNewOngoingRequest(
      OngoingGetIsRecoverabilityDegraded(std::move(cb)));

  // The Java implementation will eventually call
  // MarkLocalKeysAsStaleCompleted().
  Java_TrustedVaultClient_getIsRecoverabilityDegraded(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      static_cast<int32_t>(security_domain_id_), request_id, account_info);
}

void TrustedVaultClientAndroid::AddTrustedRecoveryMethod(
    const GaiaId& gaia_id,
    const std::vector<uint8_t>& public_key,
    int method_type_hint,
    base::OnceCallback<void(bool)> cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(cb);

  const CoreAccountInfo account_info =
      gaia_account_info_by_gaia_id_cb_.Run(gaia_id);

  // TODO(crbug.com/540854648): Record the same metric for the passkeys security
  // domain.
  if (security_domain_id_ == SecurityDomainId::kChromeSync) {
    base::UmaHistogramBoolean(
        "Sync.TrustedVaultJavascriptAddRecoveryMethodUserKnown",
        account_info != CoreAccountInfo());
  }

  if (account_info == CoreAccountInfo()) {
    std::move(cb).Run(false);
    return;
  }

  // Store for later completion when Java invokes
  // AddTrustedRecoveryMethodCompleted().
  const RequestId request_id =
      RegisterNewOngoingRequest(OngoingAddTrustedRecoveryMethod(std::move(cb)));

  JNIEnv* const env = base::android::AttachCurrentThread();
  const base::android::ScopedJavaLocalRef<jbyteArray> java_public_key =
      base::android::ToJavaByteArray(env, public_key);

  // The Java implementation will eventually call
  // AddTrustedRecoveryMethodCompleted().
  Java_TrustedVaultClient_addTrustedRecoveryMethod(
      env, reinterpret_cast<intptr_t>(this),
      static_cast<int32_t>(security_domain_id_), request_id, account_info,
      java_public_key, method_type_hint);
}

void TrustedVaultClientAndroid::ClearLocalDataForAccount(
    const CoreAccountInfo& account_info) {
  // Not relevant for Android implementation.
}

void TrustedVaultClientAndroid::CanEnroll(const CoreAccountInfo& account_info,
                                          base::OnceCallback<void(bool)> cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(cb);

  // Store for later completion when Java invokes CanEnrollCompleted().
  const RequestId request_id =
      RegisterNewOngoingRequest(OngoingCanEnroll(std::move(cb)));

  // The Java implementation will eventually call CanEnrollCompleted().
  Java_TrustedVaultClient_canEnroll(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      static_cast<int32_t>(security_domain_id_), request_id, account_info);
}

void TrustedVaultClientAndroid::Enroll(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(const std::vector<uint8_t>&, int)> cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(cb);

  // Store for later completion when Java invokes EnrollCompleted().
  const RequestId request_id =
      RegisterNewOngoingRequest(OngoingEnroll(std::move(cb)));

  // The Java implementation will eventually call EnrollCompleted().
  Java_TrustedVaultClient_enroll(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      static_cast<int32_t>(security_domain_id_), request_id, account_info);
}

TrustedVaultClientAndroid::RequestId
TrustedVaultClientAndroid::RegisterNewOngoingRequest(OngoingRequest request) {
  ongoing_requests_.emplace(++last_request_id_, std::move(request));
  return last_request_id_;
}

TrustedVaultClientAndroid::OngoingRequest
TrustedVaultClientAndroid::GetAndUnregisterOngoingRequest(RequestId id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto it = ongoing_requests_.find(id);
  CHECK(it != ongoing_requests_.end());

  OngoingRequest request = std::move(it->second);
  ongoing_requests_.erase(it);
  return request;
}

static void JNI_TrustedVaultClient_RecordKeyRetrievalTrigger(JNIEnv* env,
                                                             int trigger) {
  syncer::RecordKeyRetrievalTrigger(
      static_cast<TrustedVaultUserActionTriggerForUMA>(trigger));
}

static void JNI_TrustedVaultClient_RecordRecoverabilityDegradedFixTrigger(
    JNIEnv* env,
    int trigger) {
  syncer::RecordRecoverabilityDegradedFixTrigger(
      static_cast<TrustedVaultUserActionTriggerForUMA>(trigger));
}

}  // namespace trusted_vault

DEFINE_JNI(TrustedVaultClient)
DEFINE_JNI(TrustedVaultKeyAndVersion)
