// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/messages/android/message_wrapper.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/test/task_environment.h"
#include "components/messages/android/test/messages_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace messages {

class MessageWrapperTest : public ::testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  MessagesTestHelper helper_;
};

TEST_F(MessageWrapperTest, ClearsJavaPointerOnDestruction) {
  // Create the MessageWrapper. MessageIdentifier::TEST_MESSAGE is used.
  auto message_wrapper =
      std::make_unique<MessageWrapper>(MessageIdentifier::TEST_MESSAGE);

  // Keep a reference to the Java object before destroying the C++ object.
  base::android::ScopedJavaGlobalRef<jobject> java_object;
  java_object.Reset(base::android::AttachCurrentThread(),
                    message_wrapper->GetJavaMessageWrapper());

  // Verify it is not 0 initially.
  ASSERT_NE(0, helper_.GetNativePtr(java_object));

  // Destroy the C++ object.
  message_wrapper.reset();

  // Verify that the Java pointer was cleared (set to 0).
  EXPECT_EQ(0, helper_.GetNativePtr(java_object));
}

}  // namespace messages
