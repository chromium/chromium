// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/android/component_loader_policy.h"

#include <jni.h>
#include <stddef.h>
#include <stdio.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/android/component_loader_policy_forward.h"
#include "components/component_updater/android/components_info_holder.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crash/core/common/crash_key.h"
#include "components/metrics/component_metrics_provider.h"
#include "components/update_client/utils.h"
#include "third_party/metrics_proto/system_profile.pb.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/component_updater/android/embedded_component_loader_jni_headers/ComponentLoaderPolicyBridge_jni.h"

namespace component_updater {
namespace {

constexpr char kManifestFileName[] = "manifest.json";

// Size of the "crx-components" crash key in bytes. Each entry is of the form
// "COMPONENT_NAME-123.456.789," and the longest component name is 39 bytes so
// the maximum size of an entry is 52 bytes. Currently there are 5 components
// registered for WebView, 512 bytes should be able to hold about 10 entries.
constexpr size_t kComponentsKeySize = 512;

std::optional<base::Value::Dict> ReadManifest(
    const std::string& manifest_content) {
  JSONStringValueDeserializer deserializer(manifest_content);
  std::string error;
  std::unique_ptr<base::Value> root = deserializer.Deserialize(nullptr, &error);
  if (root && root->is_dict()) {
    return std::move(*root).TakeDict();
  }

  return std::nullopt;
}

std::optional<base::Value::Dict> ReadManifestFromFd(int fd) {
  std::string content;
  base::ScopedFILE file_stream(
      base::FileToFILE(base::File(std::move(fd)), "r"));
  return base::ReadStreamToString(file_stream.get(), &content)
             ? ReadManifest(content)
             : std::nullopt;
}

void RecordComponentLoadStatusHistogram(const std::string& suffix,
                                        ComponentLoadResult status) {
  DCHECK(!suffix.empty());
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"ComponentUpdater.AndroidComponentLoader.LoadStatus.", suffix}),
      status);
}

std::string ComponentToString(const ComponentInfo& component) {
  const auto id =
      metrics::ComponentMetricsProvider::CrxIdToComponentId(component.id);
  if (id == metrics::SystemProfileProto_ComponentId_UNKNOWN) {
    return std::string();
  }
  return base::StringPrintf("%s-%s",
                            SystemProfileProto_ComponentId_Name(id).c_str(),
                            component.version.GetString().c_str());
}

void UpdateCrashKeys() {
  std::vector<std::string> components_crash_key_values;
  for (const ComponentInfo& component :
       ComponentsInfoHolder::GetInstance()->GetComponents()) {
    components_crash_key_values.push_back(ComponentToString(component));
  }

  static ::crash_reporter::CrashKeyString<kComponentsKeySize>
      components_crash_key("crx-components");
  components_crash_key.Set(base::JoinString(components_crash_key_values, ","));
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
  base::flat_map<std::string, base::ScopedFD> fd_map;
  int manifest_fd = -1;
  for (size_t i = 0; i < file_names.size(); ++i) {
    const std::string& file_name = file_names[i];
    if (file_name == kManifestFileName) {
      manifest_fd = fds[i];
    } else {
      fd_map[file_name] = base::ScopedFD(fds[i]);
    }
  }

  if (manifest_fd == -1) {
    ComponentLoadFailedInternal(ComponentLoadResult::kMissingManifest);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(ReadManifestFromFd, manifest_fd),
      base::BindOnce(&AndroidComponentLoaderPolicy::NotifyNewVersion,
                     base::Owned(this), base::OwnedRef(std::move(fd_map))));
}

void AndroidComponentLoaderPolicy::ComponentLoadFailed(JNIEnv* env,
                                                       jint error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(error_code > static_cast<int>(ComponentLoadResult::kComponentLoaded));
  DCHECK(error_code <= static_cast<int>(ComponentLoadResult::kMaxValue));
  ComponentLoadFailedInternal(static_cast<ComponentLoadResult>(error_code));
  delete this;
}

std::string AndroidComponentLoaderPolicy::GetComponentId() const {
  std::vector<uint8_t> hash;
  loader_policy_->GetHash(&hash);
  return update_client::GetCrxIdFromPublicKeyHash(hash);
}

base::android::ScopedJavaLocalRef<jstring>
AndroidComponentLoaderPolicy::GetComponentId(JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(env, GetComponentId());
}

void AndroidComponentLoaderPolicy::NotifyNewVersion(
    base::flat_map<std::string, base::ScopedFD>& fd_map,
    std::optional<base::Value::Dict> manifest) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!manifest) {
    ComponentLoadFailedInternal(ComponentLoadResult::kMalformedManifest);
    return;
  }
  std::string version_ascii;
  if (const std::string* ptr = manifest->FindString("version")) {
    if (base::IsStringASCII(*ptr)) {
      version_ascii = *ptr;
    }
  }
  base::Version version(version_ascii);
  if (!version.IsValid()) {
    ComponentLoadFailedInternal(ComponentLoadResult::kInvalidVersion);
    return;
  }

  RecordComponentLoadStatusHistogram(loader_policy_->GetMetricsSuffix(),
                                     ComponentLoadResult::kComponentLoaded);
  ComponentsInfoHolder::GetInstance()->AddComponent(GetComponentId(), version);
  loader_policy_->ComponentLoaded(version, fd_map, std::move(*manifest));
  UpdateCrashKeys();
}

void AndroidComponentLoaderPolicy::ComponentLoadFailedInternal(
    ComponentLoadResult error) {
  RecordComponentLoadStatusHistogram(loader_policy_->GetMetricsSuffix(), error);
  loader_policy_->ComponentLoadFailed(error);
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
