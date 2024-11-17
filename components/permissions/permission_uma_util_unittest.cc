// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_uma_util.h"

#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_uma_util.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/constants.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/test/test_permissions_client.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_render_frame_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

namespace permissions {

namespace {

constexpr const char* kTopLevelUrl = "https://google.com";
constexpr const char* kSameOriginFrameUrl = "https://google.com/a/same.html";
constexpr const char* kCrossOriginFrameUrl = "https://embedded.google.com";
constexpr const char* kCrossOriginFrameUrl2 = "https://embedded2.google.com";

constexpr const char* kGeolocationUsageHistogramName =
    "Permissions.Experimental.Usage.Geolocation.IsCrossOriginFrame";
constexpr const char* kGeolocationPermissionsPolicyUsageHistogramName =
    "Permissions.Experimental.Usage.Geolocation.CrossOriginFrame."
    "TopLevelHeaderPolicy";
constexpr const char* kGeolocationPermissionsPolicyActionHistogramName =
    "Permissions.Action.Geolocation.CrossOriginFrame."
    "TopLevelHeaderPolicy";

blink::ParsedPermissionsPolicy CreatePermissionsPolicy(
    blink::mojom::PermissionsPolicyFeature feature,
    const std::vector<std::string>& origins,
    bool matches_all_origins = false) {
  std::vector<blink::OriginWithPossibleWildcards> allow_origins;
  for (const auto& origin : origins) {
    allow_origins.emplace_back(*blink::OriginWithPossibleWildcards::FromOrigin(
        url::Origin::Create(GURL(origin))));
  }
  return {{feature, allow_origins, /*self_if_matches=*/std::nullopt,
           matches_all_origins,
           /*matches_opaque_src*/ false}};
}

PermissionRequestManager* SetupRequestManager(
    content::WebContents* web_contents) {
  PermissionRequestManager::CreateForWebContents(web_contents);
  return PermissionRequestManager::FromWebContents(web_contents);
}

struct PermissionsDelegationTestConfig {
  ContentSettingsType type;
  PermissionAction action;
  std::optional<blink::mojom::PermissionsPolicyFeature> feature_overriden;

  bool matches_all_origins;
  std::vector<std::string> origins;

  // Expected resulting permissions policy configuration.
  std::optional<PermissionHeaderPolicyForUMA> expected_configuration;
};

#if !BUILDFLAG(IS_ANDROID)
ContentSettingsForOneType GetRevokedUnusedPermissions(
    HostContentSettingsMap* hcsm) {
  return hcsm->GetSettingsForOneType(
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS);
}
#endif

// Wrapper class so that we can pass a closure to the PermissionRequest
// ctor, to handle all dtor paths (avoid crash in dtor of WebContent)
class PermissionRequestWrapper {
 public:
  explicit PermissionRequestWrapper(permissions::RequestType type,
                                    const char* url) {
    const bool user_gesture = true;
    auto decided = [](ContentSetting, bool, bool) {};
    request_ = std::make_unique<permissions::PermissionRequest>(
        GURL(url), type, user_gesture, base::BindRepeating(decided),
        base::BindOnce(&PermissionRequestWrapper::DeleteThis,
                       base::Unretained(this)));
  }

  PermissionRequestWrapper(const PermissionRequestWrapper&) = delete;
  PermissionRequestWrapper& operator=(const PermissionRequestWrapper&) = delete;

  permissions::PermissionRequest* request() { return request_.get(); }

 private:
  void DeleteThis() { delete this; }

