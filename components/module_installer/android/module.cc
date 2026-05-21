// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/bundle_utils.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/threading/thread_restrictions.h"
#include "ui/base/resource/resource_bundle_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/module_installer/android/jni_headers/Module_jni.h"

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

void LoadResources(const std::string& pak, const std::string& name) {
  module_installer::ScopedAllowModulePakLoad scoped_allow_module_pak_load;
  ui::LoadPackFileFromApk("assets/" + pak, name);
}

}  // namespace

static void JNI_Module_LoadNative(JNIEnv* env,
                                  const std::string& name,
                                  const std::vector<std::string>& paks) {
  for (const auto& pak : paks) {
    LoadResources(pak, name);
  }
}

}  // namespace module_installer

DEFINE_JNI(Module)
