// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/content_setting_permission_context_base.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/permissions/test/permission_test_util.h"
#include "components/permissions/test/test_permissions_client.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/render_frame_host_test_support.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "services/network/public/cpp/permissions_policy/origin_with_possible_wildcards.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"
#include "url/gurl.h"

namespace permissions {

namespace {
using blink::mojom::EmbeddedPermissionRequestDescriptor;
using blink::mojom::EmbeddedPermissionRequestDescriptorPtr;
using blink::mojom::GeolocationEmbeddedPermissionRequestDescriptor;
using blink::mojom::PermissionDescriptor;
using blink::mojom::PermissionDescriptorPtr;
using blink::mojom::PermissionName;
}  // namespace

class PEPCInitiatedPermissionRequestTest
    : public content::RenderViewHostTestHarness {
 public:
  PEPCInitiatedPermissionRequestTest()
      : scoped_feature_list_(blink::features::kPermissionElement) {}
  PEPCInitiatedPermissionRequestTest(
      const PEPCInitiatedPermissionRequestTest&) = delete;
  PEPCInitiatedPermissionRequestTest& operator=(
      const PEPCInitiatedPermissionRequestTest&) = delete;
  ~PEPCInitiatedPermissionRequestTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    origin_ = GURL(permissions::MockPermissionRequest::kDefaultOrigin);
    NavigateAndCommit(GURL(origin_));

    PermissionRequestManager::CreateForWebContents(web_contents());

    prompt_factory_ =
        std::make_unique<permissions::MockPermissionPromptFactory>(
            PermissionRequestManager::FromWebContents(web_contents()));

    permission_request_callback_loop_ = std::make_unique<base::RunLoop>();
    RebindPermissionService();

    content::TestBrowserContext::FromBrowserContext(browser_context())
        ->SetPermissionControllerDelegate(
            GetPermissionControllerDelegate(browser_context()));
  }

