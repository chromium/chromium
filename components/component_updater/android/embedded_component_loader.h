// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_ANDROID_EMBEDDED_COMPONENT_LOADER_H_
#define COMPONENTS_COMPONENT_UPDATER_ANDROID_EMBEDDED_COMPONENT_LOADER_H_

#include <jni.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"

namespace base {
class Version;
class DictionaryValue;
}  // namespace base

namespace component_updater {

// Components should use `EmbeddedComponentLoader` by defining a class that
// implements the members of `ComponentLoaderPolicy`, and then registering a
// `EmbeddedComponentLoader` that has been constructed with an instance of that
// class in an instance of embedded WebView or WebLayer with the Java
// EmbeddedComponentLoader. The `EmbeddedComponentLoader` will fetch the
// components files from the Android `ComponentsProviderService` and invoke the
// callbacks defined in this class.
//
// Ideally, the implementation of this class should share implementation with
// its component `ComponentInstallerPolicy` counterpart.
class ComponentLoaderPolicy {
 public:
  virtual ~ComponentLoaderPolicy();

  // ComponentLoaded is called when the loader successfully gets file
  // descriptors for all files in the component from the
  // ComponentsProviderService.
  //
  // Must close all file descriptors after using them. Can be called multiple
  // times in the same run.
  //
  // `version` is the version of the component.
  // `fd_map` maps file relative paths in the install directory to its file
  //          descriptor.
  // `manifest` is the manifest for this version of the component.
  virtual void ComponentLoaded(
      const base::Version& version,
      const base::flat_map<std::string, int>& fd_map,
      std::unique_ptr<base::DictionaryValue> manifest) = 0;

  // Called if connection to the service fails, components files are not found
  // or if the manifest file is missing or invalid. Can
  // be called multiple times in the same run.
  //
  // TODO(crbug.com/1180966) accept error code for different types of errors.
  virtual void ComponentLoadFailed() = 0;

  // Returns the component's SHA2 hash as raw bytes, the hash value is used as
  // the unique id of the component and will be used to request components files
  // from the ComponentsProviderService.
  virtual void GetHash(std::vector<uint8_t>* hash) const = 0;
};

// Provides a bridge from Java to native to receive callbacks from the Java
// loader and pass it to the wrapped ComponentLoaderPolicy instance.
//
// Must only be created and used on the same thread.
class EmbeddedComponentLoader {
 public:
  explicit EmbeddedComponentLoader(
      std::unique_ptr<ComponentLoaderPolicy> loader_policy);
  ~EmbeddedComponentLoader();

  // JNI overrides:
  void ComponentLoaded(JNIEnv* env,
                       const base::android::JavaRef<jobjectArray>& jfile_names,
                       const base::android::JavaRef<jintArray>& jfds);
  void ComponentLoadFailed(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jstring> GetComponentId(JNIEnv* env);

  void NotifyNewVersion(const base::flat_map<std::string, int>& fd_map,
                        std::unique_ptr<base::DictionaryValue> manifest);

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<ComponentLoaderPolicy> loader_policy_;
  base::WeakPtrFactory<EmbeddedComponentLoader> weak_ptr_factory_{this};
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_ANDROID_EMBEDDED_COMPONENT_LOADER_H_