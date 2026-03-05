// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/permissions/test/test_permissions_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace permissions {

class MockPermissionPromptDelegate : public PermissionPrompt::Delegate {
 public:
  MockPermissionPromptDelegate() = default;
  ~MockPermissionPromptDelegate() override = default;

  MOCK_METHOD(const std::vector<std::unique_ptr<PermissionRequest>>&,
              Requests,
              (),
              (override));
  MOCK_METHOD(GURL, GetRequestingOrigin, (), (const, override));
  MOCK_METHOD(GURL, GetEmbeddingOrigin, (), (const, override));
  MOCK_METHOD(void, Accept, (const PromptOptions&), (override));
  MOCK_METHOD(void, AcceptThisTime, (const PromptOptions&), (override));
  MOCK_METHOD(void, Deny, (const PromptOptions&), (override));
  MOCK_METHOD(void, Dismiss, (const PromptOptions&), (override));
  MOCK_METHOD(void, Ignore, (const PromptOptions&), (override));
  MOCK_METHOD(void, SwitchToLoudPrompt, (), (override));
  MOCK_METHOD(GeolocationAccuracy,
              GetInitialGeolocationAccuracySelection,
              (),
              (const, override));
  MOCK_METHOD(void, FinalizeCurrentRequests, (), (override));
  MOCK_METHOD(void, OpenHelpCenterLink, (const ui::Event&), (override));
  MOCK_METHOD(void, PreIgnoreQuietPrompt, (), (override));
  MOCK_METHOD(std::optional<PermissionUiSelector::QuietUiReason>,
              ReasonForUsingQuietUi,
              (),
              (const, override));
  MOCK_METHOD(bool, ShouldCurrentRequestUseQuietUI, (), (const, override));
  MOCK_METHOD(bool,
              ShouldDropCurrentRequestIfCannotShowQuietly,
              (),
              (const, override));
  MOCK_METHOD(bool, WasCurrentRequestAlreadyDisplayed, (), (override));
  MOCK_METHOD(void, SetDismissOnTabClose, (), (override));
  MOCK_METHOD(void, SetPromptShown, (), (override));
  MOCK_METHOD(void, SetDecisionTime, (), (override));
  MOCK_METHOD(void, SetManageClicked, (), (override));
  MOCK_METHOD(void, SetLearnMoreClicked, (), (override));
  MOCK_METHOD(void,
              SetHatsShownCallback,
              (base::OnceCallback<void()>),
              (override));
  MOCK_METHOD(content::WebContents*, GetAssociatedWebContents, (), (override));
  MOCK_METHOD(base::WeakPtr<Delegate>, GetWeakPtr, (), (override));
  MOCK_METHOD(bool, RecreateView, (), (override));
  MOCK_METHOD(const PermissionPrompt*, GetCurrentPrompt, (), (const, override));
};

class PermissionPromptAndroidFactoryTest
    : public content::RenderViewHostTestHarness {
 public:
  PermissionPromptAndroidFactoryTest() {
    requests.push_back(
        std::make_unique<MockPermissionRequest>(RequestType::kNotifications));
  }

  ~PermissionPromptAndroidFactoryTest() override = default;

 protected:
  testing::NiceMock<MockPermissionPromptDelegate> delegate;
  std::vector<std::unique_ptr<PermissionRequest>> requests;

 private:
  TestPermissionsClient client_;
};

TEST_F(PermissionPromptAndroidFactoryTest, FeatureEnabledAndQuietUi) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      permissions::features::kPermissionPromiseLifetimeModulationAndroid);

  ON_CALL(delegate, Requests()).WillByDefault(testing::ReturnRef(requests));

  EXPECT_CALL(delegate, ShouldCurrentRequestUseQuietUI())
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(delegate, PreIgnoreQuietPrompt()).Times(1);

  PermissionPrompt::Create(web_contents(), &delegate);
}

TEST_F(PermissionPromptAndroidFactoryTest, FeatureDisabledAndQuietUi) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      permissions::features::kPermissionPromiseLifetimeModulationAndroid);

  ON_CALL(delegate, Requests()).WillByDefault(testing::ReturnRef(requests));

  EXPECT_CALL(delegate, ShouldCurrentRequestUseQuietUI())
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(delegate, PreIgnoreQuietPrompt()).Times(0);

  PermissionPrompt::Create(web_contents(), &delegate);
}

TEST_F(PermissionPromptAndroidFactoryTest, FeatureEnabledAndLoudUi) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      permissions::features::kPermissionPromiseLifetimeModulationAndroid);

  ON_CALL(delegate, Requests()).WillByDefault(testing::ReturnRef(requests));

  EXPECT_CALL(delegate, ShouldCurrentRequestUseQuietUI())
      .WillRepeatedly(testing::Return(false));

  EXPECT_CALL(delegate, PreIgnoreQuietPrompt()).Times(0);

  PermissionPrompt::Create(web_contents(), &delegate);
}

}  // namespace permissions
