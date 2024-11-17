// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/android/sync_service_android_bridge.h"

#include <map>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_utils.h"
#include "components/sync/service/sync_user_settings.h"
#include "google_apis/gaia/google_service_auth_error.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/sync/android/jni_headers/SyncServiceImpl_jni.h"
#include "components/sync/android/jni_headers/SyncService_jni.h"

using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace syncer {

namespace {

DataType IntToDataTypeChecked(int type) {
  CHECK_GE(type, static_cast<int>(DataType::FIRST_REAL_DATA_TYPE));
  CHECK_LE(type, static_cast<int>(DataType::LAST_REAL_DATA_TYPE));
  return static_cast<DataType>(type);
}

ScopedJavaLocalRef<jintArray> DataTypeSetToJavaIntArray(JNIEnv* env,
                                                        DataTypeSet types) {
  std::vector<int> type_vector;
  for (DataType type : types) {
    type_vector.push_back(type);
  }
  return base::android::ToJavaIntArray(env, type_vector);
}

DataTypeSet JavaIntArrayToDataTypeSet(JNIEnv* env,
                                      const JavaParamRef<jintArray>& types) {
  std::vector<int> types_vector;
  base::android::JavaIntArrayToIntVector(env, types, &types_vector);
  DataTypeSet data_type_set;
  for (int type : types_vector) {
    data_type_set.Put(IntToDataTypeChecked(type));
  }
  return data_type_set;
}

ScopedJavaLocalRef<jintArray> UserSelectableTypeSetToJavaIntArray(
    JNIEnv* env,
    UserSelectableTypeSet types) {
  std::vector<int> type_vector;
  for (UserSelectableType type : types) {
    type_vector.push_back(static_cast<int>(type));
  }
  return base::android::ToJavaIntArray(env, type_vector);
}

// Native callback for the JNI GetTypesWithUnsyncedData method. When
// SyncService::GetTypesWithUnsyncedData() completes, this method is called and
// the results are sent to the Java callback.
void NativeGetTypesWithUnsyncedDataCallback(
    JNIEnv* env,
    const base::android::ScopedJavaGlobalRef<jobject>& callback,
    DataTypeSet types) {
  Java_SyncServiceImpl_onGetTypesWithUnsyncedDataResult(
      env, callback, DataTypeSetToJavaIntArray(env, types));
}

void NativeGetLocalDataDescriptionsCallback(
    JNIEnv* env,
    const base::android::ScopedJavaGlobalRef<jobject>& callback,
    std::map<DataType, LocalDataDescription> localDataDescription) {
  std::vector<int> data_types;
  std::vector<LocalDataDescription> local_data_descriptions;
  for (const auto& [data_type, description] : localDataDescription) {
    data_types.push_back(data_type);
    local_data_descriptions.push_back(description);
  }
  base::android::ScopedJavaLocalRef<jclass> localdatadescription_clazz =
      base::android::GetClass(
          env, "org/chromium/components/sync/LocalDataDescription");
  base::android::ScopedJavaLocalRef<jobjectArray> array(
      env, env->NewObjectArray(local_data_descriptions.size(),
                               localdatadescription_clazz.obj(), nullptr));
  base::android::CheckException(env);

  for (size_t i = 0; i < local_data_descriptions.size(); ++i) {
    base::android::ScopedJavaLocalRef<jobject> item =
        ConvertToJavaLocalDataDescription(env, local_data_descriptions[i]);
    env->SetObjectArrayElement(array.obj(), i, item.obj());
  }

  Java_SyncServiceImpl_onGetLocalDataDescriptionsResult(
      env, callback, base::android::ToJavaIntArray(env, data_types), array);
}

// Native callback for the JNI GetAllNodes method. When
// SyncService::GetAllNodesForDebugging() completes, this method is called and
// the results are sent to the Java callback.
void NativeGetAllNodesCallback(
    JNIEnv* env,
    const base::android::ScopedJavaGlobalRef<jobject>& callback,
    base::Value::List result) {
  std::string json_string;
  if (!base::JSONWriter::Write(result, &json_string)) {
    DVLOG(1) << "Writing as JSON failed. Passing empty string to Java code.";
    json_string = std::string();
  }

  Java_SyncServiceImpl_onGetAllNodesResult(
      env, callback, ConvertUTF8ToJavaString(env, json_string));
}

UserSelectableType IntToUserSelectableTypeChecked(int type) {
  CHECK_GE(type, static_cast<int>(UserSelectableType::kFirstType));
  CHECK_LE(type, static_cast<int>(UserSelectableType::kLastType));
  return static_cast<UserSelectableType>(type);
}

}  // namespace

// static
SyncService* SyncServiceAndroidBridge::FromJavaObject(
    const base::android::JavaRef<jobject>& j_sync_service) {
  if (!j_sync_service) {
    return nullptr;
  }
  auto* bridge = reinterpret_cast<SyncServiceAndroidBridge*>(
      Java_SyncService_getNativeSyncServiceAndroidBridge(AttachCurrentThread(),
                                                         j_sync_service));
  return bridge ? bridge->native_sync_service_ : nullptr;
}

SyncServiceAndroidBridge::SyncServiceAndroidBridge(
    SyncService* native_sync_service)
    : native_sync_service_(native_sync_service) {
  DCHECK(native_sync_service_);

  java_ref_.Reset(Java_SyncServiceImpl_Constructor(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this)));

  native_sync_service_->AddObserver(this);
}

