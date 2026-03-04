// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/infobar_container_android.h"

#include "base/android/jni_android.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/android/tab_android.h"
#include "components/infobars/android/infobar_android.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/InfoBarContainer_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

// InfoBarContainerAndroid ----------------------------------------------------

InfoBarContainerAndroid::InfoBarContainerAndroid(TabAndroid* tab)
    : infobars::InfoBarContainer(nullptr), tab_(tab) {}

InfoBarContainerAndroid::~InfoBarContainerAndroid() {
  RemoveAllInfoBarsForDestruction();
}

void InfoBarContainerAndroid::SetWebContents(
    JNIEnv* env,
    content::WebContents* web_contents) {
  infobars::ContentInfoBarManager* infobar_manager =
      web_contents
          ? infobars::ContentInfoBarManager::FromWebContents(web_contents)
          : nullptr;
  ChangeInfoBarManager(infobar_manager);
}

void InfoBarContainerAndroid::Destroy(JNIEnv* env) {
  delete this;
}

// Creates the Java equivalent of |android_bar| and add it to the java
// container.
void InfoBarContainerAndroid::PlatformSpecificAddInfoBar(
    infobars::InfoBar* infobar,
    size_t position) {
  DCHECK(infobar);
  infobars::InfoBarAndroid* android_bar =
      static_cast<infobars::InfoBarAndroid*>(infobar);

  if (android_bar->HasSetJavaInfoBar()) {
    return;
  }
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> java_container = GetJavaObject(env);
  if (Java_InfoBarContainer_hasInfoBars(env, java_container)) {
    base::UmaHistogramSparse("InfoBar.Shown.Hidden",
                             android_bar->delegate()->GetIdentifier());
    infobars::InfoBarDelegate::InfoBarIdentifier identifier =
        static_cast<infobars::InfoBarDelegate::InfoBarIdentifier>(
            Java_InfoBarContainer_getTopInfoBarIdentifier(env, java_container));
    if (identifier != infobars::InfoBarDelegate::InfoBarIdentifier::INVALID) {
      base::UmaHistogramSparse("InfoBar.Shown.Hiding", identifier);
    }
  } else {
    base::UmaHistogramSparse("InfoBar.Shown.Visible",
                             android_bar->delegate()->GetIdentifier());
  }

  ScopedJavaLocalRef<jobject> java_infobar = android_bar->CreateRenderInfoBar(
      env, base::BindRepeating(&ResourceMapper::MapToJavaDrawableId));
  android_bar->SetJavaInfoBar(java_infobar);
  Java_InfoBarContainer_addInfoBar(env, java_container, java_infobar);
}

void InfoBarContainerAndroid::PlatformSpecificReplaceInfoBar(
    infobars::InfoBar* old_infobar,
    infobars::InfoBar* new_infobar) {
  static_cast<infobars::InfoBarAndroid*>(new_infobar)
      ->PassJavaInfoBar(static_cast<infobars::InfoBarAndroid*>(old_infobar));
}

void InfoBarContainerAndroid::PlatformSpecificRemoveInfoBar(
    infobars::InfoBar* infobar) {
  infobars::InfoBarAndroid* android_infobar =
      static_cast<infobars::InfoBarAndroid*>(infobar);
  android_infobar->CloseJavaInfoBar();
}

ScopedJavaLocalRef<jobject> InfoBarContainerAndroid::GetJavaObject(
    JNIEnv* env) {
  ScopedJavaLocalRef<jobject> java_container =
      Java_InfoBarContainer_getFromTab(env, tab_.get());
  CHECK(java_container);
  return java_container;
}

// Native JNI methods ---------------------------------------------------------

static int64_t JNI_InfoBarContainer_Init(JNIEnv* env, TabAndroid* tab) {
  InfoBarContainerAndroid* infobar_container = new InfoBarContainerAndroid(tab);
  return reinterpret_cast<intptr_t>(infobar_container);
}

DEFINE_JNI(InfoBarContainer)
