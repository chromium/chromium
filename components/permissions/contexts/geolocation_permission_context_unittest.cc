// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/geolocation_permission_context.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/id_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "build/build_config.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/browser/test_page_specific_content_settings_delegate.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_context_base.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/permission_test_util.h"
#include "components/permissions/test/test_permissions_client.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/geolocation/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/location/android/location_settings_dialog_outcome.h"
#include "components/location/android/mock_location_settings.h"
#include "components/permissions/contexts/geolocation_permission_context_android.h"
#include "components/prefs/pref_service.h"
#endif

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
#include "components/permissions/contexts/geolocation_permission_context_system.h"
#include "services/device/public/cpp/test/fake_geolocation_system_permission_manager.h"
#endif  // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)

using content::MockRenderProcessHost;

namespace permissions {
namespace {

using blink::mojom::PermissionStatus;

class TestGeolocationPermissionContextDelegate
    : public GeolocationPermissionContext::Delegate {
 public:
  explicit TestGeolocationPermissionContextDelegate(
      content::BrowserContext* browser_context) {
#if BUILDFLAG(IS_ANDROID)
    GeolocationPermissionContextAndroid::RegisterProfilePrefs(
        prefs_.registry());
#endif
  }

  bool DecidePermission(const PermissionRequestID& id,
                        const GURL& requesting_origin,
                        bool user_gesture,
                        BrowserPermissionCallback* callback,
                        GeolocationPermissionContext* context) override {
    return false;
  }

#if BUILDFLAG(IS_ANDROID)
  bool IsInteractable(content::WebContents* web_contents) override {
    return true;
  }

  PrefService* GetPrefs(content::BrowserContext* browser_context) override {
    return &prefs_;
  }

  bool IsRequestingOriginDSE(content::BrowserContext* browser_context,
                             const GURL& requesting_origin) override {
    return dse_origin_ &&
           dse_origin_.value() == url::Origin::Create(requesting_origin);
  }
#endif

  void SetDSEOriginForTesting(const url::Origin& dse_origin) {
    dse_origin_ = dse_origin;
  }

 private:
  TestingPrefServiceSimple prefs_;
  std::optional<url::Origin> dse_origin_;
};
}  // namespace

// GeolocationPermissionContextTests ------------------------------------------

class GeolocationPermissionContextTests
    : public content::RenderViewHostTestHarness,
      public permissions::Observer {
 public:
  GeolocationPermissionContextTests();

 protected:
  // RenderViewHostTestHarness:
  void SetUp() override;
  void TearDown() override;
  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override;

  PermissionRequestID RequestID(int request_id);
  PermissionRequestID RequestIDForTab(int tab, int request_id);

  void RequestGeolocationPermission(const PermissionRequestID& id,
                                    const GURL& requesting_frame,
                                    bool user_gesture);

  blink::mojom::PermissionStatus GetPermissionStatus(
      blink::PermissionType permission,
      const GURL& requesting_origin);

  void PermissionResponse(const PermissionRequestID& id,
                          ContentSetting content_setting);
  void CheckPermissionMessageSent(int request_id, bool allowed);
  void CheckPermissionMessageSentForTab(int tab, int request_id, bool allowed);
  void CheckPermissionMessageSentInternal(MockRenderProcessHost* process,
                                          int request_id,
                                          bool allowed);
  void AddNewTab(const GURL& url);
  void CheckTabContentsState(const GURL& requesting_frame,
                             ContentSetting expected_content_setting);
  void SetupRequestManager(content::WebContents* web_contents);

  // permissions::Observer:
  void OnPermissionChanged(const ContentSettingsPattern& primary_pattern,
                           const ContentSettingsPattern& secondary_pattern,
                           ContentSettingsTypeSet content_type_set) override;

#if BUILDFLAG(IS_ANDROID)
  bool RequestPermissionIsLSDShown(const GURL& origin);
  bool RequestPermissionIsLSDShownWithPermissionPrompt(const GURL& origin);
  void AddDayOffsetForTesting(int days);
#endif
  void RequestManagerDocumentLoadCompleted();
  void RequestManagerDocumentLoadCompleted(content::WebContents* web_contents);
  ContentSetting GetGeolocationContentSetting(GURL frame_0, GURL frame_1);
  void SetGeolocationContentSetting(GURL frame_0,
                                    GURL frame_1,
                                    ContentSetting content_setting);
  bool HasActivePrompt();
  bool HasActivePrompt(content::WebContents* web_contents);
  void AcceptPrompt();
  void AcceptPrompt(content::WebContents* web_contents);
  void AcceptPromptThisTime();
  void DenyPrompt();
  void ClosePrompt();
  std::u16string GetPromptText();

  TestPermissionsClient client_;
  // owned by |BrowserContest::GetPermissionControllerDelegate()|
  raw_ptr<GeolocationPermissionContext, AcrossTasksDanglingUntriaged>
      geolocation_permission_context_ = nullptr;
  // owned by |geolocation_permission_context_|
  raw_ptr<TestGeolocationPermissionContextDelegate,
          AcrossTasksDanglingUntriaged>
      delegate_ = nullptr;
  std::vector<std::unique_ptr<content::WebContents>> extra_tabs_;
  std::vector<std::unique_ptr<MockPermissionPromptFactory>>
      mock_permission_prompt_factories_;

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  raw_ptr<device::FakeGeolocationSystemPermissionManager>
      fake_geolocation_system_permission_manager_;
#endif  // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)

  // A map between renderer child id and a pair represending the bridge id and
  // whether the requested permission was allowed.
  std::map<int, std::pair<PermissionRequestID::RequestLocalId, bool>>
      responses_;
  int num_permission_updates_ = 0;
  raw_ptr<ContentSettingsPattern> expected_primary_pattern_ = nullptr;
  raw_ptr<ContentSettingsPattern> expected_secondary_pattern_ = nullptr;

  base::test::ScopedFeatureList feature_list_;
};

GeolocationPermissionContextTests::GeolocationPermissionContextTests() {
  feature_list_.InitWithFeatures(/*enabled_features=*/
                                 {
                                     permissions::features::kOneTimePermission,
#if BUILDFLAG(IS_WIN)
                                     ::features::kWinSystemLocationPermission,
#endif  // BUILDFLAG(IS_WIN)
                                 },
                                 /*disabled_features=*/{});
}

PermissionRequestID GeolocationPermissionContextTests::RequestID(
    int request_id) {
  return PermissionRequestID(
      web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
      PermissionRequestID::RequestLocalId(request_id));
}

PermissionRequestID GeolocationPermissionContextTests::RequestIDForTab(
    int tab,
    int request_id) {
  return PermissionRequestID(
      extra_tabs_[tab]->GetPrimaryMainFrame()->GetGlobalId(),
      PermissionRequestID::RequestLocalId(request_id));
}

void GeolocationPermissionContextTests::RequestGeolocationPermission(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    bool user_gesture) {
  geolocation_permission_context_->RequestPermission(
      permissions::PermissionRequestData(geolocation_permission_context_, id,
                                         user_gesture, requesting_frame),
      base::BindOnce(&GeolocationPermissionContextTests::PermissionResponse,
                     base::Unretained(this), id));
  content::RunAllTasksUntilIdle();
}

blink::mojom::PermissionStatus
GeolocationPermissionContextTests::GetPermissionStatus(
    blink::PermissionType permission,
    const GURL& requesting_origin) {
  return browser_context()
      ->GetPermissionController()
      ->GetPermissionResultForOriginWithoutContext(
          permission, url::Origin::Create(requesting_origin))
      .status;
}

void GeolocationPermissionContextTests::PermissionResponse(
    const PermissionRequestID& id,
    ContentSetting content_setting) {
  responses_[id.global_render_frame_host_id().child_id] =
      std::make_pair(id.request_local_id_for_testing(),
                     content_setting == CONTENT_SETTING_ALLOW);
}

void GeolocationPermissionContextTests::OnPermissionChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  EXPECT_TRUE(primary_pattern.IsValid());
  EXPECT_TRUE(secondary_pattern.IsValid());
  EXPECT_EQ(*expected_primary_pattern_, primary_pattern);
  EXPECT_EQ(*expected_secondary_pattern_, secondary_pattern);
  EXPECT_EQ(content_type_set.GetType(), ContentSettingsType::GEOLOCATION);
  num_permission_updates_++;
}

