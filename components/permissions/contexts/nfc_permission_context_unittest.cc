// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/nfc_permission_context.h"

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/content_settings/browser/test_page_specific_content_settings_delegate.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/test_permissions_client.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/permissions/android/nfc/mock_nfc_system_level_setting.h"
#include "components/permissions/contexts/nfc_permission_context_android.h"
#endif

using content::MockRenderProcessHost;

namespace permissions {
namespace {
class TestNfcPermissionContextDelegate : public NfcPermissionContext::Delegate {
 public:
#if BUILDFLAG(IS_ANDROID)
  bool IsInteractable(content::WebContents* web_contents) override {
    return true;
  }
#endif
};
}  // namespace

// NfcPermissionContextTests ------------------------------------------

class NfcPermissionContextTests : public content::RenderViewHostTestHarness {
 protected:
  // RenderViewHostTestHarness:
  void SetUp() override;
  void TearDown() override;

  PermissionRequestID RequestID(int request_id);

  void RequestNfcPermission(const PermissionRequestID& id,
                            const GURL& requesting_frame,
                            bool user_gesture);

  void PermissionResponse(const PermissionRequestID& id,
                          ContentSetting content_setting);
  void CheckPermissionMessageSent(int request_id, bool allowed);
  void CheckPermissionMessageSentInternal(MockRenderProcessHost* process,
                                          int request_id,
                                          bool allowed);
  void SetupRequestManager(content::WebContents* web_contents);
  void RequestManagerDocumentLoadCompleted();
  ContentSetting GetNfcContentSetting(GURL frame_0, GURL frame_1);
  void SetNfcContentSetting(GURL frame_0,
                            GURL frame_1,
                            ContentSetting content_setting);
  bool HasActivePrompt();
  void AcceptPrompt();
  void DenyPrompt();
  void ClosePrompt();

  TestPermissionsClient client_;
  // Owned by |manager_|.
  raw_ptr<NfcPermissionContext> nfc_permission_context_;
  std::vector<std::unique_ptr<MockPermissionPromptFactory>>
      mock_permission_prompt_factories_;
  std::unique_ptr<PermissionManager> manager_;

  // A map between renderer child id and a pair represending the bridge id and
  // whether the requested permission was allowed.
  std::map<int,
           std::pair<permissions::PermissionRequestID::RequestLocalId, bool>>
      responses_;
};

PermissionRequestID NfcPermissionContextTests::RequestID(int request_id) {
  return PermissionRequestID(
      web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
      permissions::PermissionRequestID::RequestLocalId(request_id));
}

void NfcPermissionContextTests::RequestNfcPermission(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    bool user_gesture) {
  nfc_permission_context_->RequestPermission(
      PermissionRequestData(nfc_permission_context_, id, user_gesture,
                            requesting_frame),
      base::BindOnce(&NfcPermissionContextTests::PermissionResponse,
                     base::Unretained(this), id));
  content::RunAllTasksUntilIdle();
}

void NfcPermissionContextTests::PermissionResponse(
    const PermissionRequestID& id,
    ContentSetting content_setting) {
  responses_[id.global_render_frame_host_id().child_id] =
      std::make_pair(id.request_local_id_for_testing(),
                     content_setting == CONTENT_SETTING_ALLOW);
}

void NfcPermissionContextTests::CheckPermissionMessageSent(int request_id,
                                                           bool allowed) {
  CheckPermissionMessageSentInternal(process(), request_id, allowed);
}

void NfcPermissionContextTests::CheckPermissionMessageSentInternal(
    MockRenderProcessHost* process,
    int request_id,
    bool allowed) {
  ASSERT_EQ(responses_.count(process->GetID()), 1U);
  EXPECT_EQ(permissions::PermissionRequestID::RequestLocalId(request_id),
            responses_[process->GetID()].first);
  EXPECT_EQ(allowed, responses_[process->GetID()].second);
  responses_.erase(process->GetID());
}

void NfcPermissionContextTests::SetUp() {
  RenderViewHostTestHarness::SetUp();

  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<
          content_settings::TestPageSpecificContentSettingsDelegate>(
          /*prefs=*/nullptr,
          PermissionsClient::Get()->GetSettingsMap(browser_context())));
  SetupRequestManager(web_contents());

  auto delegate = std::make_unique<TestNfcPermissionContextDelegate>();

#if BUILDFLAG(IS_ANDROID)
  auto context = std::make_unique<NfcPermissionContextAndroid>(
      browser_context(), std::move(delegate));
  context->set_nfc_system_level_setting_for_testing(
      std::unique_ptr<NfcSystemLevelSetting>(new MockNfcSystemLevelSetting()));
  MockNfcSystemLevelSetting::SetNfcSystemLevelSettingEnabled(true);
  MockNfcSystemLevelSetting::SetNfcAccessIsPossible(true);
  MockNfcSystemLevelSetting::ClearHasShownNfcSettingPrompt();
#else
  auto context = std::make_unique<NfcPermissionContext>(browser_context(),
                                                        std::move(delegate));
#endif

