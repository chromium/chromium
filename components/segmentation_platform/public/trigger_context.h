// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_TRIGGER_CONTEXT_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_TRIGGER_CONTEXT_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace segmentation_platform {

// Contains contextual information for a trigger event.
struct TriggerContext {
 public:
  TriggerContext();
  virtual ~TriggerContext();

#if BUILDFLAG(IS_ANDROID)
  // Returns a Java object representing the TriggerContext.
  virtual base::android::ScopedJavaLocalRef<jobject> CreateJavaObject() const;
#endif  // BUILDFLAG(IS_ANDROID)
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_TRIGGER_CONTEXT_H_