void GeolocationPermissionContextTests::CheckPermissionMessageSent(
    int request_id,
    bool allowed) {
  CheckPermissionMessageSentInternal(process(), request_id, allowed);
}

void GeolocationPermissionContextTests::CheckPermissionMessageSentForTab(
    int tab,
    int request_id,
    bool allowed) {
  CheckPermissionMessageSentInternal(
      static_cast<MockRenderProcessHost*>(
          extra_tabs_[tab]->GetPrimaryMainFrame()->GetProcess()),
      request_id, allowed);
}

void GeolocationPermissionContextTests::CheckPermissionMessageSentInternal(
    MockRenderProcessHost* process,
    int request_id,
    bool allowed) {
  ASSERT_EQ(responses_.count(process->GetID()), 1U);
  EXPECT_EQ(PermissionRequestID::RequestLocalId(request_id),
            responses_[process->GetID()].first);
  EXPECT_EQ(allowed, responses_[process->GetID()].second);
  responses_.erase(process->GetID());
}

void GeolocationPermissionContextTests::AddNewTab(const GURL& url) {
  std::unique_ptr<content::WebContents> new_tab = CreateTestWebContents();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(new_tab.get(),
                                                             url);
  SetupRequestManager(new_tab.get());

  extra_tabs_.push_back(std::move(new_tab));
}

void GeolocationPermissionContextTests::CheckTabContentsState(
    const GURL& requesting_frame,
    ContentSetting expected_content_setting) {
  auto* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  EXPECT_TRUE(
      expected_content_setting == CONTENT_SETTING_BLOCK
          ? content_settings->IsContentBlocked(ContentSettingsType::GEOLOCATION)
          : content_settings->IsContentAllowed(
                ContentSettingsType::GEOLOCATION));
}

std::unique_ptr<content::BrowserContext>
GeolocationPermissionContextTests::CreateBrowserContext() {
  std::unique_ptr<content::TestBrowserContext> test_browser_contest =
      std::make_unique<content::TestBrowserContext>();
  test_browser_contest->SetPermissionControllerDelegate(
      permissions::GetPermissionControllerDelegate(test_browser_contest.get()));
  return test_browser_contest;
}

void GeolocationPermissionContextTests::SetUp() {
  RenderViewHostTestHarness::SetUp();

  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<
          content_settings::TestPageSpecificContentSettingsDelegate>(
          /*prefs=*/nullptr,
          PermissionsClient::Get()->GetSettingsMap(browser_context())));

  auto delegate = std::make_unique<TestGeolocationPermissionContextDelegate>(
      browser_context());
  delegate_ = delegate.get();

