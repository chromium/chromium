// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/first_run/accessibility_annotator_first_run_service_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/accessibility_annotator/core/accessibility_annotator_enablement_service.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {
namespace {

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
    auto client = std::make_unique<MockAccessibilityAnnotatorFirstRunClient>();
    client_ = client.get();

    service_ = std::make_unique<AccessibilityAnnotatorFirstRunServiceImpl>(
        std::move(client), &enablement_service_);
  }

  MockAccessibilityAnnotatorFirstRunClient* client() { return client_; }
  MockAccessibilityAnnotatorEnablementService* enablement_service() {
    return &enablement_service_;
  }
  AccessibilityAnnotatorFirstRunServiceImpl* service() {
    return service_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  MockAccessibilityAnnotatorEnablementService enablement_service_;
  std::unique_ptr<AccessibilityAnnotatorFirstRunServiceImpl> service_;
  raw_ptr<MockAccessibilityAnnotatorFirstRunClient> client_ = nullptr;
};

TEST_F(AccessibilityAnnotatorFirstRunServiceImplTest,
       DoesNotTriggerWhenNotEligible) {
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(testing::Return(
          RemoteAnnotatorEnablementState::kDisabledNotEligible));

  EXPECT_CALL(*client(),
              ShowRemoteAnnotatorInfo(testing::_, testing::_, testing::_))
      .Times(0);

  bool callback_run = false;
  service()->MaybeTriggerFirstRun(
      nullptr, FirstRunInvocationSource::kAutofill,
      base::BindLambdaForTesting([&](FirstRunTriggerResult result) {
        EXPECT_EQ(result, FirstRunTriggerResult::kIgnoredNotEligible);
        callback_run = true;
      }));
  EXPECT_TRUE(callback_run);
}

TEST_F(AccessibilityAnnotatorFirstRunServiceImplTest,
       DoesNotTriggerWhenAlreadyEnabled) {
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(testing::Return(RemoteAnnotatorEnablementState::kEnabled));

  EXPECT_CALL(*client(),
              ShowRemoteAnnotatorInfo(testing::_, testing::_, testing::_))
      .Times(0);

  bool callback_run = false;
  service()->MaybeTriggerFirstRun(
      nullptr, FirstRunInvocationSource::kAutofill,
      base::BindLambdaForTesting([&](FirstRunTriggerResult result) {
        EXPECT_EQ(result, FirstRunTriggerResult::kIgnoredAlreadyEnabled);
        callback_run = true;
      }));
  EXPECT_TRUE(callback_run);
}

TEST_F(AccessibilityAnnotatorFirstRunServiceImplTest,
       DoesNotTriggerWhenPendingSetup) {
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(testing::Return(
          RemoteAnnotatorEnablementState::kDisabledPendingSetup));

  EXPECT_CALL(*client(),
              ShowRemoteAnnotatorInfo(testing::_, testing::_, testing::_))
      .Times(0);

  service()->MaybeTriggerFirstRun(nullptr, FirstRunInvocationSource::kAutofill,
                                  base::DoNothing());
}

TEST_F(AccessibilityAnnotatorFirstRunServiceImplTest, TriggersWhenPendingInfo) {
  EXPECT_CALL(*enablement_service(), GetEnablementState())
      .WillOnce(testing::Return(
          RemoteAnnotatorEnablementState::kDisabledPendingInfo));

  EXPECT_CALL(*client(),
              ShowRemoteAnnotatorInfo(testing::_, testing::_, testing::_))
      .Times(1);

  service()->MaybeTriggerFirstRun(nullptr, FirstRunInvocationSource::kAutofill,
                                  base::DoNothing());
}

}  // namespace
}  // namespace accessibility_annotator
