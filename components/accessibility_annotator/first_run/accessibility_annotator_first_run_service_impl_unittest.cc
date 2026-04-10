// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/first_run/accessibility_annotator_first_run_service_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/accessibility_annotator/core/accessibility_annotator_enablement_service.h"
#include "components/accessibility_annotator/core/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {
namespace {

using ::testing::_;
using ::testing::Return;

class MockAccessibilityAnnotatorFirstRunClient
    : public AccessibilityAnnotatorFirstRunClient {
 public:
  MOCK_METHOD(void,
              ShowRemoteAnnotatorInfo,
              (content::WebContents*,
               FirstRunInvocationSource,
               base::OnceCallback<void(InfoResult)>),
              (override));
};

class MockAccessibilityAnnotatorEnablementService
    : public AccessibilityAnnotatorEnablementService {
 public:
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
  MOCK_METHOD(RemoteAnnotatorEnablementState,
              GetEnablementState,
              (),
              (override));
};

class AccessibilityAnnotatorFirstRunServiceImplTest : public testing::Test {
 public:
  AccessibilityAnnotatorFirstRunServiceImplTest() {
    prefs::RegisterProfilePrefs(pref_service_.registry());

    auto client = std::make_unique<MockAccessibilityAnnotatorFirstRunClient>();
    client_ = client.get();

    service_ = std::make_unique<AccessibilityAnnotatorFirstRunServiceImpl>(
        std::move(client), &enablement_service_, &pref_service_);
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

  MockAccessibilityAnnotatorFirstRunClient* client() { return client_; }

  MockAccessibilityAnnotatorEnablementService* enablement_service() {
    return &enablement_service_;
  }

  AccessibilityAnnotatorFirstRunServiceImpl* service() {
    return service_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  MockAccessibilityAnnotatorEnablementService enablement_service_;
  std::unique_ptr<AccessibilityAnnotatorFirstRunServiceImpl> service_;
  raw_ptr<MockAccessibilityAnnotatorFirstRunClient> client_ = nullptr;
};

TEST_F(AccessibilityAnnotatorFirstRunServiceImplTest, SetsPrefOnAcknowledge) {
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(Return(RemoteAnnotatorEnablementState::kDisabledPendingInfo));

  EXPECT_CALL(*client(), ShowRemoteAnnotatorInfo)
      .WillOnce([](content::WebContents*, FirstRunInvocationSource,
                   base::OnceCallback<void(InfoResult)> callback) {
        std::move(callback).Run(InfoResult::kAcknowledged);
      });

  base::test::TestFuture<FirstRunTriggerResult> future;

  service()->MaybeTriggerFirstRun(nullptr, FirstRunInvocationSource::kAutofill,
                                  future.GetCallback());

  EXPECT_EQ(future.Get(), FirstRunTriggerResult::kSuccess);
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kShouldShowRemoteAnnotatorFirstRunInfo));
}

TEST_F(AccessibilityAnnotatorFirstRunServiceImplTest, DoesNotSetPrefOnDismiss) {
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(Return(RemoteAnnotatorEnablementState::kDisabledPendingInfo));

  EXPECT_CALL(*client(), ShowRemoteAnnotatorInfo)
      .WillOnce([](content::WebContents*, FirstRunInvocationSource,
                   base::OnceCallback<void(InfoResult)> callback) {
        std::move(callback).Run(InfoResult::kNotAcknowledged);
      });

  base::test::TestFuture<FirstRunTriggerResult> future;

  service()->MaybeTriggerFirstRun(nullptr, FirstRunInvocationSource::kAutofill,
                                  future.GetCallback());

  EXPECT_EQ(future.Get(), FirstRunTriggerResult::kSuccess);
  EXPECT_TRUE(pref_service()->GetBoolean(
      prefs::kShouldShowRemoteAnnotatorFirstRunInfo));
}

TEST_F(AccessibilityAnnotatorFirstRunServiceImplTest,
       DoesNotTriggerWhenNotEligible) {
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(Return(RemoteAnnotatorEnablementState::kDisabledNotEligible));

  EXPECT_CALL(*client(), ShowRemoteAnnotatorInfo).Times(0);

  base::test::TestFuture<FirstRunTriggerResult> future;

  service()->MaybeTriggerFirstRun(nullptr, FirstRunInvocationSource::kAutofill,
                                  future.GetCallback());

  EXPECT_EQ(future.Get(), FirstRunTriggerResult::kIgnoredNotEligible);
}

TEST_F(AccessibilityAnnotatorFirstRunServiceImplTest,
       DoesNotTriggerWhenAlreadyEnabled) {
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(Return(RemoteAnnotatorEnablementState::kEnabled));

  EXPECT_CALL(*client(), ShowRemoteAnnotatorInfo).Times(0);

  base::test::TestFuture<FirstRunTriggerResult> future;

  service()->MaybeTriggerFirstRun(nullptr, FirstRunInvocationSource::kAutofill,
                                  future.GetCallback());

  EXPECT_EQ(future.Get(), FirstRunTriggerResult::kIgnoredAlreadyEnabled);
}

TEST_F(AccessibilityAnnotatorFirstRunServiceImplTest,
       DoesNotTriggerWhenPendingSetup) {
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(Return(RemoteAnnotatorEnablementState::kDisabledPendingSetup));

  EXPECT_CALL(*client(), ShowRemoteAnnotatorInfo).Times(0);

  service()->MaybeTriggerFirstRun(nullptr, FirstRunInvocationSource::kAutofill,
                                  base::DoNothing());
}

TEST_F(AccessibilityAnnotatorFirstRunServiceImplTest, TriggersWhenPendingInfo) {
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(Return(RemoteAnnotatorEnablementState::kDisabledPendingInfo));

  EXPECT_CALL(*client(), ShowRemoteAnnotatorInfo).Times(1);

  service()->MaybeTriggerFirstRun(nullptr, FirstRunInvocationSource::kAutofill,
                                  base::DoNothing());
}

}  // namespace
}  // namespace accessibility_annotator