  std::unique_ptr<permissions::PermissionRequest> request_;
};

}  // namespace

class PermissionsDelegationUmaUtilTest
    : public content::RenderViewHostTestHarness,
      public testing::WithParamInterface<PermissionsDelegationTestConfig> {
 protected:
  void SetUp() override { RenderViewHostTestHarness::SetUp(); }

  content::RenderFrameHost* GetMainFrameAndNavigate(const char* origin) {
    content::RenderFrameHost* result = web_contents()->GetPrimaryMainFrame();
    content::RenderFrameHostTester::For(result)
        ->InitializeRenderFrameIfNeeded();
    SimulateNavigation(&result, GURL(origin));
    return result;
  }

  content::RenderFrameHost* AddChildFrameWithPermissionsPolicy(
      content::RenderFrameHost* parent,
      const char* origin,
      blink::ParsedPermissionsPolicy policy) {
    content::RenderFrameHost* result =
        content::RenderFrameHostTester::For(parent)->AppendChildWithPolicy(
            "", policy);
    content::RenderFrameHostTester::For(result)
        ->InitializeRenderFrameIfNeeded();
    SimulateNavigation(&result, GURL(origin));
    return result;
  }

  // The permissions policy is invariant and required the page to be
  // refreshed
  void RefreshAndSetPermissionsPolicy(content::RenderFrameHost** rfh,
                                      blink::ParsedPermissionsPolicy policy) {
    content::RenderFrameHost* current = *rfh;
    auto navigation = content::NavigationSimulator::CreateRendererInitiated(
        current->GetLastCommittedURL(), current);
    navigation->SetPermissionsPolicyHeader(policy);
    navigation->Commit();
    *rfh = navigation->GetFinalRenderFrameHost();
  }

  // Simulates navigation and returns the final RenderFrameHost.
  void SimulateNavigation(content::RenderFrameHost** rfh, const GURL& url) {
    auto navigation_simulator =
        content::NavigationSimulator::CreateRendererInitiated(url, *rfh);
    navigation_simulator->Commit();
    *rfh = navigation_simulator->GetFinalRenderFrameHost();
  }

 private:
  TestPermissionsClient permissions_client_;
};

class PermissionUmaUtilTest : public testing::Test {
 private:
  content::BrowserTaskEnvironment task_environment_;
  TestPermissionsClient permissions_client_;
};

TEST_F(PermissionUmaUtilTest, ScopedRevocationReporter) {
  content::TestBrowserContext browser_context;

  // TODO(tsergeant): Add more comprehensive tests of PermissionUmaUtil.
  base::HistogramTester histograms;
  HostContentSettingsMap* map =
      PermissionsClient::Get()->GetSettingsMap(&browser_context);
  GURL host("https://example.com");
  ContentSettingsPattern host_pattern =
      ContentSettingsPattern::FromURLNoWildcard(host);
  ContentSettingsPattern host_containing_wildcards_pattern =
      ContentSettingsPattern::FromString("https://[*.]example.com/");
  ContentSettingsType type = ContentSettingsType::GEOLOCATION;
  PermissionSourceUI source_ui = PermissionSourceUI::SITE_SETTINGS;

  // Allow->Block triggers a revocation.
  map->SetContentSettingDefaultScope(host, host, type, CONTENT_SETTING_ALLOW);
  {
    PermissionUmaUtil::ScopedRevocationReporter scoped_revocation_reporter(
        &browser_context, host, host, type, source_ui);
    map->SetContentSettingDefaultScope(host, host, type, CONTENT_SETTING_BLOCK);
  }
  histograms.ExpectBucketCount("Permissions.Action.Geolocation",
                               static_cast<int>(PermissionAction::REVOKED), 1);

  // Block->Allow does not trigger a revocation.
  {
    PermissionUmaUtil::ScopedRevocationReporter scoped_revocation_reporter(
        &browser_context, host, host, type, source_ui);
    map->SetContentSettingDefaultScope(host, host, type, CONTENT_SETTING_ALLOW);
  }
  histograms.ExpectBucketCount("Permissions.Action.Geolocation",
                               static_cast<int>(PermissionAction::REVOKED), 1);

  // Allow->Default triggers a revocation when default is 'ask'.
  map->SetDefaultContentSetting(type, CONTENT_SETTING_ASK);
  {
    PermissionUmaUtil::ScopedRevocationReporter scoped_revocation_reporter(
        &browser_context, host, host, type, source_ui);
    map->SetContentSettingDefaultScope(host, host, type,
                                       CONTENT_SETTING_DEFAULT);
  }
  histograms.ExpectBucketCount("Permissions.Action.Geolocation",
                               static_cast<int>(PermissionAction::REVOKED), 2);

  // Allow->Default does not trigger a revocation when default is 'allow'.
  map->SetDefaultContentSetting(type, CONTENT_SETTING_ALLOW);
  {
    PermissionUmaUtil::ScopedRevocationReporter scoped_revocation_reporter(
        &browser_context, host, host, type, source_ui);
    map->SetContentSettingDefaultScope(host, host, type,
                                       CONTENT_SETTING_DEFAULT);
  }
  histograms.ExpectBucketCount("Permissions.Action.Geolocation",
                               static_cast<int>(PermissionAction::REVOKED), 2);

  // Allow->Block with url pattern string triggers a revocation.
  map->SetContentSettingDefaultScope(host, host, type, CONTENT_SETTING_ALLOW);
  {
    PermissionUmaUtil::ScopedRevocationReporter scoped_revocation_reporter(
        &browser_context, host_pattern, host_pattern, type, source_ui);
    map->SetContentSettingCustomScope(host_pattern,
                                      ContentSettingsPattern::Wildcard(), type,
                                      CONTENT_SETTING_BLOCK);
  }
  histograms.ExpectBucketCount("Permissions.Action.Geolocation",
                               static_cast<int>(PermissionAction::REVOKED), 3);

  // Allow->Block with non url pattern string does not trigger a revocation.
  map->SetContentSettingDefaultScope(host, host, type, CONTENT_SETTING_ALLOW);
  {
    PermissionUmaUtil::ScopedRevocationReporter scoped_revocation_reporter(
        &browser_context, host_containing_wildcards_pattern, host_pattern, type,
        source_ui);
    map->SetContentSettingCustomScope(host_containing_wildcards_pattern,
                                      ContentSettingsPattern::Wildcard(), type,
                                      CONTENT_SETTING_BLOCK);
  }
  histograms.ExpectBucketCount("Permissions.Action.Geolocation",
                               static_cast<int>(PermissionAction::REVOKED), 3);
}

TEST_F(PermissionUmaUtilTest, CrowdDenyVersionTest) {
  base::HistogramTester histograms;

  const std::optional<base::Version> empty_version;
  PermissionUmaUtil::RecordCrowdDenyVersionAtAbuseCheckTime(empty_version);
  histograms.ExpectBucketCount(
      "Permissions.CrowdDeny.PreloadData.VersionAtAbuseCheckTime", 0, 1);

  const std::optional<base::Version> valid_version =
      base::Version({2020, 10, 11, 1234});
  PermissionUmaUtil::RecordCrowdDenyVersionAtAbuseCheckTime(valid_version);
  histograms.ExpectBucketCount(
      "Permissions.CrowdDeny.PreloadData.VersionAtAbuseCheckTime", 20201011, 1);

  const std::optional<base::Version> valid_old_version =
      base::Version({2019, 10, 10, 1234});
  PermissionUmaUtil::RecordCrowdDenyVersionAtAbuseCheckTime(valid_old_version);
  histograms.ExpectBucketCount(
      "Permissions.CrowdDeny.PreloadData.VersionAtAbuseCheckTime", 1, 1);

  const std::optional<base::Version> valid_future_version =
      base::Version({2021, 1, 1, 1234});
  PermissionUmaUtil::RecordCrowdDenyVersionAtAbuseCheckTime(
      valid_future_version);
  histograms.ExpectBucketCount(
      "Permissions.CrowdDeny.PreloadData.VersionAtAbuseCheckTime", 20210101, 1);

  const std::optional<base::Version> invalid_version =
      base::Version({2020, 10, 11});
  PermissionUmaUtil::RecordCrowdDenyVersionAtAbuseCheckTime(valid_version);
  histograms.ExpectBucketCount(
      "Permissions.CrowdDeny.PreloadData.VersionAtAbuseCheckTime", 1, 1);
}

// Test that the appropriate UMA metrics have been recorded when the DSE is
// disabled.
TEST_F(PermissionUmaUtilTest, MetricsAreRecordedWhenAutoDSEPermissionReverted) {
  const std::string kTransitionHistogramPrefix =
      "Permissions.DSE.AutoPermissionRevertTransition.";

  constexpr struct {
    ContentSetting backed_up_setting;
    ContentSetting effective_setting;
    ContentSetting end_state_setting;
    permissions::AutoDSEPermissionRevertTransition expected_transition;
  } kTests[] = {
      // Expected valid combinations.
      {CONTENT_SETTING_ASK, CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK,
       permissions::AutoDSEPermissionRevertTransition::NO_DECISION_ASK},
      {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW,
       permissions::AutoDSEPermissionRevertTransition::PRESERVE_ALLOW},
      {CONTENT_SETTING_BLOCK, CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK,
       permissions::AutoDSEPermissionRevertTransition::CONFLICT_ASK},
      {CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK, CONTENT_SETTING_BLOCK,
       permissions::AutoDSEPermissionRevertTransition::PRESERVE_BLOCK_ASK},
      {CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK, CONTENT_SETTING_BLOCK,
       permissions::AutoDSEPermissionRevertTransition::PRESERVE_BLOCK_ALLOW},
      {CONTENT_SETTING_BLOCK, CONTENT_SETTING_BLOCK, CONTENT_SETTING_BLOCK,
       permissions::AutoDSEPermissionRevertTransition::PRESERVE_BLOCK_BLOCK},
  };

  // We test every combination of test case for notifications and geolocation to
  // basically test the entire possible transition space.
  for (const auto& test : kTests) {
    for (const auto type : {ContentSettingsType::NOTIFICATIONS,
                            ContentSettingsType::GEOLOCATION}) {
      const std::string type_string = type == ContentSettingsType::NOTIFICATIONS
                                          ? "Notifications"
                                          : "Geolocation";
      base::HistogramTester histograms;
      PermissionUmaUtil::RecordAutoDSEPermissionReverted(
          type, test.backed_up_setting, test.effective_setting,
          test.end_state_setting);

      // Test that the expected samples are recorded in histograms.
      histograms.ExpectBucketCount(kTransitionHistogramPrefix + type_string,
                                   test.expected_transition, 1);
      histograms.ExpectTotalCount(kTransitionHistogramPrefix + type_string, 1);
    }
  }
}

TEST_F(PermissionsDelegationUmaUtilTest, UsageAndPromptInTopLevelFrame) {
  base::HistogramTester histograms;
  auto* main_frame = GetMainFrameAndNavigate(kTopLevelUrl);
  histograms.ExpectTotalCount(kGeolocationUsageHistogramName, 0);

  auto* permission_request_manager = SetupRequestManager(web_contents());
  PermissionRequestWrapper* request_owner =
      new PermissionRequestWrapper(RequestType::kGeolocation, kTopLevelUrl);
  permission_request_manager->AddRequest(main_frame, request_owner->request());
  PermissionUmaUtil::RecordPermissionsUsageSourceAndPolicyConfiguration(
      ContentSettingsType::GEOLOCATION, main_frame);
  EXPECT_THAT(histograms.GetAllSamples(kGeolocationUsageHistogramName),
              testing::ElementsAre(base::Bucket(0, 1)));
  PermissionUmaUtil::PermissionPromptResolved(
      {request_owner->request()}, web_contents(), PermissionAction::GRANTED,
      /*time_to_decision*/ base::TimeDelta(),
      PermissionPromptDisposition::NOT_APPLICABLE,
      /* ui_reason*/ std::nullopt,
      /*variants*/ {},
      /*predicted_grant_likelihood*/ std::nullopt,
      /*prediction_decision_held_back*/ std::nullopt,
      /*ignored_reason*/ std::nullopt, /*did_show_prompt*/ false,
      /*did_click_managed*/ false,
      /*did_click_learn_more*/ false);
  histograms.ExpectTotalCount(kGeolocationPermissionsPolicyActionHistogramName,
                              0);
}

TEST_F(PermissionUmaUtilTest, LhsIndicatorsShowTest) {
  base::HistogramTester histograms;

  PermissionUmaUtil::RecordActivityIndicator(
      {ContentSettingsType::MEDIASTREAM_CAMERA},
      /*blocked=*/false,
      /*blocked_system_level=*/false,
      /*click=*/false);
  histograms.ExpectBucketCount(
      "Permissions.ActivityIndicator.LHS.VideoCapture.Show",
      ActivityIndicatorState::kInUse, 1);

  PermissionUmaUtil::RecordActivityIndicator(
      {ContentSettingsType::MEDIASTREAM_CAMERA},
      /*blocked=*/true,
      /*blocked_system_level=*/false,
      /*click=*/false);
  histograms.ExpectBucketCount(
      "Permissions.ActivityIndicator.LHS.VideoCapture.Show",
      ActivityIndicatorState::kBlockedOnSiteLevel, 1);

  PermissionUmaUtil::RecordActivityIndicator(
      {ContentSettingsType::MEDIASTREAM_CAMERA},
      /*blocked=*/true,
      /*blocked_system_level=*/true,
      /*click=*/false);
  histograms.ExpectBucketCount(
      "Permissions.ActivityIndicator.LHS.VideoCapture.Show",
      ActivityIndicatorState::kBlockedOnSystemLevel, 1);

  PermissionUmaUtil::RecordActivityIndicator(
      {ContentSettingsType::MEDIASTREAM_MIC},
      /*blocked=*/false,
      /*blocked_system_level=*/false,
      /*click=*/false);
  histograms.ExpectBucketCount(
      "Permissions.ActivityIndicator.LHS.AudioCapture.Show",
      ActivityIndicatorState::kInUse, 1);

  PermissionUmaUtil::RecordActivityIndicator(
      {ContentSettingsType::MEDIASTREAM_MIC,
       ContentSettingsType::MEDIASTREAM_CAMERA},
      /*blocked=*/false,
      /*blocked_system_level=*/false,
      /*click=*/false);
  histograms.ExpectBucketCount(
      "Permissions.ActivityIndicator.LHS.AudioAndVideoCapture.Show",
      ActivityIndicatorState::kInUse, 1);
}

TEST_F(PermissionUmaUtilTest, LhsIndicatorsClickTest) {
  base::HistogramTester histograms;

  PermissionUmaUtil::RecordActivityIndicator(
      {ContentSettingsType::MEDIASTREAM_CAMERA},
      /*blocked=*/false,
      /*blocked_system_level=*/false,
      /*click=*/true);
  histograms.ExpectBucketCount(
      "Permissions.ActivityIndicator.LHS.VideoCapture.Click",
      ActivityIndicatorState::kInUse, 1);

  PermissionUmaUtil::RecordActivityIndicator(
      {ContentSettingsType::MEDIASTREAM_CAMERA},
      /*blocked=*/true,
      /*blocked_system_level=*/false,
      /*click=*/true);
  histograms.ExpectBucketCount(
      "Permissions.ActivityIndicator.LHS.VideoCapture.Click",
      ActivityIndicatorState::kBlockedOnSiteLevel, 1);

  PermissionUmaUtil::RecordActivityIndicator(
      {ContentSettingsType::MEDIASTREAM_CAMERA},
      /*blocked=*/true,
      /*blocked_system_level=*/true,
      /*click=*/true);
  histograms.ExpectBucketCount(
      "Permissions.ActivityIndicator.LHS.VideoCapture.Click",
      ActivityIndicatorState::kBlockedOnSystemLevel, 1);

  PermissionUmaUtil::RecordActivityIndicator(
      {ContentSettingsType::MEDIASTREAM_MIC},
      /*blocked=*/false,
      /*blocked_system_level=*/false,
      /*click=*/true);
  histograms.ExpectBucketCount(
      "Permissions.ActivityIndicator.LHS.AudioCapture.Click",
      ActivityIndicatorState::kInUse, 1);

  PermissionUmaUtil::RecordActivityIndicator(
      {ContentSettingsType::MEDIASTREAM_MIC,
       ContentSettingsType::MEDIASTREAM_CAMERA},
      /*blocked=*/false,
      /*blocked_system_level=*/false,
      /*click=*/true);
  histograms.ExpectBucketCount(
      "Permissions.ActivityIndicator.LHS.AudioAndVideoCapture.Click",
      ActivityIndicatorState::kInUse, 1);
}

TEST_F(PermissionUmaUtilTest, PageInfoPermissionReallowedTest) {
  base::HistogramTester histograms;

  PermissionUmaUtil::RecordPermissionRecoverySuccessRate(
      ContentSettingsType::MEDIASTREAM_CAMERA, /*is_used=*/true,
      /*show_infobar=*/true, /*page_reload=*/true);
  histograms.ExpectBucketCount(
      "Permissions.PageInfo.Changed.VideoCapture.Reallowed.Outcome",
      permissions::PermissionChangeInfo::kInfobarShownPageReloadPermissionUsed,
      1);

  PermissionUmaUtil::RecordPermissionRecoverySuccessRate(
      ContentSettingsType::MEDIASTREAM_CAMERA, /*is_used=*/false,
      /*show_infobar=*/true, /*page_reload=*/true);
  histograms.ExpectBucketCount(
      "Permissions.PageInfo.Changed.VideoCapture.Reallowed.Outcome",
      permissions::PermissionChangeInfo::
          kInfobarShownPageReloadPermissionNotUsed,
      1);

  PermissionUmaUtil::RecordPermissionRecoverySuccessRate(
      ContentSettingsType::MEDIASTREAM_CAMERA, /*is_used=*/true,
      /*show_infobar=*/true, /*page_reload=*/false);
  histograms.ExpectBucketCount(
      "Permissions.PageInfo.Changed.VideoCapture.Reallowed.Outcome",
      permissions::PermissionChangeInfo::
          kInfobarShownNoPageReloadPermissionUsed,
      1);

  PermissionUmaUtil::RecordPermissionRecoverySuccessRate(
      ContentSettingsType::MEDIASTREAM_CAMERA, /*is_used=*/false,
      /*show_infobar=*/true, /*page_reload=*/false);
  histograms.ExpectBucketCount(
      "Permissions.PageInfo.Changed.VideoCapture.Reallowed.Outcome",
      permissions::PermissionChangeInfo::
          kInfobarShownNoPageReloadPermissionNotUsed,
      1);

  PermissionUmaUtil::RecordPermissionRecoverySuccessRate(
      ContentSettingsType::MEDIASTREAM_CAMERA, /*is_used=*/true,
      /*show_infobar=*/false, /*page_reload=*/true);
  histograms.ExpectBucketCount(
      "Permissions.PageInfo.Changed.VideoCapture.Reallowed.Outcome",
      permissions::PermissionChangeInfo::
          kInfobarNotShownPageReloadPermissionUsed,
      1);

  PermissionUmaUtil::RecordPermissionRecoverySuccessRate(
      ContentSettingsType::MEDIASTREAM_CAMERA, /*is_used=*/false,
      /*show_infobar=*/false, /*page_reload=*/true);
  histograms.ExpectBucketCount(
      "Permissions.PageInfo.Changed.VideoCapture.Reallowed.Outcome",
      permissions::PermissionChangeInfo::
          kInfobarNotShownPageReloadPermissionNotUsed,
      1);

  PermissionUmaUtil::RecordPermissionRecoverySuccessRate(
      ContentSettingsType::MEDIASTREAM_CAMERA, /*is_used=*/true,
      /*show_infobar=*/false, /*page_reload=*/false);
  histograms.ExpectBucketCount(
      "Permissions.PageInfo.Changed.VideoCapture.Reallowed.Outcome",
      permissions::PermissionChangeInfo::
          kInfobarNotShownNoPageReloadPermissionUsed,
      1);

  PermissionUmaUtil::RecordPermissionRecoverySuccessRate(
      ContentSettingsType::MEDIASTREAM_CAMERA, /*is_used=*/false,
      /*show_infobar=*/false, /*page_reload=*/false);
  histograms.ExpectBucketCount(
      "Permissions.PageInfo.Changed.VideoCapture.Reallowed.Outcome",
      permissions::PermissionChangeInfo::
          kInfobarNotShownNoPageReloadPermissionNotUsed,
      1);
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(PermissionUmaUtilTest, RecordPermissionRegrantForUnusedSites) {
  const GURL origin = GURL("https://example1.com:443");
  content::TestBrowserContext browser_context;
  base::HistogramTester histograms;
  ContentSettingsType content_type = ContentSettingsType::GEOLOCATION;
  std::string permission_string =
      PermissionUtil::GetPermissionString(content_type);
  base::SimpleTestClock clock;
  base::Time now(base::Time::Now());
  clock.SetNow(now);
  HostContentSettingsMap* hcsm =
      PermissionsClient::Get()->GetSettingsMap(&browser_context);
  hcsm->SetClockForTesting(&clock);

  std::string prefix = "Settings.SafetyCheck.UnusedSitePermissionsRegrantDays";

  // Record regrant before permission has been revoked.
  PermissionUmaUtil::RecordPermissionRegrantForUnusedSites(
      origin, content_type, PermissionSourceUI::PROMPT, &browser_context, now);
  histograms.ExpectTotalCount(prefix + "Prompt." + permission_string, 0);
  histograms.ExpectTotalCount(prefix + "Prompt.All", 0);

  // Create a revoked permission.
  auto dict = base::Value::Dict().Set(
      permissions::kRevokedKey,
      base::Value::List().Append(static_cast<int32_t>(content_type)));
  // Set expiration to five days before the clean-up threshold to mimic that the
  // permission was revoked five days ago.
  base::Time past(now - base::Days(5));
  content_settings::ContentSettingConstraints constraint(past);
  constraint.set_lifetime(
      content_settings::features::
          kSafetyCheckUnusedSitePermissionsRevocationCleanUpThreshold.Get());
  hcsm->SetWebsiteSettingDefaultScope(
      origin, origin, ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      base::Value(dict.Clone()), constraint);

  // Regrant another permission through the prompt.
  PermissionUmaUtil::RecordPermissionRegrantForUnusedSites(
      origin, ContentSettingsType::NOTIFICATIONS, PermissionSourceUI::PROMPT,
      &browser_context, now);
  histograms.ExpectTotalCount(prefix + "Prompt." +
                                  PermissionUtil::GetPermissionString(
                                      ContentSettingsType::NOTIFICATIONS),
                              0);
  histograms.ExpectTotalCount(prefix + "Prompt.All", 0);

  // Regrant the geolocation permission through the prompt.
  PermissionUmaUtil::RecordPermissionRegrantForUnusedSites(
      origin, content_type, PermissionSourceUI::PROMPT, &browser_context, now);
  histograms.ExpectBucketCount(prefix + "Prompt." + permission_string, 5, 1);
  histograms.ExpectBucketCount(prefix + "Prompt.All", 5, 1);

  // Regrant the geolocation permission through site settings.
  PermissionUmaUtil::RecordPermissionRegrantForUnusedSites(
      origin, content_type, PermissionSourceUI::SITE_SETTINGS, &browser_context,
      now);
  histograms.ExpectBucketCount(prefix + "Settings." + permission_string, 5, 1);
  histograms.ExpectBucketCount(prefix + "Settings.All", 5, 1);
}

TEST_F(PermissionUmaUtilTest, GetDaysSinceUnusedSitePermissionRevocation) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      content_settings::features::kSafetyCheckUnusedSitePermissions);

