// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STYLUS_HANDWRITING_ANDROID_STYLUS_HANDWRITING_FEATURE_MAP_H_
#define COMPONENTS_STYLUS_HANDWRITING_ANDROID_STYLUS_HANDWRITING_FEATURE_MAP_H_

#include <jni.h>

#include "base/feature_list.h"

namespace stylus_handwriting::android {

BASE_DECLARE_FEATURE(kCacheStylusSettings);
BASE_DECLARE_FEATURE(kUseHandwritingInitiator);

}  // namespace stylus_handwriting::android

#endif  // COMPONENTS_STYLUS_HANDWRITING_ANDROID_STYLUS_HANDWRITING_FEATURE_MAP_H_
