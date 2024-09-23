// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/infobar_container_android.h"

#include "base/android/jni_android.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/android/resource_mapper.h"
#include "components/infobars/android/infobar_android.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/InfoBarContainer_jni.h"

using base::android::JavaParamRef;

// InfoBarContainerAndroid ----------------------------------------------------

InfoBarContainerAndroid::InfoBarContainerAndroid(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj)
    : infobars::InfoBarContainer(NULL),
      weak_java_infobar_container_(env, obj) {}

InfoBarContainerAndroid::~InfoBarContainerAndroid() {
  RemoveAllInfoBarsForDestruction();
}

void InfoBarContainerAndroid::SetWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& web_contents) {
  infobars::ContentInfoBarManager* infobar_manager =
      web_contents
          ? infobars::ContentInfoBarManager::FromWebContents(
                content::WebContents::FromJavaWebContents(web_contents))
          : nullptr;
  ChangeInfoBarManager(infobar_manager);
}

void InfoBarContainerAndroid::Destroy(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
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

  if (android_bar->HasSetJavaInfoBar())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();

  if (Java_InfoBarContainer_hasInfoBars(
          env, weak_java_infobar_container_.get(env))) {
    base::UmaHistogramSparse("InfoBar.Shown.Hidden",
                             android_bar->delegate()->GetIdentifier());
    infobars::InfoBarDelegate::InfoBarIdentifier identifier =
        static_cast<infobars::InfoBarDelegate::InfoBarIdentifier>(
            Java_InfoBarContainer_getTopInfoBarIdentifier(
                env, weak_java_infobar_container_.get(env)));
    if (identifier != infobars::InfoBarDelegate::InfoBarIdentifier::INVALID) {
      base::UmaHistogramSparse("InfoBar.Shown.Hiding", identifier);
    }
  } else {
    base::UmaHistogramSparse("InfoBar.Shown.Visible",
                             android_bar->delegate()->GetIdentifier());
  }

  base::android::ScopedJavaLocalRef<jobject> java_infobar =
      android_bar->CreateRenderInfoBar(
          env, base::BindRepeating(&ResourceMapper::MapToJavaDrawableId));
  android_bar->SetJavaInfoBar(java_infobar);
  Java_InfoBarContainer_addInfoBar(env, weak_java_infobar_container_.get(env),
                                   java_infobar);
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


// Native JNI methods ---------------------------------------------------------

static jlong JNI_InfoBarContainer_Init(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  InfoBarContainerAndroid* infobar_container =
      new InfoBarContainerAndroid(env, obj);
  return reinterpret_cast<intptr_t>(infobar_container);
}
