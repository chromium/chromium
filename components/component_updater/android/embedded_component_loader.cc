// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/android/embedded_component_loader.h"

#include <jni.h>
#include <stdio.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
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

EmbeddedComponentLoader::EmbeddedComponentLoader(
    std::unique_ptr<ComponentLoaderPolicy> loader_policy)
    : loader_policy_(std::move(loader_policy)) {}

EmbeddedComponentLoader::~EmbeddedComponentLoader() = default;

void EmbeddedComponentLoader::ComponentLoaded(
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
    loader_policy_->ComponentLoadFailed();
    return;
  }

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(ReadManifestFromFd, manifest_fd),
      base::BindOnce(&EmbeddedComponentLoader::NotifyNewVersion,
                     weak_ptr_factory_.GetWeakPtr(), fd_map));
}

void EmbeddedComponentLoader::ComponentLoadFailed(JNIEnv* env) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  loader_policy_->ComponentLoadFailed();
}

base::android::ScopedJavaLocalRef<jstring>
EmbeddedComponentLoader::GetComponentId(JNIEnv* env) {
  std::vector<uint8_t> hash;
  loader_policy_->GetHash(&hash);
  return base::android::ConvertUTF8ToJavaString(
      env, update_client::GetCrxIdFromPublicKeyHash(hash));
}

void EmbeddedComponentLoader::NotifyNewVersion(
    const base::flat_map<std::string, int>& fd_map,
    std::unique_ptr<base::DictionaryValue> manifest) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!manifest) {
    loader_policy_->ComponentLoadFailed();
    return;
  }
  std::string version_ascii;
  manifest->GetStringASCII("version", &version_ascii);
  base::Version version(version_ascii);
  if (!version.IsValid()) {
    loader_policy_->ComponentLoadFailed();
    return;
  }
  loader_policy_->ComponentLoaded(version, fd_map, std::move(manifest));
}

}  // namespace component_updater
