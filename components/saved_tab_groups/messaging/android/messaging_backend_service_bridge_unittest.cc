// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/messaging/android/messaging_backend_service_bridge.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "components/saved_tab_groups/messaging/android/messaging_backend_service_bridge.h"
#include "components/saved_tab_groups/messaging/messaging_backend_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/saved_tab_groups/messaging/android/native_j_unittests_jni_headers/MessagingBackendServiceBridgeUnitTestCompanion_jni.h"

using testing::_;
using testing::Return;

namespace tab_groups::messaging::android {

class MockMessagingBackendService : public MessagingBackendService {
 public:
  MockMessagingBackendService() = default;
  ~MockMessagingBackendService() override = default;

  // MessagingBackendService implementation.
  MOCK_METHOD(void, SetInstantMessageDelegate, (InstantMessageDelegate*));
  MOCK_METHOD(void, AddPersistentMessageObserver, (PersistentMessageObserver*));
  MOCK_METHOD(void,
              RemovePersistentMessageObserver,
              (PersistentMessageObserver*));
  MOCK_METHOD(bool, IsInitialized, ());
  MOCK_METHOD(std::vector<PersistentMessage>,
              GetMessagesForTab,
              (EitherTabID, std::optional<PersistentNotificationType>));
  MOCK_METHOD(std::vector<PersistentMessage>,
              GetMessagesForGroup,
              (EitherGroupID, std::optional<PersistentNotificationType>));
  MOCK_METHOD(std::vector<PersistentMessage>,
              GetMessages,
              (std::optional<PersistentNotificationType>));
};

class MessagingBackendServiceBridgeTest : public testing::Test {
 public:
  MessagingBackendServiceBridgeTest() = default;
  ~MessagingBackendServiceBridgeTest() override = default;

  void SetUp() override {
    EXPECT_CALL(service(), AddPersistentMessageObserver(_)).Times(1);
    EXPECT_CALL(service(), SetInstantMessageDelegate(_)).Times(1);
    bridge_ = MessagingBackendServiceBridge::CreateForTest(&service_);

    j_service_ = bridge_->GetJavaObject();
    j_companion_ =
        Java_MessagingBackendServiceBridgeUnitTestCompanion_Constructor(
            base::android::AttachCurrentThread(), j_service_);
  }

  void TearDown() override {
    EXPECT_CALL(service_, SetInstantMessageDelegate(nullptr));
    EXPECT_CALL(service_, RemovePersistentMessageObserver(bridge()));
  }

  // API wrapper methods, since they are intentionaly private in the bridge.
  void OnMessagingBackendServiceInitialized() {
    bridge()->OnMessagingBackendServiceInitialized();
  }

  void DisplayPersistentMessage(PersistentMessage message) {
    bridge()->DisplayPersistentMessage(message);
  }

  void HidePersistentMessage(PersistentMessage message) {
    bridge()->HidePersistentMessage(message);
  }

  void DisplayInstantaneousMessage(InstantMessage message) {
    bridge()->DisplayInstantaneousMessage(message);
  }

  MessagingBackendServiceBridge* bridge() { return bridge_.get(); }
  MockMessagingBackendService& service() { return service_; }
  base::android::ScopedJavaGlobalRef<jobject> j_service() { return j_service_; }
  base::android::ScopedJavaGlobalRef<jobject> j_companion() {
    return j_companion_;
  }

 private:
  MockMessagingBackendService service_;
  std::unique_ptr<MessagingBackendServiceBridge> bridge_;
  base::android::ScopedJavaGlobalRef<jobject> j_service_;
  base::android::ScopedJavaGlobalRef<jobject> j_companion_;
};

TEST_F(MessagingBackendServiceBridgeTest, TestInitializationStatus) {
  EXPECT_CALL(service(), IsInitialized).WillOnce(Return(false));
  EXPECT_FALSE(
      Java_MessagingBackendServiceBridgeUnitTestCompanion_isInitialized(
          base::android::AttachCurrentThread(), j_companion()));

  EXPECT_CALL(service(), IsInitialized).WillOnce(Return(true));
  EXPECT_TRUE(Java_MessagingBackendServiceBridgeUnitTestCompanion_isInitialized(
      base::android::AttachCurrentThread(), j_companion()));
}

TEST_F(MessagingBackendServiceBridgeTest, TestPersistentMessageObservation) {
  // Add Java observer.
  Java_MessagingBackendServiceBridgeUnitTestCompanion_addPersistentMessageObserver(
      base::android::AttachCurrentThread(), j_companion());

  // Verify Java observer is called on init.
  OnMessagingBackendServiceInitialized();
  Java_MessagingBackendServiceBridgeUnitTestCompanion_verifyOnInitializedCalled(
      base::android::AttachCurrentThread(), j_companion(), 1);

  // Remove Java observer.
  Java_MessagingBackendServiceBridgeUnitTestCompanion_removePersistentMessageObserver(
      base::android::AttachCurrentThread(), j_companion());

  // Verify Java observer is not called again (since it should be removed), so
  // the total call count should still be 1.
  OnMessagingBackendServiceInitialized();
  Java_MessagingBackendServiceBridgeUnitTestCompanion_verifyOnInitializedCalled(
      base::android::AttachCurrentThread(), j_companion(), 1);
}

}  // namespace tab_groups::messaging::android