  content::TestBrowserContext browser_context;
  base::SimpleTestClock clock;
  base::Time now(base::Time::Now());
  clock.SetNow(now);
  HostContentSettingsMap* hcsm =
      PermissionsClient::Get()->GetSettingsMap(&browser_context);

  const GURL url = GURL("https://example1.com:443");
  const ContentSettingsType type = ContentSettingsType::GEOLOCATION;
  content_settings::ContentSettingConstraints constraint(clock.Now());
  constraint.set_track_last_visit_for_autoexpiration(true);

  std::optional<uint32_t> days_since_revocation;

  // Permission has not yet been revoked, so shouldn't return a number of days
  // since revocation.
  days_since_revocation =
      PermissionUmaUtil::GetDaysSinceUnusedSitePermissionRevocation(
          url, ContentSettingsType::GEOLOCATION, now, hcsm);
  ASSERT_FALSE(days_since_revocation.has_value());

  hcsm->SetContentSettingDefaultScope(
      url, url, type, ContentSetting::CONTENT_SETTING_ALLOW, constraint);
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm).size(), 0u);

  // Travel 70 days through time such that the granted permission would be
  // revoked.
  clock.Advance(base::Days(70));
  // Revoke permission.
  content_settings::ContentSettingConstraints expiration_constraint(
      clock.Now());
  expiration_constraint.set_lifetime(base::Days(30));
  auto dict = base::Value::Dict().Set(
      permissions::kRevokedKey, base::Value::List().Append(static_cast<int32_t>(
                                    ContentSettingsType::GEOLOCATION)));
  hcsm->SetWebsiteSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      base::Value(std::move(dict)), expiration_constraint);
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm).size(), 1u);

  days_since_revocation =
      PermissionUmaUtil::GetDaysSinceUnusedSitePermissionRevocation(
          url, ContentSettingsType::GEOLOCATION, clock.Now(), hcsm);
  ASSERT_TRUE(days_since_revocation.has_value());
  EXPECT_EQ(days_since_revocation.value(), 0u);

  // Forward the clock for five days, which would be the number of days since
  // revocation.
  clock.Advance(base::Days(5));

  days_since_revocation =
      PermissionUmaUtil::GetDaysSinceUnusedSitePermissionRevocation(
          url, ContentSettingsType::GEOLOCATION, clock.Now(), hcsm);
  ASSERT_TRUE(days_since_revocation.has_value());
  EXPECT_EQ(days_since_revocation.value(), 5u);
}
#endif

