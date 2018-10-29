// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_server/android/fake_server_helper_android.h"

#include <stddef.h>

#include <set>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/net/network_resources.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/fake_server/bookmark_entity_builder.h"
#include "components/sync/test/fake_server/fake_server.h"
#include "components/sync/test/fake_server/fake_server_network_resources.h"
#include "components/sync/test/fake_server/fake_server_verifier.h"
#include "jni/FakeServerHelper_jni.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::android::JavaParamRef;

FakeServerHelperAndroid::FakeServerHelperAndroid(JNIEnv* env, jobject obj) {}

FakeServerHelperAndroid::~FakeServerHelperAndroid() {}

static jlong JNI_FakeServerHelper_Init(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  FakeServerHelperAndroid* fake_server_android =
      new FakeServerHelperAndroid(env, obj);
  return reinterpret_cast<intptr_t>(fake_server_android);
}

jlong FakeServerHelperAndroid::CreateFakeServer(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  fake_server::FakeServer* fake_server = new fake_server::FakeServer();
  return reinterpret_cast<intptr_t>(fake_server);
}

jlong FakeServerHelperAndroid::CreateNetworkResources(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong fake_server) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  syncer::NetworkResources* resources =
      new fake_server::FakeServerNetworkResources(fake_server_ptr->AsWeakPtr());
  return reinterpret_cast<intptr_t>(resources);
}

void FakeServerHelperAndroid::DeleteFakeServer(JNIEnv* env,
                                               const JavaParamRef<jobject>& obj,
                                               jlong fake_server) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  delete fake_server_ptr;
}

jboolean FakeServerHelperAndroid::VerifyEntityCountByTypeAndName(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong fake_server,
    jlong count,
    jint model_type_int,
    const JavaParamRef<jstring>& name) {
  syncer::ModelType model_type = static_cast<syncer::ModelType>(model_type_int);
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  fake_server::FakeServerVerifier fake_server_verifier(fake_server_ptr);
  testing::AssertionResult result =
      fake_server_verifier.VerifyEntityCountByTypeAndName(
          count, model_type, base::android::ConvertJavaStringToUTF8(env, name));

  if (!result)
    LOG(WARNING) << result.message();

  return result;
}

jboolean FakeServerHelperAndroid::VerifySessions(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong fake_server,
    const JavaParamRef<jobjectArray>& url_array) {
  std::multiset<std::string> tab_urls;
  for (int i = 0; i < env->GetArrayLength(url_array); i++) {
    base::android::ScopedJavaLocalRef<jstring> j_string(
        env, static_cast<jstring>(env->GetObjectArrayElement(url_array, i)));
    tab_urls.insert(base::android::ConvertJavaStringToUTF8(env, j_string));
  }
  fake_server::SessionsHierarchy expected_sessions;
  expected_sessions.AddWindow(tab_urls);

  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  fake_server::FakeServerVerifier fake_server_verifier(fake_server_ptr);
  testing::AssertionResult result =
      fake_server_verifier.VerifySessions(expected_sessions);

  if (!result)
    LOG(WARNING) << result.message();

  return result;
}

base::android::ScopedJavaLocalRef<jobjectArray>
FakeServerHelperAndroid::GetSyncEntitiesByModelType(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong fake_server,
    jint model_type_int) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);

  syncer::ModelType model_type = static_cast<syncer::ModelType>(model_type_int);

  std::vector<sync_pb::SyncEntity> entities =
      fake_server_ptr->GetSyncEntitiesByModelType(model_type);

  std::vector<std::string> entity_strings;
  for (size_t i = 0; i < entities.size(); ++i) {
    std::string s;
    entities[i].SerializeToString(&s);
    entity_strings.push_back(s);
  }
  return base::android::ToJavaArrayOfByteArray(env, entity_strings);
}

void FakeServerHelperAndroid::InjectUniqueClientEntity(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong fake_server,
    const JavaParamRef<jstring>& name,
    const JavaParamRef<jbyteArray>& serialized_entity_specifics) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);

  sync_pb::EntitySpecifics entity_specifics;
  DeserializeEntitySpecifics(env, serialized_entity_specifics,
                             &entity_specifics);

  int64_t now = syncer::TimeToProtoTime(base::Time::Now());
  fake_server_ptr->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromEntitySpecifics(
          base::android::ConvertJavaStringToUTF8(env, name), entity_specifics,
          /*creation_time=*/now, /*last_modified_time=*/now));
}

void FakeServerHelperAndroid::SetWalletData(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jlong fake_server,
    const base::android::JavaParamRef<jbyteArray>& serialized_entity) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);

  sync_pb::SyncEntity entity;
  DeserializeEntity(env, serialized_entity, &entity);

  fake_server_ptr->SetWalletData({entity});
}

void FakeServerHelperAndroid::ModifyEntitySpecifics(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong fake_server,
    const JavaParamRef<jstring>& id,
    const JavaParamRef<jbyteArray>& serialized_entity_specifics) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);

  sync_pb::EntitySpecifics entity_specifics;
  DeserializeEntitySpecifics(env, serialized_entity_specifics,
                             &entity_specifics);

  fake_server_ptr->ModifyEntitySpecifics(
      base::android::ConvertJavaStringToUTF8(env, id), entity_specifics);
}