#if BUILDFLAG(IS_ANDROID)
  auto context = std::make_unique<GeolocationPermissionContextAndroid>(
      browser_context(), std::move(delegate), /*is_regular_profile=*/false,
      std::make_unique<MockLocationSettings>());
  MockLocationSettings::SetLocationStatus(
      /*has_android_coarse_location_permission=*/true,
      /*has_android_fine_location_permission=*/true,
      /*is_system_location_setting_enabled=*/true);
  MockLocationSettings::SetCanPromptForAndroidPermission(true);
  MockLocationSettings::SetLocationSettingsDialogStatus(false /* enabled */,
                                                        GRANTED);
  MockLocationSettings::ClearHasShownLocationSettingsDialog();
#elif BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  auto fake_geolocation_system_permission_manager =
      std::make_unique<device::FakeGeolocationSystemPermissionManager>();
  fake_geolocation_system_permission_manager_ =
      fake_geolocation_system_permission_manager.get();
  fake_geolocation_system_permission_manager->SetSystemPermission(
      device::LocationSystemPermissionStatus::kAllowed);
  device::FakeGeolocationSystemPermissionManager::SetInstance(
      std::move(fake_geolocation_system_permission_manager));
  auto context = std::make_unique<GeolocationPermissionContextSystem>(
      browser_context(), std::move(delegate));
#else
  auto context = std::make_unique<GeolocationPermissionContext>(
      browser_context(), std::move(delegate));
#endif
  SetupRequestManager(web_contents());

  geolocation_permission_context_ = context.get();

  PermissionManager* permission_manager = static_cast<PermissionManager*>(
      browser_context()->GetPermissionControllerDelegate());

  permission_manager
      ->PermissionContextsForTesting()[ContentSettingsType::GEOLOCATION] =
      std::move(context);
}

void GeolocationPermissionContextTests::TearDown() {
  mock_permission_prompt_factories_.clear();
  extra_tabs_.clear();
  DeleteContents();
  RenderViewHostTestHarness::TearDown();
}

void GeolocationPermissionContextTests::SetupRequestManager(
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

#if BUILDFLAG(IS_ANDROID)

bool GeolocationPermissionContextTests::RequestPermissionIsLSDShown(
    const GURL& origin) {
  NavigateAndCommit(origin);
  RequestManagerDocumentLoadCompleted();
  MockLocationSettings::ClearHasShownLocationSettingsDialog();
  RequestGeolocationPermission(RequestID(0), origin, true);

  return MockLocationSettings::HasShownLocationSettingsDialog();
}

bool GeolocationPermissionContextTests::
    RequestPermissionIsLSDShownWithPermissionPrompt(const GURL& origin) {
  NavigateAndCommit(origin);
  RequestManagerDocumentLoadCompleted();
  MockLocationSettings::ClearHasShownLocationSettingsDialog();
  RequestGeolocationPermission(RequestID(0), origin, true);

  EXPECT_TRUE(HasActivePrompt());
  AcceptPrompt();

  return MockLocationSettings::HasShownLocationSettingsDialog();
}

void GeolocationPermissionContextTests::AddDayOffsetForTesting(int days) {
  GeolocationPermissionContextAndroid::AddDayOffsetForTesting(days);
}
#endif

void GeolocationPermissionContextTests::RequestManagerDocumentLoadCompleted() {
  GeolocationPermissionContextTests::RequestManagerDocumentLoadCompleted(
      web_contents());
}

void GeolocationPermissionContextTests::RequestManagerDocumentLoadCompleted(
    content::WebContents* web_contents) {
  PermissionRequestManager::FromWebContents(web_contents)
      ->DocumentOnLoadCompletedInPrimaryMainFrame();
}

ContentSetting GeolocationPermissionContextTests::GetGeolocationContentSetting(
    GURL frame_0,
    GURL frame_1) {
  return PermissionsClient::Get()
      ->GetSettingsMap(browser_context())
      ->GetContentSetting(frame_0, frame_1, ContentSettingsType::GEOLOCATION);
}

void GeolocationPermissionContextTests::SetGeolocationContentSetting(
    GURL frame_0,
    GURL frame_1,
    ContentSetting content_setting) {
  return PermissionsClient::Get()
      ->GetSettingsMap(browser_context())
      ->SetContentSettingDefaultScope(
          frame_0, frame_1, ContentSettingsType::GEOLOCATION, content_setting);
}

bool GeolocationPermissionContextTests::HasActivePrompt() {
  return HasActivePrompt(web_contents());
}

bool GeolocationPermissionContextTests::HasActivePrompt(
    content::WebContents* web_contents) {
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents);
  return manager->IsRequestInProgress();
}

void GeolocationPermissionContextTests::AcceptPrompt() {
  return AcceptPrompt(web_contents());
}

void GeolocationPermissionContextTests::AcceptPrompt(
    content::WebContents* web_contents) {
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents);
  manager->Accept();
  base::RunLoop().RunUntilIdle();
}

void GeolocationPermissionContextTests::AcceptPromptThisTime() {
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents());
  manager->AcceptThisTime();
  base::RunLoop().RunUntilIdle();
}

void GeolocationPermissionContextTests::DenyPrompt() {
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents());
  manager->Deny();
  base::RunLoop().RunUntilIdle();
}

void GeolocationPermissionContextTests::ClosePrompt() {
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents());
  manager->Dismiss();
  base::RunLoop().RunUntilIdle();
}

