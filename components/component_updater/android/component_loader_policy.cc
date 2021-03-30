// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/android/component_loader_policy.h"

#include <jni.h>
#include <stdio.h>
#include <unistd.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/android/component_loader_policy_forward.h"
#include "components/component_updater/android/embedded_component_loader_jni_headers/ComponentLoaderPolicyBridge_jni.h"
#include "components/update_client/utils.h"

namespace component_updater {
namespace {

constexpr char kManifestFileName[] = "manifest.json";

// TODO(crbug.com/1180964) move to base/file_util.h
bool ReadFdToString(int fd, std::string* contents) {
  base::ScopedFILE file_stream(fdopen(fd, "r"));
  return file_stream.get()
             ? base::ReadStreamToString(file_stream.get(), contents)
             : false;
}

std::unique_ptr<base::DictionaryValue> ReadManifest(
    const std::string& manifest_content) {
  JSONStringValueDeserializer deserializer(manifest_content);
  std::string error;
  std::unique_ptr<base::Value> root = deserializer.Deserialize(nullptr, &error);
  return (root && root->is_dict())
             ? std::unique_ptr<base::DictionaryValue>(
                   static_cast<base::DictionaryValue*>(root.release()))
             : nullptr;
}

std::unique_ptr<base::DictionaryValue> ReadManifestFromFd(int fd) {
  std::string content;
  if (!ReadFdToString(fd, &content)) {
    return nullptr;
  }
  return ReadManifest(content);
}

}  // namespace

ComponentLoaderPolicy::~ComponentLoaderPolicy() = default;

AndroidComponentLoaderPolicy::AndroidComponentLoaderPolicy(
    std::unique_ptr<ComponentLoaderPolicy> loader_policy)
    : loader_policy_(std::move(loader_policy)) {
  JNIEnv* env = base::android::AttachCurrentThread();
  obj_.Reset(env, Java_ComponentLoaderPolicyBridge_Constructor(
                      env, reinterpret_cast<intptr_t>(this))
                      .obj());
}

AndroidComponentLoaderPolicy::~AndroidComponentLoaderPolicy() = default;

base::android::ScopedJavaLocalRef<jobject>
AndroidComponentLoaderPolicy::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(obj_);
}

void AndroidComponentLoaderPolicy::ComponentLoaded(
    JNIEnv* env,
    const base::android::JavaRef<jobjectArray>& jfile_names,
    const base::android::JavaRef<jintArray>& jfds) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::string> file_names;
  std::vector<int> fds;
  base::android::AppendJavaStringArrayToStringVector(env, jfile_names,
                                                     &file_names);
  base::android::JavaIntArrayToIntVector(env, jfds, &fds);
  DCHECK_EQ(file_names.size(), fds.size());

  // Construct the file_name->file_descriptor map excluding the manifest file
  // as it's parsed and passed separately.
  base::flat_map<std::string, int> fd_map;
  int manifest_fd = -1;
  for (size_t i = 0; i < file_names.size(); ++i) {
    const std::string& file_name = file_names[i];
    if (file_name == kManifestFileName) {
      manifest_fd = fds[i];
    } else {
      fd_map[file_name] = fds[i];
    }
  }
  if (manifest_fd == -1) {
    CloseFdsAndFail(fd_map);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(ReadManifestFromFd, manifest_fd),
      base::BindOnce(&AndroidComponentLoaderPolicy::NotifyNewVersion,
                     base::Owned(this), fd_map));
}

void AndroidComponentLoaderPolicy::ComponentLoadFailed(JNIEnv* env) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  loader_policy_->ComponentLoadFailed();
  delete this;
}

base::android::ScopedJavaLocalRef<jstring>
AndroidComponentLoaderPolicy::GetComponentId(JNIEnv* env) {
  std::vector<uint8_t> hash;
  loader_policy_->GetHash(&hash);
  return base::android::ConvertUTF8ToJavaString(
      env, update_client::GetCrxIdFromPublicKeyHash(hash));
}

void AndroidComponentLoaderPolicy::NotifyNewVersion(
    const base::flat_map<std::string, int>& fd_map,
    std::unique_ptr<base::DictionaryValue> manifest) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!manifest) {
    CloseFdsAndFail(fd_map);
    return;
  }
  std::string version_ascii;
  manifest->GetStringASCII("version", &version_ascii);
  base::Version version(version_ascii);
  if (!version.IsValid()) {
    CloseFdsAndFail(fd_map);
    return;
  }
  loader_policy_->ComponentLoaded(version, fd_map, std::move(manifest));
}

void AndroidComponentLoaderPolicy::CloseFdsAndFail(
    const base::flat_map<std::string, int>& fd_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& iter : fd_map) {
    close(iter.second);
  }
  loader_policy_->ComponentLoadFailed();
}

// static
base::android::ScopedJavaLocalRef<jobjectArray>
AndroidComponentLoaderPolicy::ToJavaArrayOfAndroidComponentLoaderPolicy(
    JNIEnv* env,
    ComponentLoaderPolicyVector policies) {
  base::android::ScopedJavaLocalRef<jobjectArray> policy_array =
      Java_ComponentLoaderPolicyBridge_createNewArray(env, policies.size());

  for (size_t i = 0; i < policies.size(); ++i) {
    // The `AndroidComponentLoaderPolicy` object is owned by the java
    // ComponentLoaderPolicy object which manages its life time and will triger
    // deletion.
    auto* android_policy =
        new AndroidComponentLoaderPolicy(std::move(policies[i]));
    Java_ComponentLoaderPolicyBridge_setArrayElement(
        env, policy_array, i, android_policy->GetJavaObject());
  }
  return policy_array;
}

}  // namespace component_updater
