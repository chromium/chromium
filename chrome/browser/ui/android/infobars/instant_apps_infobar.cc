// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrome/browser/ui/android/infobars/instant_apps_infobar.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/android/chrome_jni_headers/InstantAppsInfoBar_jni.h"
#include "chrome/browser/android/instantapps/instant_apps_infobar_delegate.h"

InstantAppsInfoBar::InstantAppsInfoBar(
    std::unique_ptr<InstantAppsInfoBarDelegate> delegate)
    : ConfirmInfoBar(std::move(delegate)) {}

InstantAppsInfoBar::~InstantAppsInfoBar() {}

base::android::ScopedJavaLocalRef<jobject>
InstantAppsInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  InstantAppsInfoBarDelegate* delegate =
      static_cast<InstantAppsInfoBarDelegate*>(GetDelegate());
  base::android::ScopedJavaLocalRef<jobject> infobar;
  infobar.Reset(Java_InstantAppsInfoBar_create(env, delegate->data()));

  java_infobar_.Reset(env, infobar.obj());
  return infobar;
}
