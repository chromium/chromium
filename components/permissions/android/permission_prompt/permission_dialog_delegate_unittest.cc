// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/permissions/android/permission_prompt/permission_dialog_delegate.h"

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/android/permission_prompt/permission_prompt_android.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/test/mock_permission_prompt_delegate.h"
#include "components/permissions/test/mock_permission_request.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::Exactly;

namespace permissions {
namespace test {

class MockPermissionPromptAndroid : public PermissionPromptAndroid {
 public:
  MockPermissionPromptAndroid(content::WebContents* web_contents,
                              PermissionPrompt::Delegate* delegate)
      : PermissionPromptAndroid(web_contents, delegate) {}

  MOCK_METHOD(PermissionPromptDisposition,
              GetPromptDisposition,
              (),
              (const, override));
};

class MockPermissionDialogJavaDelegate : public PermissionDialogJavaDelegate {
 public:
  MockPermissionDialogJavaDelegate() : PermissionDialogJavaDelegate(nullptr) {}
  MOCK_METHOD(void,
              CreateJavaDelegate,
              (content::WebContents * web_contents,
               PermissionDialogDelegate* owner),
              (override));
  MOCK_METHOD(void,
              CreateDialog,
              (content::WebContents * web_contents),
              (override));
  MOCK_METHOD(void, DismissDialog, (), (override));
};

class PermissionDialogDelegateTest : public content::RenderViewHostTestHarness {
};

TEST_F(PermissionDialogDelegateTest, DismissDialogWhenPrimaryPageChanges) {
  auto mock_permission_dialog_java_delegate =
      std::make_unique<testing::NiceMock<MockPermissionDialogJavaDelegate>>();
  EXPECT_CALL(*mock_permission_dialog_java_delegate, DismissDialog())
      .Times(testing::AtLeast(1));

  auto dialog = PermissionDialogDelegate::CreateForTesting(
      web_contents(), nullptr, std::move(mock_permission_dialog_java_delegate));

  // Use NavigationSimulator to simulate a navigation from the browser to make
  // sure that PermissionDialogDelegate::PrimaryPageChanged gets called.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(url::kAboutBlankURL));
}

TEST_F(PermissionDialogDelegateTest, PropagateGeolocationAccuracy) {
  base::test::ScopedFeatureList enable_approximate_location{
      content_settings::features::kApproximateGeolocationPermission};
  std::vector<std::unique_ptr<PermissionRequest>> owned_requests;

  testing::NiceMock<MockPermissionPromptDelegate> mock_delegate;
  owned_requests.push_back(std::make_unique<MockPermissionRequest>(
      GURL("https://example.com"), RequestType::kGeolocation,
      PermissionRequestGestureType::GESTURE,
      std::optional<GeolocationPromptType>(
          GeolocationPromptType::kApproximateOrPrecise)));

  std::vector<base::WeakPtr<PermissionRequest>> requests;
  requests.push_back(owned_requests[0]->GetWeakPtr());

  ON_CALL(mock_delegate, Requests())
      .WillByDefault(testing::ReturnRef(owned_requests));

  testing::NiceMock<MockPermissionPromptAndroid> mock_permission_prompt(
      web_contents(), &mock_delegate);

  EXPECT_CALL(mock_delegate, GetInitialGeolocationAccuracySelection())
      .WillRepeatedly(testing::Return(GeolocationAccuracy::kApproximate));

  auto mock_permission_dialog_java_delegate =
      std::make_unique<testing::NiceMock<MockPermissionDialogJavaDelegate>>();

  auto dialog = PermissionDialogDelegate::CreateForTesting(
      web_contents(), &mock_permission_prompt,
      std::move(mock_permission_dialog_java_delegate));

  // Initially, it should use the value from
  // GetInitialGeolocationAccuracySelection.
  EXPECT_CALL(mock_delegate,
              Accept(testing::VariantWith<std::monostate>(testing::_)))
      .Times(1);
  dialog->Accept(nullptr);

  // Update accuracy and verify it's propagated.
  dialog->OnGeolocationAccuracySelected(
      nullptr, static_cast<int32_t>(GeolocationAccuracy::kPrecise));
  EXPECT_CALL(mock_delegate,
              Accept(testing::VariantWith<GeolocationPromptOptions>(
                  GeolocationPromptOptions{.selected_accuracy =
                                               GeolocationAccuracy::kPrecise})))
      .Times(1);
  dialog->Accept(nullptr);
}

}  // namespace test
}  // namespace permissions
