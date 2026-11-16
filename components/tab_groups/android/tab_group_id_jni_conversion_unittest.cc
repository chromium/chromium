// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tab_groups/android/tab_group_id_jni_conversion.h"

#include "base/android/jni_android.h"
#include "base/token.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {

TEST(TabGroupIdJniConversionTest, TestJniConversions) {
  JNIEnv* env = base::android::AttachCurrentThread();
  const TabGroupId tab_group_id = TabGroupId::FromRawToken(
      base::Token(0xDEADBEEF12345678ULL, 0xABCDEF0123456789ULL));

  jni_zero::ScopedJavaLocalRef<jobject> j_tab_group_id =
      jni_zero::ToJniType(env, tab_group_id);
  EXPECT_EQ(tab_group_id,
            jni_zero::FromJniType<TabGroupId>(env, j_tab_group_id));
}

}  // namespace tab_groups