// Inside your PermissionRecorderTest test fixture from earlier
TEST_F(PermissionsDelegationUmaUtilTest, SiteLevelAndOSPromptVariantsTest) {
  std::vector<ElementAnchoredBubbleVariant> variant_vector = {
      ElementAnchoredBubbleVariant::ASK};

#if BUILDFLAG(IS_MAC)
  variant_vector.push_back(ElementAnchoredBubbleVariant::OS_PROMPT);
  variant_vector.push_back(ElementAnchoredBubbleVariant::OS_SYSTEM_SETTINGS);
#endif

  std::optional<std::vector<ElementAnchoredBubbleVariant>> variants =
      variant_vector;

  ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto* main_frame = GetMainFrameAndNavigate(kTopLevelUrl);

  auto* permission_request_manager = SetupRequestManager(web_contents());
  PermissionRequestWrapper* request_owner =
      new PermissionRequestWrapper(RequestType::kCameraStream, kTopLevelUrl);
  permission_request_manager->AddRequest(main_frame, request_owner->request());

  PermissionUmaUtil::PermissionPromptResolved(
      {request_owner->request()}, web_contents(), PermissionAction::GRANTED,
      /*time_to_decision*/ base::TimeDelta(),
      PermissionPromptDisposition::ELEMENT_ANCHORED_BUBBLE,
      /* ui_reason*/ std::nullopt, variants,
      /*predicted_grant_likelihood*/ std::nullopt,
      /*prediction_decision_held_back*/ std::nullopt,
      /*ignored_reason*/ std::nullopt, /*did_show_prompt*/ true,
      /*did_click_managed*/ false,
      /*did_click_learn_more*/ false);

  const auto entries = ukm_recorder.GetEntriesByName("Permission");
  ASSERT_EQ(1u, entries.size());
  const auto* entry = entries.back().get();
  EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "SiteLevelScreen"),
            static_cast<int64_t>(ElementAnchoredBubbleVariant::ASK));
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "OsPromptScreen"),
            static_cast<int64_t>(ElementAnchoredBubbleVariant::OS_PROMPT));
  EXPECT_EQ(
      *ukm_recorder.GetEntryMetric(entry, "OsSystemSettingsScreen"),
      static_cast<int64_t>(ElementAnchoredBubbleVariant::OS_SYSTEM_SETTINGS));