SyncServiceAndroidBridge::~SyncServiceAndroidBridge() = default;

ScopedJavaLocalRef<jobject> SyncServiceAndroidBridge::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(java_ref_);
}

void SyncServiceAndroidBridge::OnStateChanged(SyncService* sync) {
  // Notify the java world that our sync state has changed.
  JNIEnv* env = AttachCurrentThread();
  Java_SyncServiceImpl_syncStateChanged(env, java_ref_);
}

void SyncServiceAndroidBridge::OnSyncShutdown(SyncService* sync) {
  native_sync_service_->RemoveObserver(this);
  Java_SyncServiceImpl_destroy(AttachCurrentThread(), java_ref_);
  // Not worth resetting `native_sync_service_`, it owns this object and will
  // destroy it shortly.
}

void SyncServiceAndroidBridge::SetSyncRequested(JNIEnv* env) {
  native_sync_service_->SetSyncFeatureRequested();
}

jboolean SyncServiceAndroidBridge::IsSyncFeatureEnabled(JNIEnv* env) {
  return native_sync_service_->IsSyncFeatureEnabled();
}

jboolean SyncServiceAndroidBridge::IsSyncFeatureActive(JNIEnv* env) {
  return native_sync_service_->IsSyncFeatureActive();
}

jboolean SyncServiceAndroidBridge::IsSyncDisabledByEnterprisePolicy(
    JNIEnv* env) {
  return native_sync_service_->HasDisableReason(
      SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
}

jboolean SyncServiceAndroidBridge::IsEngineInitialized(JNIEnv* env) {
  return native_sync_service_->IsEngineInitialized();
}

void SyncServiceAndroidBridge::SetSetupInProgress(JNIEnv* env,
                                                  jboolean in_progress) {
  if (!in_progress) {
    sync_blocker_.reset();
    return;
  }

  if (!sync_blocker_) {
    sync_blocker_ = native_sync_service_->GetSetupInProgressHandle();
  }
}

jboolean SyncServiceAndroidBridge::IsInitialSyncFeatureSetupComplete(
    JNIEnv* env) {
  return native_sync_service_->GetUserSettings()
      ->IsInitialSyncFeatureSetupComplete();
}

void SyncServiceAndroidBridge::SetInitialSyncFeatureSetupComplete(JNIEnv* env,
                                                                  jint source) {
  native_sync_service_->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      static_cast<SyncFirstSetupCompleteSource>(source));
}

ScopedJavaLocalRef<jintArray> SyncServiceAndroidBridge::GetActiveDataTypes(
    JNIEnv* env) {
  return DataTypeSetToJavaIntArray(env,
                                   native_sync_service_->GetActiveDataTypes());
}

ScopedJavaLocalRef<jintArray> SyncServiceAndroidBridge::GetSelectedTypes(
    JNIEnv* env) {
  UserSelectableTypeSet user_selectable_types;
  user_selectable_types =
      native_sync_service_->GetUserSettings()->GetSelectedTypes();
  return UserSelectableTypeSetToJavaIntArray(env, user_selectable_types);
}

void SyncServiceAndroidBridge::GetTypesWithUnsyncedData(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& callback) {
  base::android::ScopedJavaGlobalRef<jobject> java_callback;
  java_callback.Reset(env, callback);
  native_sync_service_->GetTypesWithUnsyncedData(
      TypesRequiringUnsyncedDataCheckOnSignout(),
      base::BindOnce(&NativeGetTypesWithUnsyncedDataCallback, env,
                     java_callback));
}

