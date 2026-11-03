// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_SERVICE_ANDROID_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_SERVICE_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/dom_distiller/core/dom_distiller_service.h"

namespace reader_mode::android {
class ReaderModeServiceFactoryAndroid;
}  // namespace reader_mode::android

namespace dom_distiller::android {

// Native implementation of DomDistillerService,
// provides access to Java DistilledPagePrefs.
class DomDistillerServiceAndroid {
 public:
  DomDistillerServiceAndroid(DomDistillerService* service);
  virtual ~DomDistillerServiceAndroid();
  // Returns native pointer to native DistilledPagePrefs registered with
  // DomDistillerService.
  int64_t GetDistilledPagePrefsPtr(JNIEnv* env);

 private:
  // Friend class so that ReaderModeServiceFactoryAndroid has access to
  // private member object java_ref_.
  friend class ::reader_mode::android::ReaderModeServiceFactoryAndroid;
  // Points to a Java instance of DomDistillerService.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
  raw_ptr<DomDistillerService> service_;
};

}  // namespace dom_distiller::android

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_SERVICE_ANDROID_H_
