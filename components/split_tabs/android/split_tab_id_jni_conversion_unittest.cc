// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/split_tabs/android/split_tab_id_jni_conversion.h"

#include "base/android/jni_android.h"
#include "base/token.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace split_tabs {

TEST(SplitTabIdJniConversionTest, TestJniConversions) {
  JNIEnv* env = base::android::AttachCurrentThread();
  const SplitTabId split_tab_id = SplitTabId::FromRawToken(
      base::Token(0xDEADBEEF12345678ULL, 0xABCDEF0123456789ULL));

  jni_zero::ScopedJavaLocalRef<jobject> j_split_tab_id =
      jni_zero::ToJniType(env, split_tab_id);
  EXPECT_EQ(split_tab_id,
            jni_zero::FromJniType<SplitTabId>(env, j_split_tab_id));
}

}  // namespace split_tabs
