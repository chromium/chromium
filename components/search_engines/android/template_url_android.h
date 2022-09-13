// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_ANDROID_TEMPLATE_URL_ANDROID_H_
#define COMPONENTS_SEARCH_ENGINES_ANDROID_TEMPLATE_URL_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "components/search_engines/template_url.h"

base::android::ScopedJavaLocalRef<jobject> CreateTemplateUrlAndroid(
    JNIEnv* env,
    const TemplateURL* template_url);

#endif  // COMPONENTS_SEARCH_ENGINES_ANDROID_TEMPLATE_URL_ANDROID_H_