  void TearDown() override {
    prompt_factory_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  void SetContentSetting(ContentSettingsType type, ContentSetting value) {
    PermissionsClient::Get()
        ->GetSettingsMap(browser_context())
        ->SetContentSettingDefaultScope(origin_, origin_, type, value);
  }

  ContentSetting GetContentSetting(ContentSettingsType type) {
    return PermissionsClient::Get()
        ->GetSettingsMap(browser_context())
        ->GetContentSetting(origin_, origin_, type);
  }

  blink::mojom::PermissionService* permission_service() {
    return service_remote_.get();
  }

  MockPermissionPromptFactory* prompt_factory() {
    return prompt_factory_.get();
  }

  std::vector<PermissionDescriptorPtr> CreatePermissionDescriptorPtrs(
      ContentSettingsType type) {
    std::vector<PermissionDescriptorPtr> descriptors;
    PermissionDescriptorPtr permission_descriptor = PermissionDescriptor::New();
    switch (type) {
      case ContentSettingsType::MEDIASTREAM_CAMERA:
        permission_descriptor->name = PermissionName::VIDEO_CAPTURE;
        break;
      case ContentSettingsType::MEDIASTREAM_MIC:
        permission_descriptor->name = PermissionName::AUDIO_CAPTURE;
        break;
      case ContentSettingsType::GEOLOCATION:
        permission_descriptor->name = PermissionName::GEOLOCATION;
        break;
      default:
        NOTREACHED() << "Unsupported permission type in this test fixture";
    }
    descriptors.push_back(std::move(permission_descriptor));
    return descriptors;
  }

  EmbeddedPermissionRequestDescriptorPtr
  CreateEmbeddedPermissionRequestDescriptorPtr() {
    EmbeddedPermissionRequestDescriptorPtr permission_descriptor =
        EmbeddedPermissionRequestDescriptor::New();

    return permission_descriptor;
  }

  EmbeddedPermissionRequestDescriptorPtr
  CreateGeolocationEmbeddedPermissionRequestDescriptorPtr(
      bool autolocate = false) {
    EmbeddedPermissionRequestDescriptorPtr permission_descriptor =
        EmbeddedPermissionRequestDescriptor::New();
    permission_descriptor->geolocation =
        GeolocationEmbeddedPermissionRequestDescriptor::New();
    permission_descriptor->geolocation->autolocate = autolocate;
    return permission_descriptor;
  }

  void WaitForPermissionServiceCallback() {
    permission_request_callback_loop_->Run();
    permission_request_callback_loop_ = std::make_unique<base::RunLoop>();
  }

  void PermissionServiceCallbackPEPC(
      blink::mojom::EmbeddedPermissionControlResult result) {
    permission_request_callback_loop_->Quit();
  }

  void PermissionServiceCallback(blink::mojom::PermissionStatus result) {
    permission_request_callback_loop_->Quit();
  }

  void RebindPermissionService(content::RenderFrameHost* rfh = nullptr) {
    if (!rfh) {
      rfh = main_rfh();
    }

    service_remote_.reset();

    content::CreatePermissionService(
        rfh, service_remote_.BindNewPipeAndPassReceiver());
  }

  const GURL& origin() { return origin_; }

 private:
  GURL origin_;
  mojo::Remote<blink::mojom::PermissionService> service_remote_;
  std::unique_ptr<MockPermissionPromptFactory> prompt_factory_;
  std::unique_ptr<base::RunLoop> permission_request_callback_loop_;
  TestPermissionsClient client_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PEPCInitiatedPermissionRequestTest, PEPCRequestWhenSettingAllowed) {
  // The current setting is allowed, all new prompts will be denied.
  SetContentSetting(ContentSettingsType::MEDIASTREAM_CAMERA,
                    CONTENT_SETTING_ALLOW);
  prompt_factory()->set_response_type(
      PermissionRequestManager::AutoResponseType::DENY_ALL);

  // A regular request will not reach the permission request manager, since the
  // permission is already granted.
  permission_service()->RequestPermission(
      CreatePermissionDescriptorPtrs(ContentSettingsType::MEDIASTREAM_CAMERA)
          .front()
          .Clone(),
      /* user_gesture= */ true,
      base::BindOnce(
          &PEPCInitiatedPermissionRequestTest::PermissionServiceCallback,
          base::Unretained(this)));

  WaitForPermissionServiceCallback();

  // No new prompts and no change in setting.
  EXPECT_EQ(0, prompt_factory()->request_count());
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(ContentSettingsType::MEDIASTREAM_CAMERA));

  // A PEPC request is allowed through regardless of the state of the content
  // setting.
  permission_service()->RequestPageEmbeddedPermission(
      CreatePermissionDescriptorPtrs(ContentSettingsType::MEDIASTREAM_CAMERA),
      CreateEmbeddedPermissionRequestDescriptorPtr(),
      base::BindOnce(
          &PEPCInitiatedPermissionRequestTest::PermissionServiceCallbackPEPC,
          base::Unretained(this)));

  WaitForPermissionServiceCallback();

  // A prompt has been shown and since the user denied, it has resulted in a
  // change to the content setting.
  EXPECT_EQ(prompt_factory()->request_count(), 1);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(ContentSettingsType::MEDIASTREAM_CAMERA));
}

