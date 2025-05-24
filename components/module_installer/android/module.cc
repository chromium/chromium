// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <string>

#include "base/android/bundle_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "ui/base/resource/resource_bundle_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/module_installer/android/jni_headers/Module_jni.h"

using base::android::BundleUtils;

namespace module_installer {

// Allows memory mapping module PAK files on the main thread.
//
// We expect the memory mapping step to be quick. All it does is retrieve a
// region from the APK file that should already be memory mapped and read the
// PAK file header. Most of the disk IO is happening when accessing a resource.
// And this traditionally happens synchronously on the main thread.
//
// This class needs to be a friend of base::ScopedAllowBlocking and so cannot be
// in the unnamed namespace.
class ScopedAllowModulePakLoad {
 public:
  ScopedAllowModulePakLoad() = default;
  ~ScopedAllowModulePakLoad() = default;

 private:
  base::ScopedAllowBlocking allow_blocking_;
};

namespace {

typedef bool JniRegistrationFunction(JNIEnv* env);

void* LoadLibrary(const std::string& library_name,
                  const std::string& module_name) {
  void* library_handle = nullptr;

#if defined(LOAD_FROM_PARTITIONS)
  // The partition library must be opened via native code (using
  // android_dlopen_ext() under the hood). There is no need to repeat the
  // operation on the Java side, because JNI registration is done explicitly
  // (hence there is no reason for the Java ClassLoader to be aware of the
  // library, for lazy JNI registration).
  const std::string partition_name = module_name + "_partition";
  library_handle = BundleUtils::DlOpenModuleLibraryPartition(
      library_name, partition_name, module_name);
#elif defined(COMPONENT_BUILD)
  std::string library_path =
      BundleUtils::ResolveLibraryPath(library_name, module_name);
  library_handle = dlopen(library_path.c_str(), RTLD_LOCAL);
#else
#error "Unsupported configuration."
#endif  // defined(COMPONENT_BUILD)
  CHECK(library_handle != nullptr)
      << "Could not open feature library " << library_name << ": " << dlerror();

  return library_handle;
}

void RegisterJni(JNIEnv* env, void* library_handle, const std::string& name) {
  const std::string registration_name = "JNI_OnLoad_" + name;
  // Find the module's JNI registration method from the feature library.
  void* symbol = dlsym(library_handle, registration_name.c_str());
  CHECK(symbol) << "Could not find JNI registration method '"
                << registration_name << "' for '" << name << "': " << dlerror();
  auto* registration_function =
      reinterpret_cast<JniRegistrationFunction*>(symbol);
  CHECK(registration_function(env)) << "JNI registration failed: " << name;
}

void LoadResources(const std::string& pak, const std::string& name) {
  module_installer::ScopedAllowModulePakLoad scoped_allow_module_pak_load;
  ui::LoadPackFileFromApk("assets/" + pak, name);
}

}  // namespace

static void JNI_Module_LoadNative(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jname,
    const base::android::JavaParamRef<jobjectArray>& jlibraries,
    const base::android::JavaParamRef<jobjectArray>& jpaks) {
  std::string name;
  base::android::ConvertJavaStringToUTF8(env, jname, &name);
  std::vector<std::string> libraries;
  base::android::AppendJavaStringArrayToStringVector(env, jlibraries,
                                                     &libraries);
  if (libraries.size() > 0) {
    void* library_handle = nullptr;
    for (const auto& library : libraries) {
      library_handle = LoadLibrary(library, name);
    }
    // module libraries are ordered such that the root library will be the last
    // item in the list. We expect this library to provide the JNI registration
    // function.
    RegisterJni(env, library_handle, name);
  }
  std::vector<std::string> paks;
  base::android::AppendJavaStringArrayToStringVector(env, jpaks, &paks);
  for (const auto& pak : paks) {
    LoadResources(pak, name);
  }
}

}  // namespace module_installer