#endif
}

TEST_F(PermissionsDelegationUmaUtilTest, SameOriginFrame) {
  base::HistogramTester histograms;
  auto* main_frame = GetMainFrameAndNavigate(kTopLevelUrl);
  auto* child_frame = AddChildFrameWithPermissionsPolicy(
      main_frame, kSameOriginFrameUrl,
      CreatePermissionsPolicy(
          blink::mojom::PermissionsPolicyFeature::kGeolocation,
          {std::string(kTopLevelUrl), std::string(kSameOriginFrameUrl)},
          /*matches_all_origins*/ true));
  histograms.ExpectTotalCount(kGeolocationUsageHistogramName, 0);

  auto* permission_request_manager = SetupRequestManager(web_contents());
  PermissionRequestWrapper* request_owner = new PermissionRequestWrapper(
      RequestType::kGeolocation, kSameOriginFrameUrl);
  permission_request_manager->AddRequest(child_frame, request_owner->request());
  PermissionUmaUtil::RecordPermissionsUsageSourceAndPolicyConfiguration(
      ContentSettingsType::GEOLOCATION, child_frame);
  EXPECT_THAT(histograms.GetAllSamples(kGeolocationUsageHistogramName),
              testing::ElementsAre(base::Bucket(0, 1)));
  histograms.ExpectTotalCount(kGeolocationPermissionsPolicyUsageHistogramName,
                              0);
  PermissionUmaUtil::PermissionPromptResolved(
      {request_owner->request()}, web_contents(), PermissionAction::GRANTED,
      /*time_to_decision*/ base::TimeDelta(),
      PermissionPromptDisposition::NOT_APPLICABLE,
      /* ui_reason*/ std::nullopt,
      /*variants*/ {},
      /*predicted_grant_likelihood*/ std::nullopt,
      /*prediction_decision_held_back*/ std::nullopt,
      /*ignored_reason*/ std::nullopt, /*did_show_prompt*/ false,
      /*did_click_managed*/ false,
      /*did_click_learn_more*/ false);
  histograms.ExpectTotalCount(kGeolocationPermissionsPolicyActionHistogramName,
                              0);
}