void SyncServiceAndroidBridge::GetLocalDataDescriptions(
    JNIEnv* env,
    const base::android::JavaParamRef<jintArray>& types,
    const base::android::JavaParamRef<jobject>& callback) {
  base::android::ScopedJavaGlobalRef<jobject> java_callback;
  java_callback.Reset(env, callback);

  native_sync_service_->GetLocalDataDescriptions(
      JavaIntArrayToDataTypeSet(env, types),
      base::BindOnce(&NativeGetLocalDataDescriptionsCallback, env,
                     java_callback));
}

void SyncServiceAndroidBridge::TriggerLocalDataMigration(
    JNIEnv* env,
    const JavaParamRef<jintArray>& types) {
  native_sync_service_->TriggerLocalDataMigration(
      JavaIntArrayToDataTypeSet(env, types));
}

jboolean SyncServiceAndroidBridge::IsTypeManagedByPolicy(JNIEnv* env,
                                                         jint type) {
  return native_sync_service_->GetUserSettings()->IsTypeManagedByPolicy(
      IntToUserSelectableTypeChecked(type));
}

jboolean SyncServiceAndroidBridge::IsTypeManagedByCustodian(JNIEnv* env,
                                                            jint type) {
  return native_sync_service_->GetUserSettings()->IsTypeManagedByCustodian(
      IntToUserSelectableTypeChecked(type));
}

void SyncServiceAndroidBridge::SetSelectedTypes(
    JNIEnv* env,
    jboolean sync_everything,
    const JavaParamRef<jintArray>& user_selectable_type_array) {
  std::vector<int> types_vector;
  base::android::JavaIntArrayToIntVector(env, user_selectable_type_array,
                                         &types_vector);

  UserSelectableTypeSet user_selectable_types;
  for (int type : types_vector) {
    user_selectable_types.Put(IntToUserSelectableTypeChecked(type));
  }

  native_sync_service_->GetUserSettings()->SetSelectedTypes(
      sync_everything, user_selectable_types);
}

void SyncServiceAndroidBridge::SetSelectedType(JNIEnv* env,
                                               jint type,
                                               jboolean is_type_on) {
  native_sync_service_->GetUserSettings()->SetSelectedType(
      IntToUserSelectableTypeChecked(type), is_type_on);
}

jboolean SyncServiceAndroidBridge::IsCustomPassphraseAllowed(JNIEnv* env) {
  return native_sync_service_->GetUserSettings()->IsCustomPassphraseAllowed();
}

jboolean SyncServiceAndroidBridge::IsEncryptEverythingEnabled(JNIEnv* env) {
  return native_sync_service_->GetUserSettings()->IsEncryptEverythingEnabled();
}

jboolean SyncServiceAndroidBridge::IsPassphraseRequiredForPreferredDataTypes(
    JNIEnv* env) {
  return native_sync_service_->GetUserSettings()
      ->IsPassphraseRequiredForPreferredDataTypes();
}

jboolean SyncServiceAndroidBridge::IsTrustedVaultKeyRequired(JNIEnv* env) {
  return native_sync_service_->GetUserSettings()->IsTrustedVaultKeyRequired();
}

jboolean
SyncServiceAndroidBridge::IsTrustedVaultKeyRequiredForPreferredDataTypes(
    JNIEnv* env) {
  return native_sync_service_->GetUserSettings()
      ->IsTrustedVaultKeyRequiredForPreferredDataTypes();
}

jboolean SyncServiceAndroidBridge::IsTrustedVaultRecoverabilityDegraded(
    JNIEnv* env) {
  return native_sync_service_->GetUserSettings()
      ->IsTrustedVaultRecoverabilityDegraded();
}

jboolean SyncServiceAndroidBridge::IsUsingExplicitPassphrase(JNIEnv* env) {
  return native_sync_service_->GetUserSettings()->IsUsingExplicitPassphrase();
}

jint SyncServiceAndroidBridge::GetPassphraseType(JNIEnv* env) {
  // TODO(crbug.com/40923935): Mapping nullopt -> kImplicitPassphrase preserves
  // the historic behavior, but ideally we should propagate the nullopt state to
  // Java.
  return static_cast<jint>(
      native_sync_service_->GetUserSettings()->GetPassphraseType().value_or(
          PassphraseType::kImplicitPassphrase));
}

