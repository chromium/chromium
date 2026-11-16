// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/split_tabs/android/split_tab_visual_data_jni_conversion.h"

#include "base/android/jni_android.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace split_tabs {

TEST(SplitTabVisualDataJniConversionTest, TestJniConversions) {
  JNIEnv* env = base::android::AttachCurrentThread();

  const SplitTabVisualData split_tab_visual_data(SplitTabLayout::kSideBySide,
                                                 0.35);
  jni_zero::ScopedJavaLocalRef<jobject> j_split_tab_visual_data =
      jni_zero::ToJniType(env, split_tab_visual_data);
  EXPECT_EQ(split_tab_visual_data, jni_zero::FromJniType<SplitTabVisualData>(
                                       env, j_split_tab_visual_data));
}

}  // namespace split_tabs