TEST_P(PermissionsDelegationUmaUtilTest, TopLevelFrame) {
  auto type = GetParam().type;
  std::string permission_string = PermissionUtil::GetPermissionString(type);
  // The histogram values should match with the ones defined in
  // |permission_uma_util.cc|
  std::string kPermissionsPolicyHeaderHistogramName =
      base::StrCat({"Permissions.Experimental.PrimaryMainNavigationFinished.",
                    permission_string, ".TopLevelHeaderPolicy"});

  base::HistogramTester histograms;
  auto* main_frame = GetMainFrameAndNavigate(kTopLevelUrl);
  auto feature = PermissionUtil::GetPermissionsPolicyFeature(type);
  blink::ParsedPermissionsPolicy top_policy;
  if (feature.has_value() &&
      (GetParam().matches_all_origins || !GetParam().origins.empty())) {
    top_policy = CreatePermissionsPolicy(
        GetParam().feature_overriden.has_value()
            ? GetParam().feature_overriden.value()
            : feature.value(),
        GetParam().origins, GetParam().matches_all_origins);
  }

  if (!top_policy.empty()) {
    RefreshAndSetPermissionsPolicy(&main_frame, top_policy);
  }

  histograms.ExpectTotalCount(kPermissionsPolicyHeaderHistogramName, 0);

  PermissionUmaUtil::RecordTopLevelPermissionsHeaderPolicyOnNavigation(
      main_frame);
  EXPECT_THAT(
      histograms.GetAllSamples(kPermissionsPolicyHeaderHistogramName),
      testing::ElementsAre(base::Bucket(
          static_cast<int>(GetParam().expected_configuration.value()), 1)));
}

INSTANTIATE_TEST_SUITE_P(
    TopLevelFrame,
    PermissionsDelegationUmaUtilTest,
    testing::Values(
        PermissionsDelegationTestConfig{
            ContentSettingsType::GEOLOCATION, PermissionAction::GRANTED,
            /*feature_overriden*/ std::nullopt,
            /*matches_all_origins*/ true,
            /*origins*/ {},
            PermissionHeaderPolicyForUMA::FEATURE_ALLOWLIST_IS_WILDCARD},

        PermissionsDelegationTestConfig{
            ContentSettingsType::GEOLOCATION,
            PermissionAction::GRANTED,
            /*feature_overriden*/ std::nullopt,
            /*matches_all_origins*/ false,
            {std::string(kTopLevelUrl)},
            PermissionHeaderPolicyForUMA::
                FEATURE_ALLOWLIST_EXPLICITLY_MATCHES_ORIGIN},

        PermissionsDelegationTestConfig{
            ContentSettingsType::GEOLOCATION, PermissionAction::GRANTED,
            /*feature_overriden*/ std::nullopt,
            /*matches_all_origins*/ false,
            /*origins*/ {},
            PermissionHeaderPolicyForUMA::HEADER_NOT_PRESENT_OR_INVALID},

        PermissionsDelegationTestConfig{
            ContentSettingsType::GEOLOCATION,
            PermissionAction::GRANTED,
            std::make_optional<blink::mojom::PermissionsPolicyFeature>(
                blink::mojom::PermissionsPolicyFeature::kCamera),
            /*matches_all_origins*/ false,
            {std::string(kTopLevelUrl)},
            PermissionHeaderPolicyForUMA::FEATURE_NOT_PRESENT},

        PermissionsDelegationTestConfig{
            ContentSettingsType::GEOLOCATION,
            PermissionAction::GRANTED,
            /*feature_overriden*/ std::nullopt,
            /*matches_all_origins*/ false,
            {std::string(kCrossOriginFrameUrl)},
            PermissionHeaderPolicyForUMA::
                FEATURE_ALLOWLIST_DOES_NOT_MATCH_ORIGIN}));