  nfc_permission_context_ = context.get();

  PermissionManager::PermissionContextMap context_map;
  context_map[ContentSettingsType::NFC] = std::move(context);
  manager_ = std::make_unique<PermissionManager>(browser_context(),
                                                 std::move(context_map));
}

void NfcPermissionContextTests::TearDown() {
  mock_permission_prompt_factories_.clear();
  DeleteContents();
  nfc_permission_context_ = nullptr;
  manager_->Shutdown();
  manager_.reset();
  RenderViewHostTestHarness::TearDown();
}

void NfcPermissionContextTests::SetupRequestManager(
    content::WebContents* web_contents) {
  // Create PermissionRequestManager.
  PermissionRequestManager::CreateForWebContents(web_contents);
  PermissionRequestManager* permission_request_manager =
      PermissionRequestManager::FromWebContents(web_contents);

  // Create a MockPermissionPromptFactory for the PermissionRequestManager.
  mock_permission_prompt_factories_.push_back(
      std::make_unique<MockPermissionPromptFactory>(
          permission_request_manager));
}

void NfcPermissionContextTests::RequestManagerDocumentLoadCompleted() {
  PermissionRequestManager::FromWebContents(web_contents())
      ->DocumentOnLoadCompletedInPrimaryMainFrame();
}

ContentSetting NfcPermissionContextTests::GetNfcContentSetting(GURL frame_0,
                                                               GURL frame_1) {
  return PermissionsClient::Get()
      ->GetSettingsMap(browser_context())
      ->GetContentSetting(frame_0, frame_1, ContentSettingsType::NFC);
}

void NfcPermissionContextTests::SetNfcContentSetting(
    GURL frame_0,
    GURL frame_1,
    ContentSetting content_setting) {
  return PermissionsClient::Get()
      ->GetSettingsMap(browser_context())
      ->SetContentSettingDefaultScope(
          frame_0, frame_1, ContentSettingsType::NFC, content_setting);
}

bool NfcPermissionContextTests::HasActivePrompt() {
  return PermissionRequestManager::FromWebContents(web_contents())
      ->IsRequestInProgress();
}

void NfcPermissionContextTests::AcceptPrompt() {
  PermissionRequestManager::FromWebContents(web_contents())->Accept();
  base::RunLoop().RunUntilIdle();
}

void NfcPermissionContextTests::DenyPrompt() {
  PermissionRequestManager::FromWebContents(web_contents())->Deny();
  base::RunLoop().RunUntilIdle();
}

void NfcPermissionContextTests::ClosePrompt() {
  PermissionRequestManager::FromWebContents(web_contents())->Dismiss();
  base::RunLoop().RunUntilIdle();
}

// Tests ----------------------------------------------------------------------

TEST_F(NfcPermissionContextTests, SinglePermissionPrompt) {
  GURL requesting_frame("https://www.example.com/nfc");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();

  EXPECT_FALSE(HasActivePrompt());
  RequestNfcPermission(RequestID(0), requesting_frame, true /* user_gesture */);

#if BUILDFLAG(IS_ANDROID)
  ASSERT_TRUE(HasActivePrompt());
#else
  ASSERT_FALSE(HasActivePrompt());
#endif
}

TEST_F(NfcPermissionContextTests, SinglePermissionPromptFailsOnInsecureOrigin) {
  GURL requesting_frame("http://www.example.com/nfc");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();

  EXPECT_FALSE(HasActivePrompt());
  RequestNfcPermission(RequestID(0), requesting_frame, true);
  ASSERT_FALSE(HasActivePrompt());
}

#if BUILDFLAG(IS_ANDROID)
// Tests concerning Android NFC setting
TEST_F(NfcPermissionContextTests,
       SystemNfcSettingDisabledWhenNfcPermissionGetsGranted) {
  GURL requesting_frame("https://www.example.com/nfc");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  MockNfcSystemLevelSetting::SetNfcSystemLevelSettingEnabled(false);
  EXPECT_FALSE(HasActivePrompt());
  RequestNfcPermission(RequestID(0), requesting_frame, true);
  ASSERT_TRUE(HasActivePrompt());
  ASSERT_FALSE(MockNfcSystemLevelSetting::HasShownNfcSettingPrompt());
  AcceptPrompt();
  ASSERT_TRUE(MockNfcSystemLevelSetting::HasShownNfcSettingPrompt());
  CheckPermissionMessageSent(0 /* request _id */, true /* allowed */);
}

TEST_F(NfcPermissionContextTests,
       SystemNfcSettingDisabledWhenNfcPermissionGetsDenied) {
  GURL requesting_frame("https://www.example.com/nfc");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  MockNfcSystemLevelSetting::SetNfcSystemLevelSettingEnabled(false);
  EXPECT_FALSE(HasActivePrompt());
  RequestNfcPermission(RequestID(0), requesting_frame, true);
  ASSERT_TRUE(HasActivePrompt());
  ASSERT_FALSE(MockNfcSystemLevelSetting::HasShownNfcSettingPrompt());
  DenyPrompt();
  ASSERT_FALSE(MockNfcSystemLevelSetting::HasShownNfcSettingPrompt());
  CheckPermissionMessageSent(0 /* request _id */, false /* allowed */);
}