TEST_F(PEPCInitiatedPermissionRequestTest, PEPCRequestWhenSettingBlocked) {
  // The current setting is blocked, all new prompts will be allowed.
  SetContentSetting(ContentSettingsType::MEDIASTREAM_MIC,
                    CONTENT_SETTING_BLOCK);
  prompt_factory()->set_response_type(
      PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  // A regular request will not reach the permission request manager, since the
  // permission is blocked.
  permission_service()->RequestPermission(
      CreatePermissionDescriptorPtrs(ContentSettingsType::MEDIASTREAM_MIC)
          .front()
          .Clone(),
      /* user_gesture= */ true,
      base::BindOnce(
          &PEPCInitiatedPermissionRequestTest::PermissionServiceCallback,
          base::Unretained(this)));

  WaitForPermissionServiceCallback();

  // No new prompts and no change in setting.
  EXPECT_EQ(0, prompt_factory()->request_count());
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(ContentSettingsType::MEDIASTREAM_MIC));

  // A PEPC request is allowed through regardless of the state of the content
  // setting.
  permission_service()->RequestPageEmbeddedPermission(
      CreatePermissionDescriptorPtrs(ContentSettingsType::MEDIASTREAM_MIC),
      CreateEmbeddedPermissionRequestDescriptorPtr(),
      base::BindOnce(
          &PEPCInitiatedPermissionRequestTest::PermissionServiceCallbackPEPC,
          base::Unretained(this)));

  WaitForPermissionServiceCallback();

  // A prompt has been shown and since the user granted, it has resulted in a
  // change to the content setting.
  EXPECT_EQ(prompt_factory()->request_count(), 1);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(ContentSettingsType::MEDIASTREAM_MIC));
}

TEST_F(PEPCInitiatedPermissionRequestTest, PEPCRequestBlockedInFencedFrame) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kFencedFrames, {{"implementation_type", "mparch"}});

  content::RenderFrameHost* fenced_frame =
      content::RenderFrameHostTester::For(main_rfh())->AppendFencedFrame();

  RebindPermissionService(fenced_frame);

  // A PEPC request is not allowed in a fenced frame.
  permission_service()->RequestPageEmbeddedPermission(
      CreatePermissionDescriptorPtrs(ContentSettingsType::MEDIASTREAM_MIC),
      CreateEmbeddedPermissionRequestDescriptorPtr(),
      base::BindOnce(
          &PEPCInitiatedPermissionRequestTest::PermissionServiceCallbackPEPC,
          base::Unretained(this)));

  WaitForPermissionServiceCallback();

  // No prompts have been created.
  EXPECT_EQ(prompt_factory()->request_count(), 0);
}

TEST_F(PEPCInitiatedPermissionRequestTest,
       PEPCRequestAllowedWithFeaturePolicy) {
  // The current setting is blocked, all new prompts will be allowed.
  SetContentSetting(ContentSettingsType::MEDIASTREAM_MIC,
                    CONTENT_SETTING_BLOCK);
  prompt_factory()->set_response_type(
      PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  network::ParsedPermissionsPolicy frame_policy;
  frame_policy.emplace_back(
      network::mojom::PermissionsPolicyFeature::kMicrophone,
      /*allowed_origins=*/
      std::vector{*network::OriginWithPossibleWildcards::FromOrigin(
          url::Origin::Create(origin()))},
      /*self_if_matches=*/std::nullopt, /*matches_all_origins=*/false,
      /*matches_opaque_src=*/false);
  content::RenderFrameHost* valid_child =
      content::RenderFrameHostTester::For(main_rfh())
          ->AppendChildWithPolicy("child", frame_policy);
  RebindPermissionService(valid_child);

  // A PEPC request is allowed through from a frame with a valid policy.
  permission_service()->RequestPageEmbeddedPermission(
      CreatePermissionDescriptorPtrs(ContentSettingsType::MEDIASTREAM_MIC),
      CreateEmbeddedPermissionRequestDescriptorPtr(),
      base::BindOnce(
          &PEPCInitiatedPermissionRequestTest::PermissionServiceCallbackPEPC,
          base::Unretained(this)));

  WaitForPermissionServiceCallback();

  // A prompt has been shown because the frame's policy allows requests.
  EXPECT_EQ(prompt_factory()->request_count(), 1);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(ContentSettingsType::MEDIASTREAM_MIC));
}