class CrossFramePermissionsDelegationUmaUtilTest
    : public PermissionsDelegationUmaUtilTest {
 public:
  CrossFramePermissionsDelegationUmaUtilTest() = default;
};

TEST_P(CrossFramePermissionsDelegationUmaUtilTest, CrossOriginFrame) {
  auto type = GetParam().type;
  std::string permission_string = PermissionUtil::GetPermissionString(type);
  // The histogram values should match with the ones defined in
  // |permission_uma_util.cc|
  std::string kUsageHistogramName =
      base::StrCat({PermissionUmaUtil::kPermissionsExperimentalUsagePrefix,
                    permission_string, ".IsCrossOriginFrame"});
  std::string kCrossOriginFrameActionHistogramName =
      base::StrCat({PermissionUmaUtil::kPermissionsActionPrefix,
                    permission_string, ".CrossOriginFrame"});
  std::string kPermissionsPolicyUsageHistogramName = base::StrCat(
      {PermissionUmaUtil::kPermissionsExperimentalUsagePrefix,
       permission_string, ".CrossOriginFrame.TopLevelHeaderPolicy"});
  std::string kPermissionsPolicyActionHistogramName = base::StrCat(
      {PermissionUmaUtil::kPermissionsActionPrefix, permission_string,
       ".CrossOriginFrame.TopLevelHeaderPolicy"});

  base::HistogramTester histograms;
  auto* main_frame = GetMainFrameAndNavigate(kTopLevelUrl);
  auto feature = PermissionUtil::GetPermissionsPolicyFeature(type);
  blink::ParsedPermissionsPolicy top_policy;
  if (feature.has_value() &&
      (GetParam().matches_all_origins || !GetParam().origins.empty())) {
    top_policy = CreatePermissionsPolicy(
        GetParam().feature_overriden.has_value()
            ? GetParam().feature_overriden.value()
            : feature.value(),
        GetParam().origins, GetParam().matches_all_origins);
  }

  if (!top_policy.empty()) {
    RefreshAndSetPermissionsPolicy(&main_frame, top_policy);
  }

  // Add nested subframes A(B(C))
  blink::ParsedPermissionsPolicy empty_policy;
  auto* child_frame = AddChildFrameWithPermissionsPolicy(
      main_frame, kCrossOriginFrameUrl,
      feature.has_value()
          ? CreatePermissionsPolicy(feature.value(),
                                    {std::string(kCrossOriginFrameUrl)},
                                    /*matches_all_origins*/ false)
          : empty_policy);
  child_frame = AddChildFrameWithPermissionsPolicy(
      child_frame, kCrossOriginFrameUrl2,
      feature.has_value()
          ? CreatePermissionsPolicy(feature.value(),
                                    {std::string(kCrossOriginFrameUrl2)},
                                    /*matches_all_origins*/ false)
          : empty_policy);
  histograms.ExpectTotalCount(kUsageHistogramName, 0);
  histograms.ExpectTotalCount(kPermissionsPolicyUsageHistogramName, 0);
  histograms.ExpectTotalCount(kPermissionsPolicyActionHistogramName, 0);

  PermissionUmaUtil::RecordPermissionsUsageSourceAndPolicyConfiguration(
      type, child_frame);
  EXPECT_THAT(histograms.GetAllSamples(kUsageHistogramName),
              testing::ElementsAre(base::Bucket(1, 1)));
  if (feature.has_value()) {
    EXPECT_THAT(
        histograms.GetAllSamples(kPermissionsPolicyUsageHistogramName),
        testing::ElementsAre(base::Bucket(
            static_cast<int>(GetParam().expected_configuration.value()), 1)));
  } else {
    histograms.ExpectTotalCount(kPermissionsPolicyUsageHistogramName, 0);
  }

  auto* permission_request_manager = SetupRequestManager(web_contents());
  PermissionRequestWrapper* request_owner = new PermissionRequestWrapper(
      permissions::ContentSettingsTypeToRequestType(type),
      kCrossOriginFrameUrl2);
  permission_request_manager->AddRequest(child_frame, request_owner->request());
  PermissionUmaUtil::PermissionPromptResolved(
      {request_owner->request()}, web_contents(), GetParam().action,
      /*time_to_decision*/ base::TimeDelta(),
      PermissionPromptDisposition::NOT_APPLICABLE,
      /* ui_reason*/ std::nullopt,
      /*variants*/ {},
      /*predicted_grant_likelihood*/ std::nullopt,
      /*prediction_decision_held_back*/ std::nullopt,
      /*ignored_reason*/ std::nullopt, /*did_show_prompt*/ false,
      /*did_click_managed*/ false,
      /*did_click_learn_more*/ false);
  if (feature.has_value()) {
    EXPECT_THAT(
        histograms.GetAllSamples(kPermissionsPolicyActionHistogramName),
        testing::ElementsAre(base::Bucket(
            static_cast<int>(GetParam().expected_configuration.value()), 1)));
  } else {
    histograms.ExpectTotalCount(kPermissionsPolicyActionHistogramName, 0);
  }

  EXPECT_THAT(histograms.GetAllSamples(kCrossOriginFrameActionHistogramName),
              testing::ElementsAre(
                  base::Bucket(static_cast<int>(GetParam().action), 1)));
}

