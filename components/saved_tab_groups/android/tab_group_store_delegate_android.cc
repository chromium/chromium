// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/android/tab_group_store_delegate_android.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/jni_headers/TabGroupStoreDelegate_jni.h"
#include "components/saved_tab_groups/tab_group_store_delegate.h"
#include "components/saved_tab_groups/tab_group_store_id.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace tab_groups {

TabGroupStoreDelegateAndroid::TabGroupStoreDelegateAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(
      env, Java_TabGroupStoreDelegate_create(env, reinterpret_cast<jlong>(this))
               .obj());
}

TabGroupStoreDelegateAndroid::~TabGroupStoreDelegateAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TabGroupStoreDelegate_clearNativePtr(env, java_obj_);
}

void TabGroupStoreDelegateAndroid::GetAllTabGroupIDMetadatas(
    GetCallback callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::unique_ptr<GetCallback> wrapped_callback =
      std::make_unique<GetCallback>(std::move(callback));
  CHECK(wrapped_callback.get());
  jlong j_native_ptr = reinterpret_cast<jlong>(wrapped_callback.get());
  // We store this callback in-memory, expecting Java to always call us back
  // through JNI_TabGroupStoreDelegate_OnGetTabGroupIDMetadata
  callbacks_.emplace(j_native_ptr, std::move(wrapped_callback));

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&Java_TabGroupStoreDelegate_getTabGroupIDMetadatas, env,
                     java_obj_, j_native_ptr));
}

void TabGroupStoreDelegateAndroid::OnGetTabGroupIDMetadata(
    JNIEnv* env,
    jlong callback_ptr,
    const base::android::JavaParamRef<jobjectArray>& j_sync_guids,
    const base::android::JavaParamRef<jobjectArray>& j_serialized_tokens) {
  CHECK(callback_ptr);
  // We here assume the callback is still alive, and take ownership over it.
  auto it = callbacks_.find(callback_ptr);
  if (it == callbacks_.end()) {
    // Callback is not valid, and this is an invalid state.
    CHECK(false);
    return;
  }

  std::unique_ptr<GetCallback> callback = std::move(it->second);
  callbacks_.erase(callback_ptr);

  std::vector<std::string> sync_guid_strs;
  base::android::AppendJavaStringArrayToStringVector(env, j_sync_guids,
                                                     &sync_guid_strs);

  // Convert string GUIDs to Uuids.
  std::vector<base::Uuid> sync_guids;
  for (auto& sync_guid : sync_guid_strs) {
    sync_guids.emplace_back(base::Uuid::ParseCaseInsensitive(sync_guid));
  }

  std::vector<std::string> serialized_token_strs;
  base::android::AppendJavaStringArrayToStringVector(env, j_serialized_tokens,
                                                     &serialized_token_strs);
  // Parse string base::Tokens to base::Tokens.
  std::vector<std::optional<base::Token>> tokens;
  CHECK(sync_guids.size() == serialized_token_strs.size());
  for (const auto& serialized_token_str : serialized_token_strs) {
    std::optional<base::Token> maybe_token =
        base::Token::FromString(serialized_token_str);
    tokens.emplace_back(std::move(maybe_token));
  }

  // For all entries with valid base::Tokens, return them.
  CHECK(sync_guids.size() == tokens.size());
  std::map<base::Uuid, TabGroupIDMetadata> result = {};
  for (size_t i = 0; i < sync_guids.size(); ++i) {
    if (!tokens[i]) {
      // TODO(nyquist): Add cleanup task for deleting invalid data.
      continue;
    }
    result.emplace(sync_guids[i], std::move(tokens[i].value()));
  }

  std::move(*callback).Run(std::move(result));
}

void TabGroupStoreDelegateAndroid::StoreTabGroupIDMetadata(
    const base::Uuid& sync_guid,
    const TabGroupIDMetadata& tab_group_id_metadata) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::string serialized_token =
      tab_group_id_metadata.local_tab_group_id.ToString();
  Java_TabGroupStoreDelegate_storeTabGroupIDMetadata(
      env, java_obj_,
      base::android::ConvertUTF8ToJavaString(env,
                                             sync_guid.AsLowercaseString()),
      base::android::ConvertUTF8ToJavaString(env, serialized_token));
}

void TabGroupStoreDelegateAndroid::DeleteTabGroupIDMetdata(
    const base::Uuid& sync_guid) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TabGroupStoreDelegate_deleteTabGroupIDMetadata(
      env, java_obj_,
      base::android::ConvertUTF8ToJavaString(env,
                                             sync_guid.AsLowercaseString()));
}

}  // namespace tab_groups