TEST_F(PEPCInitiatedPermissionRequestTest,
       PEPCRequestBlockedWithoutFeaturePolicy) {
  network::ParsedPermissionsPolicy frame_policy;
  frame_policy.push_back({network::mojom::PermissionsPolicyFeature::kMicrophone,
                          /*allowed_origins=*/
                          {*network::OriginWithPossibleWildcards::FromOrigin(
                              url::Origin::Create(GURL("http://fakeurl.com")))},
                          /*self_if_matches=*/std::nullopt,
                          /*matches_all_origins=*/false,
                          /*matches_opaque_src=*/false});
  content::RenderFrameHost* invalid_child =
      content::RenderFrameHostTester::For(main_rfh())
          ->AppendChildWithPolicy("child", frame_policy);
  RebindPermissionService(invalid_child);

  // A PEPC request is not allowed through from a frame without a valid policy.
  permission_service()->RequestPageEmbeddedPermission(
      CreatePermissionDescriptorPtrs(ContentSettingsType::MEDIASTREAM_MIC),
      CreateEmbeddedPermissionRequestDescriptorPtr(),
      base::BindOnce(
          &PEPCInitiatedPermissionRequestTest::PermissionServiceCallbackPEPC,
          base::Unretained(this)));

  WaitForPermissionServiceCallback();

  // No prompt has been shown because the request was not allowed.
  EXPECT_EQ(prompt_factory()->request_count(), 0);
}

TEST_F(PEPCInitiatedPermissionRequestTest, PEPCRequestBlockedByKillSwitch) {
  // Setup the kill switch.
  std::map<std::string, std::string> params;
  params[permissions::PermissionUtil::GetPermissionString(
      ContentSettingsType::MEDIASTREAM_CAMERA)] =
      ContentSettingPermissionContextBase::kPermissionsKillSwitchBlockedValue;
  base::AssociateFieldTrialParams(
      permissions::ContentSettingPermissionContextBase::
          kPermissionsKillSwitchFieldStudy,
      "TestGroup", params);
  base::FieldTrialList::CreateFieldTrial(
      permissions::ContentSettingPermissionContextBase::
          kPermissionsKillSwitchFieldStudy,
      "TestGroup");

  // Attempt to make a PEPC request.
  permission_service()->RequestPageEmbeddedPermission(
      CreatePermissionDescriptorPtrs(ContentSettingsType::MEDIASTREAM_CAMERA),
      CreateEmbeddedPermissionRequestDescriptorPtr(),
      base::BindOnce(
          &PEPCInitiatedPermissionRequestTest::PermissionServiceCallbackPEPC,
          base::Unretained(this)));

  WaitForPermissionServiceCallback();

  // PEPC requests are not allowed when the kill switch is on.
  EXPECT_EQ(prompt_factory()->request_count(), 0);
}

TEST_F(PEPCInitiatedPermissionRequestTest, PEPCRequestBlockedOnInsecureOrigin) {
  GURL insecure_origin("http://google.com");
  NavigateAndCommit(insecure_origin);
  RebindPermissionService();

  // Attempt to make a PEPC request.
  permission_service()->RequestPageEmbeddedPermission(
      CreatePermissionDescriptorPtrs(ContentSettingsType::MEDIASTREAM_CAMERA),
      CreateEmbeddedPermissionRequestDescriptorPtr(),
      base::BindOnce(
          &PEPCInitiatedPermissionRequestTest::PermissionServiceCallbackPEPC,
          base::Unretained(this)));

  WaitForPermissionServiceCallback();

  // PEPC requests are not allowed from insecure origins.
  EXPECT_EQ(prompt_factory()->request_count(), 0);
}

class PEPCInitiatedPermissionRequestTestWithAutolocate
    : public PEPCInitiatedPermissionRequestTest,
      public ::testing::WithParamInterface<bool> {};

