// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ANDROID_SYNC_SERVICE_ANDROID_BRIDGE_H_
#define COMPONENTS_SYNC_ANDROID_SYNC_SERVICE_ANDROID_BRIDGE_H_

#include <cstdint>
#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/sync/service/sync_service_observer.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace syncer {

class SyncService;
class SyncSetupInProgressHandle;

// Forwards calls from SyncServiceImpl.java to the native SyncService and
// back. Instead of directly implementing JNI free functions, this class is used
// so it can manage the lifetime of objects like SyncSetupInProgressHandle.
// Note that on Android, there's only a single profile, a single native
// SyncService, and therefore a single instance of this class.
// Must only be accessed from the UI thread.
class SyncServiceAndroidBridge : public SyncServiceObserver {
 public:
  // `j_sync_service` must be an object implementing the SyncService interface.
  static SyncService* FromJavaObject(
      const base::android::JavaRef<jobject>& j_sync_service);

  // `native_sync_service` must be non-null and outlive this object.
  explicit SyncServiceAndroidBridge(SyncService* native_sync_service);
  ~SyncServiceAndroidBridge() override;

  SyncServiceAndroidBridge(const SyncServiceAndroidBridge&) = delete;
  SyncServiceAndroidBridge& operator=(const SyncServiceAndroidBridge&) = delete;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // SyncServiceObserver:
  void OnStateChanged(SyncService* sync) override;
  void OnSyncShutdown(SyncService* sync) override;

  // Please keep all methods below in the same order as the @NativeMethods in
  // SyncServiceImpl.java.
  void AcknowledgeBookmarksLimitExceededError(JNIEnv* env, int32_t source);
  bool IsSyncFeatureEnabled(JNIEnv* env);
  bool IsSyncFeatureActive(JNIEnv* env);
  bool IsSyncDisabledByEnterprisePolicy(JNIEnv* env);
  bool IsEngineInitialized(JNIEnv* env);
  void SetSetupInProgress(JNIEnv* env, bool in_progress);
  bool IsInitialSyncFeatureSetupComplete(JNIEnv* env);
  void SetInitialSyncFeatureSetupComplete(JNIEnv* env, int32_t source);
  base::android::ScopedJavaLocalRef<jintArray> GetActiveDataTypes(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jintArray> GetSelectedTypes(JNIEnv* env);
  void GetTypesWithUnsyncedData(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& callback);
  void GetLocalDataDescriptions(
      JNIEnv* env,
      const base::android::JavaRef<jintArray>& types,
      const base::android::JavaRef<jobject>& callback);
  void TriggerLocalDataMigration(
      JNIEnv* env,
      const base::android::JavaRef<jintArray>& types);
  bool IsTypeManagedByPolicy(JNIEnv* env, int32_t type);
  bool IsTypeManagedByCustodian(JNIEnv* env, int32_t type);
  void SetSelectedTypes(
      JNIEnv* env,
      bool sync_everything,
      const base::android::JavaRef<jintArray>& user_selectable_type_selection);
  void SetSelectedType(JNIEnv* env, int32_t type, bool is_type_on);
  bool IsCustomPassphraseAllowed(JNIEnv* env);
  bool IsEncryptEverythingEnabled(JNIEnv* env);
  bool IsPassphraseRequiredForPreferredDataTypes(JNIEnv* env);
  bool IsTrustedVaultKeyRequired(JNIEnv* env);
  bool IsTrustedVaultKeyRequiredForPreferredDataTypes(JNIEnv* env);
  bool IsTrustedVaultRecoverabilityDegraded(JNIEnv* env);
  bool IsUsingExplicitPassphrase(JNIEnv* env);
  int32_t GetPassphraseType(JNIEnv* env);
  int32_t GetTransportState(JNIEnv* env);
  int32_t GetUserActionableError(JNIEnv* env);
  void SetEncryptionPassphrase(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& passphrase);
  bool SetDecryptionPassphrase(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& passphrase);
  // Returns 0 if there's no passphrase time.
  int64_t GetExplicitPassphraseTime(JNIEnv* env);
  void GetAllNodes(JNIEnv* env,
                   const base::android::JavaRef<jobject>& callback);
  GoogleServiceAuthError GetAuthError(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetAccountInfo(JNIEnv* env);
  bool HasSyncConsent(JNIEnv* env);
  bool IsPassphrasePromptMutedForCurrentProductVersion(JNIEnv* env);
  void MarkPassphrasePromptMutedForCurrentProductVersion(JNIEnv* env);
  bool HasKeepEverythingSynced(JNIEnv* env);
  bool ShouldOfferTrustedVaultOptIn(JNIEnv* env);
  void TriggerRefresh(JNIEnv* env);
  // Returns a timestamp for when a sync was last executed. The return value is
  // the internal value of base::Time.
  int64_t GetLastSyncedTimeForDebugging(JNIEnv* env);
  void KeepAccountSettingsPrefsOnlyForUsers(
      JNIEnv* env,
      const base::android::JavaRef<jobjectArray>& gaia_ids);

 private:
  // A reference to the sync service for this profile.
  const raw_ptr<SyncService> native_sync_service_;

  // Java-side SyncServiceImpl object.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  // Prevents Sync from running until configuration is complete.
  std::unique_ptr<SyncSetupInProgressHandle> sync_blocker_;
};

}  // namespace syncer

namespace jni_zero {

template <>
inline syncer::SyncService* FromJniType<syncer::SyncService*>(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj) {
  return syncer::SyncServiceAndroidBridge::FromJavaObject(obj);
}

}  // namespace jni_zero

#endif  // COMPONENTS_SYNC_ANDROID_SYNC_SERVICE_ANDROID_BRIDGE_H_