std::u16string GeolocationPermissionContextTests::GetPromptText() {
  PermissionRequestManager* manager =
      PermissionRequestManager::FromWebContents(web_contents());
  PermissionRequest* request = manager->Requests().front();
#if BUILDFLAG(IS_ANDROID)
  return request
      ->GetDialogAnnotatedMessageText(
          /*embedding_origin=*/request->requesting_origin())
      .text;
#else
  return base::ASCIIToUTF16(request->requesting_origin().spec()) +
         request->GetMessageTextFragment();
#endif
}

// Tests ----------------------------------------------------------------------

TEST_F(GeolocationPermissionContextTests, SinglePermissionPrompt) {
  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();

  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(RequestID(0), requesting_frame, true);
  ASSERT_TRUE(HasActivePrompt());
}

TEST_F(GeolocationPermissionContextTests,
       SinglePermissionPromptFailsOnInsecureOrigin) {
  GURL requesting_frame("http://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();

  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(RequestID(0), requesting_frame, true);
  ASSERT_FALSE(HasActivePrompt());
}

#if BUILDFLAG(IS_ANDROID)
// Tests concerning Android location settings permission
TEST_F(GeolocationPermissionContextTests, GeolocationEnabledDisabled) {
  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  base::HistogramTester histograms;
  MockLocationSettings::SetLocationStatus(
      /*has_android_coarse_location_permission=*/true,
      /*has_android_fine_location_permission=*/true,
      /*is_system_location_setting_enabled=*/true);
  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(RequestID(0), requesting_frame, true);
  EXPECT_TRUE(HasActivePrompt());
  histograms.ExpectTotalCount("Permissions.Action.Geolocation", 0);

  content::NavigationSimulator::Reload(web_contents());
  histograms.ExpectUniqueSample("Permissions.Action.Geolocation",
                                static_cast<int>(PermissionAction::IGNORED), 1);
  MockLocationSettings::SetLocationStatus(
      /*has_android_coarse_location_permission=*/false,
      /*has_android_fine_location_permission=*/false,
      /*is_system_location_setting_enabled=*/true);
  MockLocationSettings::SetCanPromptForAndroidPermission(false);
  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(RequestID(0), requesting_frame, true);
  histograms.ExpectUniqueSample("Permissions.Action.Geolocation",
                                static_cast<int>(PermissionAction::IGNORED), 1);
  EXPECT_FALSE(HasActivePrompt());
}

TEST_F(GeolocationPermissionContextTests, AndroidEnabledCanPromptAndAccept) {
  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  MockLocationSettings::SetLocationStatus(
      /*has_android_coarse_location_permission=*/false,
      /*has_android_fine_location_permission=*/false,
      /*is_system_location_setting_enabled=*/true);
  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(RequestID(0), requesting_frame, true);
  ASSERT_TRUE(HasActivePrompt());
  base::HistogramTester histograms;
  AcceptPrompt();
  histograms.ExpectUniqueSample("Permissions.Action.Geolocation",
                                static_cast<int>(PermissionAction::GRANTED), 1);
  CheckTabContentsState(requesting_frame, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);
}

TEST_F(GeolocationPermissionContextTests,
       AndroidEnabledCanPromptAndAcceptThisTime) {
  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  MockLocationSettings::SetLocationStatus(
      /*has_android_coarse_location_permission=*/false,
      /*has_android_fine_location_permission=*/false,
      /*is_system_location_setting_enabled=*/true);
  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(RequestID(0), requesting_frame, true);
  ASSERT_TRUE(HasActivePrompt());
  base::HistogramTester histograms;
  AcceptPromptThisTime();

  histograms.ExpectUniqueSample(
      "Permissions.Action.Geolocation",
      static_cast<int>(PermissionAction::GRANTED_ONCE), 1);
  CheckTabContentsState(requesting_frame, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);
}

TEST_F(GeolocationPermissionContextTests, AndroidEnabledCantPrompt) {
  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  MockLocationSettings::SetLocationStatus(
      /*has_android_coarse_location_permission=*/false,
      /*has_android_fine_location_permission=*/false,
      /*is_system_location_setting_enabled=*/true);
  MockLocationSettings::SetCanPromptForAndroidPermission(false);
  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(RequestID(0), requesting_frame, true);
  EXPECT_FALSE(HasActivePrompt());
}

TEST_F(GeolocationPermissionContextTests, SystemLocationOffLSDDisabled) {
  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  MockLocationSettings::SetLocationStatus(
      /*has_android_coarse_location_permission=*/true,
      /*has_android_fine_location_permission=*/true,
      /*is_system_location_setting_enabled=*/false);
  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(RequestID(0), requesting_frame, true);
  EXPECT_FALSE(HasActivePrompt());
  EXPECT_FALSE(MockLocationSettings::HasShownLocationSettingsDialog());
}

TEST_F(GeolocationPermissionContextTests, SystemLocationOnNoLSD) {
  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(RequestID(0), requesting_frame, true);
  ASSERT_TRUE(HasActivePrompt());
  AcceptPrompt();
  CheckTabContentsState(requesting_frame, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);
  EXPECT_FALSE(MockLocationSettings::HasShownLocationSettingsDialog());
}

TEST_F(GeolocationPermissionContextTests, SystemLocationOffLSDAccept) {
  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  MockLocationSettings::SetLocationStatus(
      /*has_android_coarse_location_permission=*/true,
      /*has_android_fine_location_permission=*/true,
      /*is_system_location_setting_enabled=*/false);
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        GRANTED);
  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(RequestID(0), requesting_frame, true);
  ASSERT_TRUE(HasActivePrompt());
  AcceptPrompt();
  CheckTabContentsState(requesting_frame, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);
  EXPECT_TRUE(MockLocationSettings::HasShownLocationSettingsDialog());
}

