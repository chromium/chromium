// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ANDROID_SYNC_SERVICE_ANDROID_BRIDGE_H_
#define COMPONENTS_SYNC_ANDROID_SYNC_SERVICE_ANDROID_BRIDGE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

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
  void AcknowledgeBookmarksLimitExceededError(int32_t source);
  bool IsSyncFeatureEnabled();
  bool IsSyncFeatureActive();
  bool IsSyncDisabledByEnterprisePolicy();
  bool IsEngineInitialized();
  void SetSetupInProgress(bool in_progress);
  bool IsInitialSyncFeatureSetupComplete();
  void SetInitialSyncFeatureSetupComplete();
  std::vector<int32_t> GetActiveDataTypes();
  std::vector<int32_t> GetSelectedTypes();
  void GetTypesWithUnsyncedData(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& callback);
  void GetLocalDataDescriptions(
      JNIEnv* env,
      const std::vector<int32_t>& types,
      const base::android::JavaRef<jobject>& callback);
  void TriggerLocalDataMigration(const std::vector<int32_t>& types);
  bool IsTypeManagedByPolicy(int32_t type);
  bool IsTypeManagedByCustodian(int32_t type);
  void SetSelectedType(int32_t type, bool is_type_on);
  bool IsCustomPassphraseAllowed();
  bool IsEncryptEverythingEnabled();
  bool IsPassphraseRequiredForPreferredDataTypes();
  bool IsTrustedVaultKeyRequired();
  bool IsTrustedVaultKeyRequiredForPreferredDataTypes();
  bool IsTrustedVaultRecoverabilityDegraded();
  bool IsUsingExplicitPassphrase();
  int32_t GetPassphraseType();
  int32_t GetTransportState();
  int32_t GetUserActionableError();
  void SetEncryptionPassphrase(const std::string& passphrase);
  bool SetDecryptionPassphrase(const std::string& passphrase);
  // Returns 0 if there's no passphrase time.
  int64_t GetExplicitPassphraseTime();
  void GetAllNodes(JNIEnv* env,
                   const base::android::JavaRef<jobject>& callback);
  GoogleServiceAuthError GetAuthError();
  base::android::ScopedJavaLocalRef<jobject> GetAccountInfo(JNIEnv* env);
  bool HasSyncConsent();
  bool IsPassphrasePromptMutedForCurrentProductVersion();
  void MarkPassphrasePromptMutedForCurrentProductVersion();
  bool ShouldOfferTrustedVaultOptIn();
  void TriggerRefresh();
  // Returns a timestamp for when a sync was last executed. The return value is
  // the internal value of base::Time.
  int64_t GetLastSyncedTimeForDebugging();
  void KeepAccountSettingsPrefsOnlyForUsers(
      const std::vector<std::string>& gaia_id_strings);

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
