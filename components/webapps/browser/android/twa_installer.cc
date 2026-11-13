// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/twa_installer.h"

#include <memory>
#include <string>
#include <utility>

#include "base/android/jni_string.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "content/public/browser/web_contents.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/webapps/browser/android/webapps_jni_headers/TwaInstaller_jni.h"

using base::android::JavaRef;

namespace webapps {

// static
bool TwaInstaller::Install(std::unique_ptr<AddToHomescreenParams> params,
                           const AddToHomescreenEventCallback& event_callback) {
  std::u16string title = params->shortcut_info->short_name;
  std::string manifest_url = params->shortcut_info->manifest_url.spec();
  auto installer =
      base::WrapUnique(new TwaInstaller(std::move(params), event_callback));
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_TwaInstaller_start(
      env, jni_zero::MakeUnique(std::move(installer)), title, manifest_url);
}

TwaInstaller::TwaInstaller(std::unique_ptr<AddToHomescreenParams> params,
                           AddToHomescreenEventCallback event_callback) {
  params_ = std::move(params);
  event_callback_ = std::move(event_callback);
}

TwaInstaller::~TwaInstaller() = default;

void TwaInstaller::OnInstallEvent(JNIEnv* env, int event) {
  event_callback_.Run(static_cast<AddToHomescreenEvent>(event), *params_);
}

}  // namespace webapps

DEFINE_JNI(TwaInstaller)