TEST_F(GeolocationPermissionContextTests, SystemLocationOffLSDReject) {
  GURL requesting_frame("https://www.example.com/geolocation");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();
  MockLocationSettings::SetLocationStatus(
      /*has_android_coarse_location_permission=*/true,
      /*has_android_fine_location_permission=*/true,
      /*is_system_location_setting_enabled=*/false);
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        DENIED);
  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(RequestID(0), requesting_frame, true);
  ASSERT_TRUE(HasActivePrompt());
  AcceptPrompt();
  CheckTabContentsState(requesting_frame, CONTENT_SETTING_BLOCK);
  CheckPermissionMessageSent(0, false);
  EXPECT_TRUE(MockLocationSettings::HasShownLocationSettingsDialog());
}

TEST_F(GeolocationPermissionContextTests, LSDBackOffDifferentSites) {
  GURL requesting_frame_1("https://www.example.com/geolocation");
  GURL requesting_frame_2("https://www.example-2.com/geolocation");
  GURL requesting_frame_dse("https://www.dse.com/geolocation");

  delegate_->SetDSEOriginForTesting(url::Origin::Create(requesting_frame_dse));

  // Set all origin geolocation permissions to ALLOW.
  SetGeolocationContentSetting(requesting_frame_1, requesting_frame_1,
                               CONTENT_SETTING_ALLOW);
  SetGeolocationContentSetting(requesting_frame_2, requesting_frame_2,
                               CONTENT_SETTING_ALLOW);
  SetGeolocationContentSetting(requesting_frame_dse, requesting_frame_dse,
                               CONTENT_SETTING_ALLOW);

  // Turn off system location but allow the LSD to be shown, and denied.
  MockLocationSettings::SetLocationStatus(
      /*has_android_coarse_location_permission=*/true,
      /*has_android_fine_location_permission=*/true,
      /*is_system_location_setting_enabled=*/false);
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        DENIED);

  // Now permission requests should trigger the LSD, but the LSD will be denied,
  // putting the requesting origins into backoff. Check that the two non-DSE
  // origins share the same backoff, which is distinct to the DSE origin.
  // First, cancel a LSD prompt on the first non-DSE origin to go into backoff.
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame_1));

  // Now check that the LSD is prevented on this origin.
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame_1));

  // Now ask on the other non-DSE origin and check backoff prevented the prompt.
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame_2));

  // Now request on the DSE and check that the LSD is shown, as the non-DSE
  // backoff should not apply.
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame_dse));

  // Now check that the DSE is in backoff.
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame_dse));
}

TEST_F(GeolocationPermissionContextTests, LSDBackOffTiming) {
  GURL requesting_frame("https://www.example.com/geolocation");
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ALLOW);

  // Turn off system location but allow the LSD to be shown, and denied.
  MockLocationSettings::SetLocationStatus(
      /*has_android_coarse_location_permission=*/true,
      /*has_android_fine_location_permission=*/true,
      /*is_system_location_setting_enabled=*/false);
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        DENIED);

  // First, cancel a LSD prompt on the first non-DSE origin to go into backoff.
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));

  // Check the LSD is prevented in 6 days time.
  AddDayOffsetForTesting(6);
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));

  // Check it is shown in one more days time, but then not straight after..
  AddDayOffsetForTesting(1);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));

  // Check that it isn't shown 29 days after that.
  AddDayOffsetForTesting(29);
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));

  // Check it is shown in one more days time, but then not straight after..
  AddDayOffsetForTesting(1);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));

  // Check that it isn't shown 89 days after that.
  AddDayOffsetForTesting(89);
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));

  // Check it is shown in one more days time, but then not straight after..
  AddDayOffsetForTesting(1);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));

  // Check that it isn't shown 89 days after that.
  AddDayOffsetForTesting(89);
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));

  // Check it is shown in one more days time, but then not straight after..
  AddDayOffsetForTesting(1);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));
}

TEST_F(GeolocationPermissionContextTests, LSDBackOffPermissionStatus) {
  GURL requesting_frame("https://www.example.com/geolocation");
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ALLOW);

  // Turn off system location but allow the LSD to be shown, and denied.
  MockLocationSettings::SetLocationStatus(
      /*has_android_coarse_location_permission=*/true,
      /*has_android_fine_location_permission=*/true,
      /*is_system_location_setting_enabled=*/false);
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        DENIED);

  // The permission status should reflect that the LSD will be shown.
  ASSERT_EQ(PermissionStatus::ASK,
            GetPermissionStatus(blink::PermissionType::GEOLOCATION,
                                requesting_frame));
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));

  // Now that the LSD is in backoff, the permission status should reflect it.
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));
  ASSERT_EQ(PermissionStatus::DENIED,
            GetPermissionStatus(blink::PermissionType::GEOLOCATION,
                                requesting_frame));
}

TEST_F(GeolocationPermissionContextTests, LSDBackOffAskPromptsDespiteBackOff) {
  GURL requesting_frame("https://www.example.com/geolocation");
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ALLOW);

  // Turn off system location but allow the LSD to be shown, and denied.
  MockLocationSettings::SetLocationStatus(
      /*has_android_coarse_location_permission=*/true,
      /*has_android_fine_location_permission=*/true,
      /*is_system_location_setting_enabled=*/false);
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        DENIED);

  // First, cancel a LSD prompt on the first non-DSE origin to go into backoff.
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));

  // Set the content setting back to ASK. The permission status should be
  // prompt, and the LSD prompt should now be shown.
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ASK);
  ASSERT_EQ(PermissionStatus::ASK,
            GetPermissionStatus(blink::PermissionType::GEOLOCATION,
                                requesting_frame));
  EXPECT_TRUE(
      RequestPermissionIsLSDShownWithPermissionPrompt(requesting_frame));
}

