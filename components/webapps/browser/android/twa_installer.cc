// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/twa_installer.h"

#include <utility>

#include "base/android/jni_string.h"
#include "base/functional/callback.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/webapps/browser/android/webapps_jni_headers/TwaInstaller_jni.h"

using base::android::JavaRef;

namespace webapps {

// static
bool TwaInstaller::Install(std::unique_ptr<AddToHomescreenParams> params,
                           const AddToHomescreenEventCallback& event_callback) {
  // This class will be destroyed by its Java counterpart.
  auto* installer = new TwaInstaller(std::move(params), event_callback);
  return installer->Start();
}

TwaInstaller::TwaInstaller(std::unique_ptr<AddToHomescreenParams> params,
                           AddToHomescreenEventCallback event_callback) {
  params_ = std::move(params);
  event_callback_ = std::move(event_callback);
}

TwaInstaller::~TwaInstaller() = default;

bool TwaInstaller::Start() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_TwaInstaller_start(env, reinterpret_cast<intptr_t>(this),
                                 params_->shortcut_info->short_name,
                                 params_->shortcut_info->manifest_url.spec());
}

void TwaInstaller::OnInstallEvent(JNIEnv* env, int event) {
  event_callback_.Run(static_cast<AddToHomescreenEvent>(event), *params_);
}

void TwaInstaller::Destroy(JNIEnv* env) {
  delete this;
}

}  // namespace webapps

DEFINE_JNI(TwaInstaller)
