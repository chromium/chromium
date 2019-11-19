// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SERVER_ANDROID_FAKE_SERVER_HELPER_ANDROID_H_
#define COMPONENTS_SYNC_TEST_FAKE_SERVER_ANDROID_FAKE_SERVER_HELPER_ANDROID_H_

#include <jni.h>

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "components/sync/test/fake_server/entity_builder_factory.h"

// Helper for utilizing native FakeServer infrastructure in Android tests.
class FakeServerHelperAndroid {
 public:
  // Creates a FakeServerHelperAndroid.
  FakeServerHelperAndroid(JNIEnv* env, jobject obj);

  // Factory method for creating a native FakeServer object. The caller assumes
  // ownership.
  jlong CreateFakeServer(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         jlong profile_sync_service);

  // Deletes the given |fake_server| (a FakeServer pointer created via
  // CreateFakeServer).
  void DeleteFakeServer(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        jlong fake_server,
                        jlong profile_sync_service);

  // Returns true if and only if |fake_server| contains |count| entities that
  // match |model_type_string| and |name|.
  jboolean VerifyEntityCountByTypeAndName(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong fake_server,
      jlong count,
      jint model_type_int,
      const base::android::JavaParamRef<jstring>& name);

  // Returns true iff |fake_server| has exactly one window of sessions with
  // tabs matching |url_array|. The order of the array does not matter.
  jboolean VerifySessions(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong fake_server,
      const base::android::JavaParamRef<jobjectArray>& url_array);

  // Return the entities for |model_type_string| on |fake_server|.
  base::android::ScopedJavaLocalRef<jobjectArray> GetSyncEntitiesByModelType(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong fake_server,
      jint model_type_int);

  // Injects a UniqueClientEntity into |fake_server|.
  void InjectUniqueClientEntity(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong fake_server,
      const base::android::JavaParamRef<jstring>& non_unique_name,
      const base::android::JavaParamRef<jstring>& client_tag,
      const base::android::JavaParamRef<jbyteArray>&
          serialized_entity_specifics);

  // Sets the Wallet card and address data to be served in following GetUpdates
  // requests.
  void SetWalletData(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong fake_server,
      const base::android::JavaParamRef<jbyteArray>& serialized_entity);

  // Modifies the entity with |id| on |fake_server|.
  void ModifyEntitySpecifics(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             jlong fake_server,
                             const base::android::JavaParamRef<jstring>& name,
                             const base::android::JavaParamRef<jbyteArray>&
                                 serialized_entity_specifics);

  // Injects a BookmarkEntity into |fake_server|.
  void InjectBookmarkEntity(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong fake_server,
      const base::android::JavaParamRef<jstring>& title,
      const base::android::JavaParamRef<jstring>& url,
      const base::android::JavaParamRef<jstring>& parent_id);

  // Injects a bookmark folder entity into |fake_server|.
  void InjectBookmarkFolderEntity(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong fake_server,
      const base::android::JavaParamRef<jstring>& title,
      const base::android::JavaParamRef<jstring>& parent_id);

  // Modify the BookmarkEntity with |entity_id| on |fake_server|.
  void ModifyBookmarkEntity(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong fake_server,
      const base::android::JavaParamRef<jstring>& entity_id,
      const base::android::JavaParamRef<jstring>& title,
      const base::android::JavaParamRef<jstring>& url,
      const base::android::JavaParamRef<jstring>& parent_id);

  // Modify the bookmark folder with |entity_id| on |fake_server|.
  void ModifyBookmarkFolderEntity(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong fake_server,
      const base::android::JavaParamRef<jstring>& entity_id,
      const base::android::JavaParamRef<jstring>& title,
      const base::android::JavaParamRef<jstring>& parent_id);

  // Returns the bookmark bar folder ID.
  base::android::ScopedJavaLocalRef<jstring> GetBookmarkBarFolderId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong fake_server);

  // Deletes an entity on the server. This is the JNI way of injecting a
  // tombstone.
  void DeleteEntity(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong fake_server,
      const base::android::JavaParamRef<jstring>& id,
      const base::android::JavaParamRef<jstring>& client_tag_hash);

  // Simulates a dashboard stop and clear.
  void ClearServerData(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       jlong fake_server);

 private:
  virtual ~FakeServerHelperAndroid();

  // Deserializes |serialized_entity| into |entity|.
  void DeserializeEntity(JNIEnv* env,
                         jbyteArray serialized_entity,
                         sync_pb::SyncEntity* entity);

  // Deserializes |serialized_entity_specifics| into |entity_specifics|.
  void DeserializeEntitySpecifics(JNIEnv* env,
                                  const base::android::JavaParamRef<jbyteArray>&
                                      serialized_entity_specifics,
                                  sync_pb::EntitySpecifics* entity_specifics);

  // Creates a bookmark entity.
  std::unique_ptr<syncer::LoopbackServerEntity> CreateBookmarkEntity(
      JNIEnv* env,
      jstring title,
      jstring url,
      jstring parent_id);
};

#endif  // COMPONENTS_SYNC_TEST_FAKE_SERVER_ANDROID_FAKE_SERVER_HELPER_ANDROID_H_