INSTANTIATE_TEST_SUITE_P(
    CrossOriginFrame,
    CrossFramePermissionsDelegationUmaUtilTest,
    testing::Values(
        PermissionsDelegationTestConfig{
            ContentSettingsType::GEOLOCATION, PermissionAction::GRANTED,
            /*feature_overriden*/ std::nullopt,
            /*matches_all_origins*/ true,
            /*origins*/ {},
            PermissionHeaderPolicyForUMA::FEATURE_ALLOWLIST_IS_WILDCARD},

        PermissionsDelegationTestConfig{
            ContentSettingsType::GEOLOCATION,
            PermissionAction::DENIED,
            /*feature_overriden*/ std::nullopt,
            /*matches_all_origins*/ false,
            {std::string(kTopLevelUrl), std::string(kCrossOriginFrameUrl),
             std::string(kCrossOriginFrameUrl2)},
            PermissionHeaderPolicyForUMA::
                FEATURE_ALLOWLIST_EXPLICITLY_MATCHES_ORIGIN},

        PermissionsDelegationTestConfig{
            ContentSettingsType::GEOLOCATION, PermissionAction::GRANTED,
            /*feature_overriden*/ std::nullopt,
            /*matches_all_origins*/ false,
            /*origins*/ {},
            PermissionHeaderPolicyForUMA::HEADER_NOT_PRESENT_OR_INVALID},

        PermissionsDelegationTestConfig{
            ContentSettingsType::GEOLOCATION,
            PermissionAction::GRANTED,
            std::make_optional<blink::mojom::PermissionsPolicyFeature>(
                blink::mojom::PermissionsPolicyFeature::kCamera),
            /*matches_all_origins*/ false,
            {std::string(kTopLevelUrl), std::string(kCrossOriginFrameUrl)},
            PermissionHeaderPolicyForUMA::FEATURE_NOT_PRESENT},

        PermissionsDelegationTestConfig{
            ContentSettingsType::GEOLOCATION,
            PermissionAction::DENIED,
            /*feature_overriden*/ std::nullopt,
            /*matches_all_origins*/ false,
            {std::string(kTopLevelUrl), std::string(kCrossOriginFrameUrl)},
            PermissionHeaderPolicyForUMA::
                FEATURE_ALLOWLIST_DOES_NOT_MATCH_ORIGIN}));

class UkmRecorderPermissionUmaUtilTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override { content::RenderViewHostTestHarness::SetUp(); }

  class UkmRecorderTestPermissionsClient : public TestPermissionsClient {
   public:
    UkmRecorderTestPermissionsClient() = default;

    void SetSimulatedHasSourceId(bool source_id) {
      simulated_has_source_id_ = source_id;
    }

    void GetUkmSourceId(ContentSettingsType permission_type,
                        content::BrowserContext* browser_context,
                        content::WebContents* web_contents,
                        const GURL& requesting_origin,
                        GetUkmSourceIdCallback callback) override {
      // Short circuit and return a null SourceId.
      if (!simulated_has_source_id_) {
        std::move(callback).Run(std::nullopt);
      } else {
        ukm::SourceId fake_source_id =
            ukm::ConvertToSourceId(1, ukm::SourceIdType::NOTIFICATION_ID);
        std::move(callback).Run(fake_source_id);
      }
    }

   private:
    bool simulated_has_source_id_ = false;
  };

  UkmRecorderTestPermissionsClient permissions_client_;
};

TEST_F(UkmRecorderPermissionUmaUtilTest,
       NotificationRevocationHistogramDidRecordUkmTest) {
  base::HistogramTester histograms;
  content::TestBrowserContext browser_context;
  ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  permissions_client_.SetSimulatedHasSourceId(true);
  const GURL origin(kTopLevelUrl);
  PermissionUmaUtil::PermissionRevoked(
      ContentSettingsType::NOTIFICATIONS,
      permissions::PermissionSourceUI::ANDROID_SETTINGS, origin,
      &browser_context);

  histograms.ExpectBucketCount("Permissions.Action.Notifications",
                               static_cast<int64_t>(PermissionAction::REVOKED),
                               1);
  histograms.ExpectBucketCount(
      "Permissions.Revocation.Notifications.DidRecordUkm", 1, 1);
  const auto entries = ukm_recorder.GetEntriesByName("Permission");
  ASSERT_EQ(1u, entries.size());
  const auto* entry = entries.back().get();
  EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "Action"),
            static_cast<int64_t>(PermissionAction::REVOKED));
}

TEST_F(UkmRecorderPermissionUmaUtilTest,
       NotificationRevocationHistogramDroppedUkmTest) {
  base::HistogramTester histograms;
  content::TestBrowserContext browser_context;
  ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  permissions_client_.SetSimulatedHasSourceId(false);
  const GURL origin(kTopLevelUrl);
  PermissionUmaUtil::PermissionRevoked(
      ContentSettingsType::NOTIFICATIONS,
      permissions::PermissionSourceUI::ANDROID_SETTINGS, origin,
      &browser_context);

  histograms.ExpectBucketCount("Permissions.Action.Notifications",
                               static_cast<int64_t>(PermissionAction::REVOKED),
                               1);

  histograms.ExpectBucketCount(
      "Permissions.Revocation.Notifications.DidRecordUkm", 0, 1);
  const auto entries = ukm_recorder.GetEntriesByName("Permission");
  EXPECT_EQ(0u, entries.size());
}

TEST_F(UkmRecorderPermissionUmaUtilTest,
       NotificationUsageHistogramDidRecordUkmTest) {
  base::HistogramTester histograms;
  content::TestBrowserContext browser_context;
  ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  permissions_client_.SetSimulatedHasSourceId(true);
  PermissionUmaUtil::RecordPermissionUsage(ContentSettingsType::NOTIFICATIONS,
                                           &browser_context, web_contents(),
                                           GURL(kTopLevelUrl));

  histograms.ExpectBucketCount("Permissions.Usage.Notifications.DidRecordUkm",
                               1, 1);
  const auto entries = ukm_recorder.GetEntriesByName("PermissionUsage");
  ASSERT_EQ(1u, entries.size());
  const auto* entry = entries.back().get();
  EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "PermissionType"),
            content_settings_uma_util::ContentSettingTypeToHistogramValue(
                ContentSettingsType::NOTIFICATIONS));
}

TEST_F(UkmRecorderPermissionUmaUtilTest,
       NotificationUsageHistogramDroppedUkmTest) {
  base::HistogramTester histograms;
  content::TestBrowserContext browser_context;

  ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  permissions_client_.SetSimulatedHasSourceId(false);
  PermissionUmaUtil::RecordPermissionUsage(ContentSettingsType::NOTIFICATIONS,
                                           &browser_context, web_contents(),
                                           GURL(kTopLevelUrl));

  histograms.ExpectBucketCount("Permissions.Usage.Notifications.DidRecordUkm",
                               0, 1);
  const auto entries = ukm_recorder.GetEntriesByName("PermissionUsage");
  ASSERT_EQ(0u, entries.size());
}
}  // namespace permissions