TEST_F(GeolocationPermissionContextTests,
       LSDBackOffAcceptPermissionResetsBackOff) {
  GURL requesting_frame("https://www.example.com/geolocation");
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ALLOW);

  // Turn off system location but allow the LSD to be shown, and denied.
  MockLocationSettings::SetLocationStatus(
      /*has_android_coarse_location_permission=*/true,
      /*has_android_fine_location_permission=*/true,
      /*is_system_location_setting_enabled=*/false);
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        DENIED);

  // First, get into the highest backoff state.
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
  AddDayOffsetForTesting(7);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
  AddDayOffsetForTesting(30);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
  AddDayOffsetForTesting(90);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));

  // Now accept a permissions prompt.
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ASK);
  EXPECT_TRUE(
      RequestPermissionIsLSDShownWithPermissionPrompt(requesting_frame));

  // Denying the LSD stops the content setting from being stored, so explicitly
  // set it to ALLOW.
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ALLOW);

  // And check that back in the lowest backoff state.
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));
  AddDayOffsetForTesting(7);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
}

TEST_F(GeolocationPermissionContextTests, LSDBackOffAcceptLSDResetsBackOff) {
  GURL requesting_frame("https://www.example.com/geolocation");
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ALLOW);

  // Turn off system location but allow the LSD to be shown, and denied.
  MockLocationSettings::SetLocationStatus(
      /*has_android_coarse_location_permission=*/true,
      /*has_android_fine_location_permission=*/true,
      /*is_system_location_setting_enabled=*/false);
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        DENIED);

  // First, get into the highest backoff state.
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
  AddDayOffsetForTesting(7);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
  AddDayOffsetForTesting(30);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));

  // Now accept the LSD.
  AddDayOffsetForTesting(90);
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        GRANTED);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));

  // Check that not in backoff, and that at the lowest backoff state.
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        DENIED);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
  EXPECT_FALSE(RequestPermissionIsLSDShown(requesting_frame));
  AddDayOffsetForTesting(7);
  EXPECT_TRUE(RequestPermissionIsLSDShown(requesting_frame));
}

#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(GeolocationPermissionContextTests, HashIsIgnored) {
  GURL url_a("https://www.example.com/geolocation#a");
  GURL url_b("https://www.example.com/geolocation#b");

  // Navigate to the first url.
  NavigateAndCommit(url_a);
  RequestManagerDocumentLoadCompleted();

  // Check permission is requested.
  ASSERT_FALSE(HasActivePrompt());
  const bool user_gesture = true;
  RequestGeolocationPermission(RequestID(0), url_a, user_gesture);
  ASSERT_TRUE(HasActivePrompt());

  // Change the hash, we'll still be on the same page.
  NavigateAndCommit(url_b);
  RequestManagerDocumentLoadCompleted();

  // Accept.
  AcceptPrompt();
  CheckTabContentsState(url_a, CONTENT_SETTING_ALLOW);
  CheckTabContentsState(url_b, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);
}

TEST_F(GeolocationPermissionContextTests, DISABLED_PermissionForFileScheme) {
  // TODO(felt): The bubble is rejecting file:// permission requests.
  // Fix and enable this test. crbug.com/444047
  GURL requesting_frame("file://example/geolocation.html");
  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();

  // Check permission is requested.
  ASSERT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(RequestID(0), requesting_frame, true);
  EXPECT_TRUE(HasActivePrompt());

  // Accept the frame.
  AcceptPrompt();
  CheckTabContentsState(requesting_frame, CONTENT_SETTING_ALLOW);
  CheckPermissionMessageSent(0, true);

  // Make sure the setting is not stored.
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetGeolocationContentSetting(requesting_frame, requesting_frame));
}

TEST_F(GeolocationPermissionContextTests, CancelGeolocationPermissionRequest) {
  GURL frame_0("https://www.example.com/geolocation");
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetGeolocationContentSetting(frame_0, frame_0));

  NavigateAndCommit(frame_0);
  RequestManagerDocumentLoadCompleted();

  ASSERT_FALSE(HasActivePrompt());

  RequestGeolocationPermission(RequestID(0), frame_0, true);

  ASSERT_TRUE(HasActivePrompt());
  std::u16string text_0 = GetPromptText();
  ASSERT_FALSE(text_0.empty());

  // Simulate the frame going away; the request should be removed.
  ClosePrompt();

  // Ensure permission isn't persisted.
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetGeolocationContentSetting(frame_0, frame_0));
}

TEST_F(GeolocationPermissionContextTests, InvalidURL) {
  // Navigate to the first url.
  GURL invalid_embedder("about:blank");
  GURL requesting_frame;
  NavigateAndCommit(invalid_embedder);
  RequestManagerDocumentLoadCompleted();

  // Nothing should be displayed.
  EXPECT_FALSE(HasActivePrompt());
  RequestGeolocationPermission(RequestID(0), requesting_frame, true);
  EXPECT_FALSE(HasActivePrompt());
  CheckPermissionMessageSent(0, false);
}

