// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_SERVICE_ANDROID_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_SERVICE_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "components/dom_distiller/core/dom_distiller_service.h"

namespace dom_distiller {
namespace android {

class DomDistillerServiceFactoryAndroid;

// Native implementation of DomDistillerService,
// provides access to Java DistilledPagePrefs.
class DomDistillerServiceAndroid {
 public:
  DomDistillerServiceAndroid(DomDistillerService* service);
  virtual ~DomDistillerServiceAndroid();
  // Returns native pointer to native DistilledPagePrefs registered with
  // DomDistillerService.
  jlong GetDistilledPagePrefsPtr(JNIEnv* env);

 private:
  // Friend class so that DomDistillerServiceFactoryAndroid has access to
  // private member object java_ref_.
  friend class DomDistillerServiceFactoryAndroid;
  // Points to a Java instance of DomDistillerService.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
  DomDistillerService* service_;
};

}  // namespace android
}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_SERVICE_ANDROID_H
