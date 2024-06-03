// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/android/tab_group_store_migration_utils.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "components/saved_tab_groups/android/tab_group_sync_conversions_utils.h"
#include "components/saved_tab_groups/jni_headers/TabGroupMetadataPersistentStore_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace tab_groups {

std::map<base::Uuid, LocalTabGroupID>
ReadAndClearIdMappingsForMigrationFromSharedPrefs() {
  std::map<base::Uuid, LocalTabGroupID> id_mappings;

  JNIEnv* env = base::android::AttachCurrentThread();

  // Read the entire shared pref into key-value pairs where key is sync ID and
  // value is the serialized local tab group ID.
  ScopedJavaLocalRef<jobjectArray> entries_array =
      Java_TabGroupMetadataPersistentStore_readAllDataForMigration(env);
  if (!entries_array) {
    LOG(ERROR) << "Failed to get entries array from SharedPreferences";
    return id_mappings;
  }

  // Walk through the list of pairs obtained from shared prefs and insert them
  // into the map.
  const int count = env->GetArrayLength(entries_array.obj());
  for (int i = 0; i < count; ++i) {
    ScopedJavaLocalRef<jobject> pair_obj = ScopedJavaLocalRef<jobject>(
        env, env->GetObjectArrayElement(entries_array.obj(), i));

    std::string sync_id_str = base::android::ConvertJavaStringToUTF8(
        Java_TabGroupMetadataPersistentStore_getFirstFromPair(env, pair_obj));
    std::string serialized_token_str = base::android::ConvertJavaStringToUTF8(
        Java_TabGroupMetadataPersistentStore_getSecondFromPair(env, pair_obj));

    auto sync_id = base::Uuid::ParseCaseInsensitive(sync_id_str);
    std::optional<base::Token> token =
        base::Token::FromString(serialized_token_str);
    if (!token) {
      LOG(ERROR) << "Unable to parse the token, skipping";
      continue;
    }

    id_mappings.emplace(sync_id, *token);
  }

  // Clear the SharedPreferences after migration so that next time the above
  // migration loop is a no-op.
  Java_TabGroupMetadataPersistentStore_clearAllData(env);

  return id_mappings;
}

void WriteMappingToSharedPrefsForTesting(const base::Uuid& sync_id,  // IN-TEST
                                         const LocalTabGroupID& local_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto j_sync_id = UuidToJavaString(env, sync_id);
  auto j_serialized_token =
      base::android::ConvertUTF8ToJavaString(env, local_id.ToString());

  Java_TabGroupMetadataPersistentStore_storeDataForTesting(  // IN-TEST
      env, j_sync_id, j_serialized_token);
}

void ClearSharedPrefsForTesting() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TabGroupMetadataPersistentStore_clearAllData(env);
}

}  // namespace tab_groups