TEST_F(GeolocationPermissionContextTests, SameOriginMultipleTabs) {
  GURL url_a("https://www.example.com/geolocation");
  GURL url_b("https://www.example-2.com/geolocation");
  NavigateAndCommit(url_a);  // Tab A0
  AddNewTab(url_b);          // Tab B (extra_tabs_[0])
  AddNewTab(url_a);          // Tab A1 (extra_tabs_[1])
  RequestManagerDocumentLoadCompleted();
  RequestManagerDocumentLoadCompleted(extra_tabs_[0].get());
  RequestManagerDocumentLoadCompleted(extra_tabs_[1].get());

  // Request permission in all three tabs.
  RequestGeolocationPermission(RequestID(0), url_a, true);
  RequestGeolocationPermission(RequestIDForTab(0, 0), url_b, true);
  RequestGeolocationPermission(RequestIDForTab(1, 0), url_a, true);
  ASSERT_TRUE(HasActivePrompt());  // For A0.
  ASSERT_TRUE(HasActivePrompt(extra_tabs_[0].get()));
  ASSERT_TRUE(HasActivePrompt(extra_tabs_[1].get()));

  // Accept the permission in tab A0.
  AcceptPrompt();
  CheckPermissionMessageSent(0, true);
  // Because they're the same origin, this should cause tab A1's prompt to
  // disappear, but it doesn't: crbug.com/443013.
  // TODO(felt): Update this test when the bubble's behavior is changed.
  // Either way, tab B should still have a pending permission request.
  ASSERT_TRUE(HasActivePrompt(extra_tabs_[0].get()));
  ASSERT_TRUE(HasActivePrompt(extra_tabs_[1].get()));
}

TEST_F(GeolocationPermissionContextTests, TabDestroyed) {
  GURL requesting_frame("https://www.example.com/geolocation");
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetGeolocationContentSetting(requesting_frame, requesting_frame));

  NavigateAndCommit(requesting_frame);
  RequestManagerDocumentLoadCompleted();

  // Request permission for two frames.
  RequestGeolocationPermission(RequestID(0), requesting_frame, false);

  ASSERT_TRUE(HasActivePrompt());
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetGeolocationContentSetting(requesting_frame, requesting_frame));
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(GeolocationPermissionContextTests, GeolocationStatusAndroidDisabled) {
  GURL requesting_frame("https://www.example.com/geolocation");

  // With the Android permission off, but location allowed for a domain, the
  // permission status should be ASK.
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ALLOW);
  MockLocationSettings::SetLocationStatus(
      /*has_android_coarse_location_permission=*/false,
      /*has_android_fine_location_permission=*/false,
      /*is_system_location_setting_enabled=*/true);
  ASSERT_EQ(PermissionStatus::ASK,
            GetPermissionStatus(blink::PermissionType::GEOLOCATION,
                                requesting_frame));

  // With the Android permission off, and location blocked for a domain, the
  // permission status should still be BLOCK.
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_BLOCK);
  ASSERT_EQ(PermissionStatus::DENIED,
            GetPermissionStatus(blink::PermissionType::GEOLOCATION,
                                requesting_frame));

  // With the Android permission off, and location prompt for a domain, the
  // permission status should still be ASK.
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ASK);
  ASSERT_EQ(PermissionStatus::ASK,
            GetPermissionStatus(blink::PermissionType::GEOLOCATION,
                                requesting_frame));
}

TEST_F(GeolocationPermissionContextTests, GeolocationStatusSystemDisabled) {
  GURL requesting_frame("https://www.example.com/geolocation");

  // With the system permission off, but location allowed for a domain, the
  // permission status should be reflect whether the LSD can be shown.
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ALLOW);
  MockLocationSettings::SetLocationStatus(
      /*has_android_coarse_location_permission=*/true,
      /*has_android_fine_location_permission=*/true,
      /*is_system_location_setting_enabled=*/false);
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        DENIED);
  ASSERT_EQ(PermissionStatus::ASK,
            GetPermissionStatus(blink::PermissionType::GEOLOCATION,
                                requesting_frame));

  MockLocationSettings::SetLocationSettingsDialogStatus(false /* enabled */,
                                                        GRANTED);
  ASSERT_EQ(PermissionStatus::DENIED,
            GetPermissionStatus(blink::PermissionType::GEOLOCATION,
                                requesting_frame));

  // The result should be the same if the location permission is ASK.
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ASK);
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        GRANTED);
  ASSERT_EQ(PermissionStatus::ASK,
            GetPermissionStatus(blink::PermissionType::GEOLOCATION,
                                requesting_frame));

  MockLocationSettings::SetLocationSettingsDialogStatus(false /* enabled */,
                                                        GRANTED);
  ASSERT_EQ(PermissionStatus::DENIED,
            GetPermissionStatus(blink::PermissionType::GEOLOCATION,
                                requesting_frame));

  // With the Android permission off, and location blocked for a domain, the
  // permission status should still be BLOCK.
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_BLOCK);
  MockLocationSettings::SetLocationSettingsDialogStatus(true /* enabled */,
                                                        GRANTED);
  ASSERT_EQ(PermissionStatus::DENIED,
            GetPermissionStatus(blink::PermissionType::GEOLOCATION,
                                requesting_frame));
}

struct PermissionStateTestEntry {
  bool has_coarse_location;
  bool has_fine_location;
  int bucket;
} kPermissionStateTestEntries[] = {
    {/*has_coarse_location=*/false, /*has_fine_location=*/false, /*bucket=*/0},
    {/*has_coarse_location=*/true, /*has_fine_location=*/false, /*bucket=*/1},
    {/*has_coarse_location=*/false, /*has_fine_location=*/true, /*bucket=*/2},
    {/*has_coarse_location=*/true, /*has_fine_location=*/true, /*bucket=*/2},
};