TEST_P(PEPCInitiatedPermissionRequestTestWithAutolocate,
       PEPCRequestHeuristicallyGrantedGeolocation) {
  bool autolocate = GetParam();
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {blink::features::kGeolocationElement,
       permissions::features::kPermissionHeuristicAutoGrant},
      {});

  auto* history =
      PermissionsClient::Get()->GetPermissionActionsHistory(browser_context());
  const int kHeuristicGrantThreshold = 3;

  // Grant just below the threshold.
  for (int i = 0; i < kHeuristicGrantThreshold; ++i) {
    EXPECT_FALSE(history->RecordTemporaryGrant(
        origin(), ContentSettingsType::GEOLOCATION));
  }

  // The next grant will trigger the auto-grant.
  EXPECT_TRUE(history->RecordTemporaryGrant(origin(),
                                            ContentSettingsType::GEOLOCATION));
  EXPECT_TRUE(history->CheckHeuristicallyAutoGranted(
      origin(), ContentSettingsType::GEOLOCATION, /*needs_update*/ false));

  if (!autolocate) {
    // A PEPC request should be auto-granted without a prompt because Autolocate
    // is on.
    permission_service()->RequestPageEmbeddedPermission(
        CreatePermissionDescriptorPtrs(ContentSettingsType::GEOLOCATION),
        CreateGeolocationEmbeddedPermissionRequestDescriptorPtr(),
        base::BindOnce(
            &PEPCInitiatedPermissionRequestTest::PermissionServiceCallbackPEPC,
            base::Unretained(this)));

    WaitForPermissionServiceCallback();
    // TODO(crbug.com/448327619): verify content setting before and after.
    EXPECT_EQ(prompt_factory()->request_count(), 0);

    // The heuristic grant should not be reset because the user was not
    // prompted.
    EXPECT_TRUE(history->CheckHeuristicallyAutoGranted(
        origin(), ContentSettingsType::GEOLOCATION, /*needs_update*/ false));
    // Let's test dismissing the prompt resets the heuristic.
    // Now, reset the heuristic grant and increase the count by 2.
    history->ResetHeuristicData(origin(), ContentSettingsType::GEOLOCATION);
    EXPECT_FALSE(history->RecordTemporaryGrant(
        origin(), ContentSettingsType::GEOLOCATION));
    EXPECT_FALSE(history->RecordTemporaryGrant(
        origin(), ContentSettingsType::GEOLOCATION));
    prompt_factory()->set_response_type(
        PermissionRequestManager::AutoResponseType::DISMISS);

    permission_service()->RequestPageEmbeddedPermission(
        CreatePermissionDescriptorPtrs(ContentSettingsType::GEOLOCATION),
        CreateGeolocationEmbeddedPermissionRequestDescriptorPtr(),
        base::BindOnce(
            &PEPCInitiatedPermissionRequestTest::PermissionServiceCallbackPEPC,
            base::Unretained(this)));

    WaitForPermissionServiceCallback();

    // A second prompt was shown.
    EXPECT_EQ(prompt_factory()->request_count(), 1);
    EXPECT_EQ(history->GetTemporaryGrantCountForTesting(
                  origin(), ContentSettingsType::GEOLOCATION),
              0);
  } else {
    // A geolocation PEPC request should still show a prompt, even with a
    // heuristic grant, if autolocate is true.
    prompt_factory()->set_response_type(
        PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

    permission_service()->RequestPageEmbeddedPermission(
        CreatePermissionDescriptorPtrs(ContentSettingsType::GEOLOCATION),
        CreateGeolocationEmbeddedPermissionRequestDescriptorPtr(autolocate),
        base::BindOnce(
            &PEPCInitiatedPermissionRequestTest::PermissionServiceCallbackPEPC,
            base::Unretained(this)));

    WaitForPermissionServiceCallback();

    // A prompt should have been shown, and the setting updated to an ALLOW.
    EXPECT_EQ(prompt_factory()->request_count(), 1);
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              GetContentSetting(ContentSettingsType::GEOLOCATION));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         PEPCInitiatedPermissionRequestTestWithAutolocate,
                         ::testing::Bool());

}  // namespace permissions
