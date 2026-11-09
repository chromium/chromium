// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_DISTILLER_CORE_WEB_DISTILLER_SERVICE_ANDROID_H_
#define COMPONENTS_WEB_DISTILLER_CORE_WEB_DISTILLER_SERVICE_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/web_distiller/core/web_distiller_service.h"

namespace reader_mode::android {
class ReaderModeServiceFactoryAndroid;
}  // namespace reader_mode::android

namespace web_distiller::android {

// Native implementation of WebDistillerService,
// provides access to Java DistilledPagePrefs.
class WebDistillerServiceAndroid {
 public:
  WebDistillerServiceAndroid(WebDistillerService* service);
  virtual ~WebDistillerServiceAndroid();
  // Returns native pointer to native DistilledPagePrefs registered with
  // WebDistillerService.
  int64_t GetDistilledPagePrefsPtr(JNIEnv* env);

 private:
  // Friend class so that ReaderModeServiceFactoryAndroid has access to
  // private member object java_ref_.
  friend class ::reader_mode::android::ReaderModeServiceFactoryAndroid;
  // Points to a Java instance of WebDistillerService.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
  raw_ptr<WebDistillerService> service_;
};

}  // namespace web_distiller::android

#endif  // COMPONENTS_WEB_DISTILLER_CORE_WEB_DISTILLER_SERVICE_ANDROID_H_
