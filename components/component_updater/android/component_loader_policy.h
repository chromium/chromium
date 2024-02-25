// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_ANDROID_COMPONENT_LOADER_POLICY_H_
#define COMPONENTS_COMPONENT_UPDATER_ANDROID_COMPONENT_LOADER_POLICY_H_

#include <jni.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "components/component_updater/android/component_loader_policy_forward.h"

namespace base {
class Version;
}  // namespace base

namespace component_updater {

// Errors that cause failure when loading a component. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.component_updater
enum class ComponentLoadResult {
  kComponentLoaded = 0,
  kFailedToConnectToComponentsProviderService = 1,
  kRemoteException = 2,
  kComponentsProviderServiceError = 3,
  kMissingManifest = 4,
  kMalformedManifest = 5,
  kInvalidVersion = 6,
  kMaxValue = kInvalidVersion,
};

// Components should use `AndroidComponentLoaderPolicy` by defining a class that
// implements the members of `ComponentLoaderPolicy`, and then registering a
// `AndroidComponentLoaderPolicy` that has been constructed with an instance of
// that class in an instance of embedded WebView or WebLayer with the Java
// AndroidComponentLoaderPolicy. The `AndroidComponentLoaderPolicy` will fetch
// the components files from the Android `ComponentsProviderService` and invoke
// the callbacks defined in this class.
//
// Ideally, the implementation of this class should share implementation with
// its component `ComponentInstallerPolicy` counterpart.
//
// Used on the UI thread, should post any non-user-visible tasks to a background
// runner.
class ComponentLoaderPolicy {
 public:
  virtual ~ComponentLoaderPolicy();

  // ComponentLoaded is called when the loader successfully gets file
  // descriptors for all files in the component from the
  // ComponentsProviderService.
  //
  // Will be called at most once. This is mutually exclusive with
  // ComponentLoadFailed; if this is called then ComponentLoadFailed won't be
  // called.
  //
  // Overriders must close all file descriptors after using them.
  //
  // `version` is the version of the component.
  // `fd_map` maps file relative paths in the install directory to its file
  //          descriptor.
  // `manifest` is the manifest for this version of the component.
  virtual void ComponentLoaded(
      const base::Version& version,
      base::flat_map<std::string, base::ScopedFD>& fd_map,
      base::Value::Dict manifest) = 0;

  // Called if connection to the service fails, components files are not found
  // or if the manifest file is missing or invalid.
  //
  // Will be called at most once. This is mutually exclusive with
  // ComponentLoaded; if this is called then ComponentLoaded won't be called.
  virtual void ComponentLoadFailed(ComponentLoadResult error) = 0;

  // Returns the component's SHA2 hash as raw bytes, the hash value is used as
  // the unique id of the component and will be used to request components files
  // from the ComponentsProviderService.
  virtual void GetHash(std::vector<uint8_t>* hash) const = 0;

  // Returns a Human readable string that can be used as a suffix for recorded
  // UMA metrics. New suffixes should be added to
  // "ComponentUpdater.AndroidComponentLoader.ComponentName" in
  // tools/metrics/histograms/metadata/histogram_suffixes_list.xml.
  virtual std::string GetMetricsSuffix() const = 0;
};

// Provides a bridge from Java to native to receive callbacks from the Java
// loader and pass it to the wrapped ComponentLoaderPolicy instance.
//
// The object is single use only, it will be deleted when ComponentLoaded or
// ComponentLoadedFailed is called once.
//
// Called on the UI thread, should post any non-user-visible tasks to a
// background runner.
class AndroidComponentLoaderPolicy {
 public:
  explicit AndroidComponentLoaderPolicy(
      std::unique_ptr<ComponentLoaderPolicy> loader_policy);
  ~AndroidComponentLoaderPolicy();

  AndroidComponentLoaderPolicy(const AndroidComponentLoaderPolicy&) = delete;
  AndroidComponentLoaderPolicy& operator=(const AndroidComponentLoaderPolicy&) =
      delete;

  // A utility method that returns an array of Java objects of
  // `org.chromium.components.component_updater.ComponentLoaderPolicy`.
  static base::android::ScopedJavaLocalRef<jobjectArray>
  ToJavaArrayOfAndroidComponentLoaderPolicy(
      JNIEnv* env,
      ComponentLoaderPolicyVector policies);

  // JNI overrides:
  void ComponentLoaded(JNIEnv* env,
                       const base::android::JavaRef<jobjectArray>& jfile_names,
                       const base::android::JavaRef<jintArray>& jfds);
  void ComponentLoadFailed(JNIEnv* env, jint error_code);
  base::android::ScopedJavaLocalRef<jstring> GetComponentId(JNIEnv* env);

 private:
  // Returns a Java object of
  // `org.chromium.components.component_updater.ComponentLoaderPolicy`.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  std::string GetComponentId() const;

  void NotifyNewVersion(base::flat_map<std::string, base::ScopedFD>& fd_map,
                        std::optional<base::Value::Dict> manifest);

  void ComponentLoadFailedInternal(ComponentLoadResult error);

  SEQUENCE_CHECKER(sequence_checker_);

  // A Java object of
  // `org.chromium.components.component_updater.ComponentLoaderPolicy`.
  base::android::ScopedJavaGlobalRef<jobject> obj_;

  std::unique_ptr<ComponentLoaderPolicy> loader_policy_;
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_ANDROID_COMPONENT_LOADER_POLICY_H_