TEST_F(NfcPermissionContextTests,
       SystemNfcSettingDisabledWhenNfcPermissionAlreadyGranted) {
  GURL requesting_frame("https://www.example.com/nfc");
  SetNfcContentSetting(requesting_frame, requesting_frame,
                       CONTENT_SETTING_ALLOW);
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  MockNfcSystemLevelSetting::SetNfcSystemLevelSettingEnabled(false);
  EXPECT_FALSE(HasActivePrompt());
  RequestNfcPermission(RequestID(0), requesting_frame, true);
  ASSERT_FALSE(HasActivePrompt());
  ASSERT_TRUE(MockNfcSystemLevelSetting::HasShownNfcSettingPrompt());
}

TEST_F(NfcPermissionContextTests,
       SystemNfcSettingEnabledWhenNfcPermissionAlreadyGranted) {
  GURL requesting_frame("https://www.example.com/nfc");
  SetNfcContentSetting(requesting_frame, requesting_frame,
                       CONTENT_SETTING_ALLOW);
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  EXPECT_FALSE(HasActivePrompt());
  RequestNfcPermission(RequestID(0), requesting_frame, true);
  ASSERT_FALSE(HasActivePrompt());
  ASSERT_FALSE(MockNfcSystemLevelSetting::HasShownNfcSettingPrompt());
}

TEST_F(NfcPermissionContextTests,
       SystemNfcSettingCantBeEnabledWhenNfcPermissionGetsGranted) {
  GURL requesting_frame("https://www.example.com/nfc");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  MockNfcSystemLevelSetting::SetNfcSystemLevelSettingEnabled(false);
  MockNfcSystemLevelSetting::SetNfcAccessIsPossible(false);
  EXPECT_FALSE(HasActivePrompt());
  RequestNfcPermission(RequestID(0), requesting_frame, true);
  ASSERT_TRUE(HasActivePrompt());
  ASSERT_FALSE(MockNfcSystemLevelSetting::HasShownNfcSettingPrompt());
  AcceptPrompt();
  ASSERT_FALSE(HasActivePrompt());
  ASSERT_FALSE(MockNfcSystemLevelSetting::HasShownNfcSettingPrompt());
  CheckPermissionMessageSent(0 /* request _id */, true /* allowed */);
}

TEST_F(NfcPermissionContextTests,
       SystemNfcSettingCantBeEnabledWhenNfcPermissionGetsDenied) {
  GURL requesting_frame("https://www.example.com/nfc");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  MockNfcSystemLevelSetting::SetNfcSystemLevelSettingEnabled(false);
  MockNfcSystemLevelSetting::SetNfcAccessIsPossible(false);
  EXPECT_FALSE(HasActivePrompt());
  RequestNfcPermission(RequestID(0), requesting_frame, true);
  ASSERT_TRUE(HasActivePrompt());
  ASSERT_FALSE(MockNfcSystemLevelSetting::HasShownNfcSettingPrompt());
  DenyPrompt();
  ASSERT_FALSE(HasActivePrompt());
  ASSERT_FALSE(MockNfcSystemLevelSetting::HasShownNfcSettingPrompt());
  CheckPermissionMessageSent(0 /* request _id */, false /* allowed */);
}

TEST_F(NfcPermissionContextTests,
       SystemNfcSettingCantBeEnabledWhenNfcPermissionAlreadyGranted) {
  GURL requesting_frame("https://www.example.com/nfc");
  SetNfcContentSetting(requesting_frame, requesting_frame,
                       CONTENT_SETTING_ALLOW);
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  MockNfcSystemLevelSetting::SetNfcSystemLevelSettingEnabled(false);
  MockNfcSystemLevelSetting::SetNfcAccessIsPossible(false);
  EXPECT_FALSE(HasActivePrompt());
  RequestNfcPermission(RequestID(0), requesting_frame, true);
  ASSERT_FALSE(HasActivePrompt());
  ASSERT_FALSE(MockNfcSystemLevelSetting::HasShownNfcSettingPrompt());
  CheckPermissionMessageSent(0 /* request _id */, true /* allowed */);
}

TEST_F(NfcPermissionContextTests, CancelNfcPermissionRequest) {
  GURL requesting_frame("https://www.example.com/nfc");
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetNfcContentSetting(requesting_frame, requesting_frame));

  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();

  ASSERT_FALSE(HasActivePrompt());

  RequestNfcPermission(RequestID(0), requesting_frame, true);

  ASSERT_TRUE(HasActivePrompt());

  // Simulate the frame going away; the request should be removed.
  ClosePrompt();

  ASSERT_FALSE(MockNfcSystemLevelSetting::HasShownNfcSettingPrompt());

  // Ensure permission isn't persisted.
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetNfcContentSetting(requesting_frame, requesting_frame));
}
#endif

}  // namespace permissions