class GeolocationAndroidPermissionRegularProfileTest
    : public content::RenderViewHostTestHarness,
      public testing::WithParamInterface<PermissionStateTestEntry> {};

TEST_P(GeolocationAndroidPermissionRegularProfileTest, Histogram) {
  const auto& [has_coarse_location, has_fine_location, bucket] = GetParam();
  MockLocationSettings::SetLocationStatus(
      has_coarse_location, has_fine_location,
      /*is_system_location_setting_enabled=*/true);
  base::HistogramTester histogram_tester;
  GeolocationPermissionContextAndroid context(
      browser_context(), /*delegate=*/nullptr, /*is_regular_profile=*/true,
      std::make_unique<MockLocationSettings>());
  histogram_tester.ExpectUniqueSample(
      "Geolocation.Android.LocationPermissionState", bucket,
      /*expected_bucket_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(
    GeolocationAndroidPermissionRegularProfileTests,
    GeolocationAndroidPermissionRegularProfileTest,
    testing::ValuesIn(kPermissionStateTestEntries),
    [](const testing::TestParamInfo<PermissionStateTestEntry>& info) {
      return base::StringPrintf("Location%sFineLocation%s",
                                info.param.has_coarse_location ? "On" : "Off",
                                info.param.has_fine_location ? "On" : "Off");
    });

using GeolocationAndroidPermissionIrregularProfileTest =
    content::RenderViewHostTestHarness;

TEST_F(GeolocationAndroidPermissionIrregularProfileTest, DoesNotRecord) {
  base::HistogramTester histogram_tester;
  GeolocationPermissionContextAndroid context(
      browser_context(), /*delegate=*/nullptr, /*is_regular_profile=*/false,
      std::make_unique<MockLocationSettings>());
  histogram_tester.ExpectTotalCount(
      "Geolocation.Android.LocationPermissionState", /*expected_count=*/0);
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
TEST_F(GeolocationPermissionContextTests,
       AllSystemAndSitePermissionCombinations) {
  GURL requesting_frame("https://www.example.com/geolocation");

  const struct {
    const LocationSystemPermissionStatus system_permission;
    const ContentSetting site_permission;
    const PermissionStatus expected_effective_site_permission;
  } kTestCases[] = {
      {LocationSystemPermissionStatus(LocationSystemPermissionStatus::kDenied),
       ContentSetting(CONTENT_SETTING_ASK), PermissionStatus::ASK},
      {LocationSystemPermissionStatus(LocationSystemPermissionStatus::kDenied),
       ContentSetting(CONTENT_SETTING_BLOCK), PermissionStatus::DENIED},
      {LocationSystemPermissionStatus(LocationSystemPermissionStatus::kDenied),
       ContentSetting(CONTENT_SETTING_ALLOW), PermissionStatus::DENIED},
      {LocationSystemPermissionStatus(
           LocationSystemPermissionStatus::kNotDetermined),
       ContentSetting(CONTENT_SETTING_ASK), PermissionStatus::ASK},
      {LocationSystemPermissionStatus(
           LocationSystemPermissionStatus::kNotDetermined),
       ContentSetting(CONTENT_SETTING_BLOCK), PermissionStatus::DENIED},
      {LocationSystemPermissionStatus(
           LocationSystemPermissionStatus::kNotDetermined),
       ContentSetting(CONTENT_SETTING_ALLOW), PermissionStatus::ASK},
      {LocationSystemPermissionStatus(LocationSystemPermissionStatus::kAllowed),
       ContentSetting(CONTENT_SETTING_ASK), PermissionStatus::ASK},
      {LocationSystemPermissionStatus(LocationSystemPermissionStatus::kAllowed),
       ContentSetting(CONTENT_SETTING_BLOCK), PermissionStatus::DENIED},
      {LocationSystemPermissionStatus(LocationSystemPermissionStatus::kAllowed),
       ContentSetting(CONTENT_SETTING_ALLOW), PermissionStatus::GRANTED},
  };

  for (auto test_case : kTestCases) {
    SetGeolocationContentSetting(requesting_frame, requesting_frame,
                                 test_case.site_permission);
    fake_geolocation_system_permission_manager_->SetSystemPermission(
        test_case.system_permission);
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(test_case.expected_effective_site_permission,
              GetPermissionStatus(blink::PermissionType::GEOLOCATION,
                                  requesting_frame));
  }
}

TEST_F(GeolocationPermissionContextTests, SystemPermissionUpdates) {
  GURL requesting_frame("https://www.example.com/geolocation");
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromURLNoWildcard(requesting_frame);
  ContentSettingsPattern secondary_pattern = ContentSettingsPattern::Wildcard();

  geolocation_permission_context_->AddObserver(this);
  expected_primary_pattern_ = &primary_pattern;
  expected_secondary_pattern_ = &secondary_pattern;
  SetGeolocationContentSetting(requesting_frame, requesting_frame,
                               CONTENT_SETTING_ALLOW);
  ASSERT_EQ(1, num_permission_updates_);
  primary_pattern = ContentSettingsPattern::Wildcard();
  fake_geolocation_system_permission_manager_->SetSystemPermission(
      LocationSystemPermissionStatus::kDenied);
  fake_geolocation_system_permission_manager_->SetSystemPermission(
      LocationSystemPermissionStatus::kAllowed);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(3, num_permission_updates_);
  geolocation_permission_context_->RemoveObserver(this);
}
#endif  // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)

}  // namespace permissions