jint SyncServiceAndroidBridge::GetTransportState(JNIEnv* env) {
  return static_cast<jint>(native_sync_service_->GetTransportState());
}

void SyncServiceAndroidBridge::SetEncryptionPassphrase(
    JNIEnv* env,
    const JavaParamRef<jstring>& passphrase) {
  native_sync_service_->GetUserSettings()->SetEncryptionPassphrase(
      ConvertJavaStringToUTF8(env, passphrase));
}

jboolean SyncServiceAndroidBridge::SetDecryptionPassphrase(
    JNIEnv* env,
    const JavaParamRef<jstring>& passphrase) {
  return native_sync_service_->GetUserSettings()->SetDecryptionPassphrase(
      ConvertJavaStringToUTF8(env, passphrase));
}

jlong SyncServiceAndroidBridge::GetExplicitPassphraseTime(JNIEnv* env) {
  return native_sync_service_->GetUserSettings()
      ->GetExplicitPassphraseTime()
      .InMillisecondsSinceUnixEpoch();
}

void SyncServiceAndroidBridge::GetAllNodes(
    JNIEnv* env,
    const JavaParamRef<jobject>& callback) {
  base::android::ScopedJavaGlobalRef<jobject> java_callback;
  java_callback.Reset(env, callback);
  native_sync_service_->GetAllNodesForDebugging(
      base::BindOnce(&NativeGetAllNodesCallback, env, java_callback));
}

jint SyncServiceAndroidBridge::GetAuthError(JNIEnv* env) {
  return native_sync_service_->GetAuthError().state();
}

jboolean SyncServiceAndroidBridge::HasUnrecoverableError(JNIEnv* env) {
  return native_sync_service_->HasUnrecoverableError();
}

jboolean SyncServiceAndroidBridge::RequiresClientUpgrade(JNIEnv* env) {
  return native_sync_service_->RequiresClientUpgrade();
}

base::android::ScopedJavaLocalRef<jobject>
SyncServiceAndroidBridge::GetAccountInfo(JNIEnv* env) {
  CoreAccountInfo account_info = native_sync_service_->GetAccountInfo();
  return account_info.IsEmpty()
             ? nullptr
             : ConvertToJavaCoreAccountInfo(env, account_info);
}

jboolean SyncServiceAndroidBridge::HasSyncConsent(JNIEnv* env) {
  return native_sync_service_->HasSyncConsent();
}

jboolean
SyncServiceAndroidBridge::IsPassphrasePromptMutedForCurrentProductVersion(
    JNIEnv* env) {
  return native_sync_service_->GetUserSettings()
      ->IsPassphrasePromptMutedForCurrentProductVersion();
}

void SyncServiceAndroidBridge::
    MarkPassphrasePromptMutedForCurrentProductVersion(JNIEnv* env) {
  native_sync_service_->GetUserSettings()
      ->MarkPassphrasePromptMutedForCurrentProductVersion();
}

jboolean SyncServiceAndroidBridge::HasKeepEverythingSynced(JNIEnv* env) {
  return native_sync_service_->GetUserSettings()->IsSyncEverythingEnabled();
}

jboolean SyncServiceAndroidBridge::ShouldOfferTrustedVaultOptIn(JNIEnv* env) {
  return syncer::ShouldOfferTrustedVaultOptIn(native_sync_service_);
}

void SyncServiceAndroidBridge::TriggerRefresh(JNIEnv* env) {
  native_sync_service_->TriggerRefresh(DataTypeSet::All());
}

jlong SyncServiceAndroidBridge::GetLastSyncedTimeForDebugging(JNIEnv* env) {
  base::Time last_sync_time =
      native_sync_service_->GetLastSyncedTimeForDebugging();
  return static_cast<jlong>(
      (last_sync_time - base::Time::UnixEpoch()).InMicroseconds());
}

void SyncServiceAndroidBridge::KeepAccountSettingsPrefsOnlyForUsers(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& gaia_ids) {
  std::vector<std::string> gaia_id_strings;
  AppendJavaStringArrayToStringVector(env, gaia_ids, &gaia_id_strings);
  std::vector<signin::GaiaIdHash> gaia_id_hashes;
  for (const std::string& gaia_id_string : gaia_id_strings) {
    gaia_id_hashes.push_back(signin::GaiaIdHash::FromGaiaId(gaia_id_string));
  }
  native_sync_service_->GetUserSettings()->KeepAccountSettingsPrefsOnlyForUsers(
      gaia_id_hashes);
}

}  // namespace syncer