void FakeServerHelperAndroid::DeserializeEntity(JNIEnv* env,
                                                jbyteArray serialized_entity,
                                                sync_pb::SyncEntity* entity) {
  int bytes_length = env->GetArrayLength(serialized_entity);
  jbyte* bytes = env->GetByteArrayElements(serialized_entity, nullptr);
  std::string string(reinterpret_cast<char*>(bytes), bytes_length);

  if (!entity->ParseFromString(string))
    NOTREACHED() << "Could not deserialize Entity";
}

void FakeServerHelperAndroid::DeserializeEntitySpecifics(
    JNIEnv* env,
    const JavaParamRef<jbyteArray>& serialized_entity_specifics,
    sync_pb::EntitySpecifics* entity_specifics) {
  std::string specifics_string;
  base::android::JavaByteArrayToString(env, serialized_entity_specifics,
                                       &specifics_string);

  if (!entity_specifics->ParseFromString(specifics_string))
    NOTREACHED() << "Could not deserialize EntitySpecifics";
}

void FakeServerHelperAndroid::InjectBookmarkEntity(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong fake_server,
    const JavaParamRef<jstring>& title,
    const JavaParamRef<jstring>& url,
    const JavaParamRef<jstring>& parent_id) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  fake_server_ptr->InjectEntity(
      CreateBookmarkEntity(env, title, url, parent_id));
}

void FakeServerHelperAndroid::InjectBookmarkFolderEntity(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong fake_server,
    const JavaParamRef<jstring>& title,
    const JavaParamRef<jstring>& parent_id) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);

  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(
          base::android::ConvertJavaStringToUTF8(env, title));
  bookmark_builder.SetParentId(
      base::android::ConvertJavaStringToUTF8(env, parent_id));

  fake_server_ptr->InjectEntity(bookmark_builder.BuildFolder());
}

void FakeServerHelperAndroid::ModifyBookmarkEntity(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong fake_server,
    const JavaParamRef<jstring>& entity_id,
    const JavaParamRef<jstring>& title,
    const JavaParamRef<jstring>& url,
    const JavaParamRef<jstring>& parent_id) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  std::unique_ptr<syncer::LoopbackServerEntity> bookmark =
      CreateBookmarkEntity(env, title, url, parent_id);
  sync_pb::SyncEntity proto;
  bookmark->SerializeAsProto(&proto);
  fake_server_ptr->ModifyBookmarkEntity(
      base::android::ConvertJavaStringToUTF8(env, entity_id),
      base::android::ConvertJavaStringToUTF8(env, parent_id),
      proto.specifics());
}

void FakeServerHelperAndroid::ModifyBookmarkFolderEntity(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong fake_server,
    const JavaParamRef<jstring>& entity_id,
    const JavaParamRef<jstring>& title,
    const JavaParamRef<jstring>& parent_id) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);

  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(
          base::android::ConvertJavaStringToUTF8(env, title));
  bookmark_builder.SetParentId(
      base::android::ConvertJavaStringToUTF8(env, parent_id));

  sync_pb::SyncEntity proto;
  bookmark_builder.BuildFolder()->SerializeAsProto(&proto);
  fake_server_ptr->ModifyBookmarkEntity(
      base::android::ConvertJavaStringToUTF8(env, entity_id),
      base::android::ConvertJavaStringToUTF8(env, parent_id),
      proto.specifics());
}

std::unique_ptr<syncer::LoopbackServerEntity>
FakeServerHelperAndroid::CreateBookmarkEntity(JNIEnv* env,
                                              jstring title,
                                              jstring url,
                                              jstring parent_id) {
  std::string url_as_string = base::android::ConvertJavaStringToUTF8(env, url);
  GURL gurl = GURL(url_as_string);
  if (!gurl.is_valid()) {
    NOTREACHED() << "The given string (" << url_as_string
                 << ") is not a valid URL.";
  }

  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(
          base::android::ConvertJavaStringToUTF8(env, title));
  bookmark_builder.SetParentId(
      base::android::ConvertJavaStringToUTF8(env, parent_id));
  return bookmark_builder.BuildBookmark(gurl);
}

base::android::ScopedJavaLocalRef<jstring>
FakeServerHelperAndroid::GetBookmarkBarFolderId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong fake_server) {
  // Rather hard code this here then incur the cost of yet another method.
  // It is very unlikely that this will ever change.
  return base::android::ConvertUTF8ToJavaString(env, "32904_bookmark_bar");
}

void FakeServerHelperAndroid::DeleteEntity(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong fake_server,
    const JavaParamRef<jstring>& id,
    const base::android::JavaParamRef<jstring>& client_defined_unique_tag) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  std::string native_id = base::android::ConvertJavaStringToUTF8(env, id);
  fake_server_ptr->InjectEntity(syncer::PersistentTombstoneEntity::CreateNew(
      native_id,
      base::android::ConvertJavaStringToUTF8(env, client_defined_unique_tag)));
}

void FakeServerHelperAndroid::ClearServerData(JNIEnv* env,
                                              const JavaParamRef<jobject>& obj,
                                              jlong fake_server) {
  fake_server::FakeServer* fake_server_ptr =
      reinterpret_cast<fake_server::FakeServer*>(fake_server);
  fake_server_ptr->ClearServerData();
}
