// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_uma_util.h"

#include <memory>

#include "base/feature_list.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
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
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/constants.h"
#include "components/permissions/permission_decision.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_data.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/permissions/resolvers/content_setting_permission_resolver.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/permissions/test/permission_request_observer.h"
#include "components/permissions/test/test_permissions_client.h"
#include "components/safety_check/safety_check.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_render_frame_host.h"
#include "services/network/public/cpp/permissions_policy/origin_with_possible_wildcards.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

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

network::ParsedPermissionsPolicy CreatePermissionsPolicy(
    network::mojom::PermissionsPolicyFeature feature,
    const std::vector<std::string>& origins,
    bool matches_all_origins = false) {
  std::vector<network::OriginWithPossibleWildcards> allow_origins;
  for (const auto& origin : origins) {
    allow_origins.emplace_back(
        *network::OriginWithPossibleWildcards::FromOrigin(
            url::Origin::Create(GURL(origin))));
  }
  return {{feature, allow_origins, /*self_if_matches=*/std::nullopt,
           matches_all_origins,
           /*matches_opaque_src=*/false}};
}

struct PermissionsDelegationTestConfig {
  ContentSettingsType type;
  PermissionAction action;
  std::optional<network::mojom::PermissionsPolicyFeature> feature_overriden;

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

std::unique_ptr<permissions::PermissionRequest> CreateRequest(
    permissions::RequestType type,
    const char* url) {
  return std::make_unique<permissions::PermissionRequest>(
      std::make_unique<PermissionRequestData>(
          std::make_unique<ContentSettingPermissionResolver>(
              RequestTypeToContentSettingsType(type).value()),
          /*user_gesture=*/true, GURL(url)),
      base::BindRepeating([](const PermissionPromptDecision&,
                             const PermissionRequestData&) {}));
}

}  // namespace

class PermissionsDelegationUmaUtilTestBase
    : public content::RenderViewHostTestHarness {
 protected:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    auto* main_frame = web_contents()->GetPrimaryMainFrame();
    content::RenderFrameHostTester::For(main_frame)
        ->InitializeRenderFrameIfNeeded();

    SimulateNavigation(&main_frame, GURL(kTopLevelUrl));

    PermissionRequestManager::CreateForWebContents(web_contents());
    manager_ = PermissionRequestManager::FromWebContents(web_contents());
    prompt_factory_ = std::make_unique<MockPermissionPromptFactory>(manager_);
  }

  void TearDown() override {
    prompt_factory_ = nullptr;
    manager_ = nullptr;
    content::RenderViewHostTestHarness::TearDown();
  }

  content::RenderFrameHost* AddChildFrameWithPermissionsPolicy(
      content::RenderFrameHost* parent,
      const char* origin,
      network::ParsedPermissionsPolicy policy) {
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
                                      network::ParsedPermissionsPolicy policy) {
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

  void AddRequest(content::RenderFrameHost* rfh,
                  std::unique_ptr<PermissionRequest> request) {
    permissions::PermissionRequestObserver observer(web_contents());
    manager_->AddRequest(rfh, std::move(request));
    observer.Wait();
  }

  content::RenderFrameHost* primary_main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }

 protected:
  raw_ptr<PermissionRequestManager> manager_;

 private:
  TestPermissionsClient permissions_client_;
  std::unique_ptr<MockPermissionPromptFactory> prompt_factory_;
};

class PermissionsDelegationUmaUtilTest
    : public PermissionsDelegationUmaUtilTestBase,
      public testing::WithParamInterface<PermissionsDelegationTestConfig> {};

class PermissionUmaUtilTest : public testing::Test {
 private:
  content::BrowserTaskEnvironment task_environment_;
  TestPermissionsClient permissions_client_;
};

TEST_F(PermissionUmaUtilTest, ScopedRevocationReporter) {
  content::TestBrowserContext browser_context;

  // TODO(tsergeant): Add more comprehensive tests of PermissionUmaUtil.
  base::HistogramTester histograms;
  auto* map = PermissionsClient::Get()->GetSettingsMap(&browser_context);
  GURL host("https://example.com");
  ContentSettingsPattern host_pattern =
      ContentSettingsPattern::FromURLNoWildcard(host);
  ContentSettingsPattern host_containing_wildcards_pattern =
      ContentSettingsPattern::FromString("https://[*.]example.com/");
  ContentSettingsType type = content_settings::GeolocationContentSettingsType();
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

TEST_F(PermissionsDelegationUmaUtilTest, UsageAndPromptInTopLevelFrame) {
  base::HistogramTester histograms;
  auto* main_frame = primary_main_frame();
  histograms.ExpectTotalCount(kGeolocationUsageHistogramName, 0);

  AddRequest(main_frame,
             CreateRequest(RequestType::kGeolocation, kTopLevelUrl));

  PermissionUmaUtil::RecordPermissionsUsageSourceAndPolicyConfiguration(
      content_settings::GeolocationContentSettingsType(), main_frame);
  EXPECT_THAT(histograms.GetAllSamples(kGeolocationUsageHistogramName),
              testing::ElementsAre(base::Bucket(0, 1)));

  PermissionUmaUtil::PermissionPromptResolved(
      manager_->Requests(), browser_context(), PermissionAction::GRANTED,
      /*prompt_options=*/std::monostate(),
      /*time_to_decision*/ base::TimeDelta(),
      PermissionPromptDisposition::NOT_APPLICABLE,
      /*ui_reason=*/std::nullopt,
      /*variants=*/{},
      /*predicted_grant_likelihood=*/std::nullopt,
      /*permission_request_relevance=*/std::nullopt,
      /*permission_ai_relevance_model=*/std::nullopt,
      /*prediction_decision_held_back=*/std::nullopt,
      /*ignored_reason=*/std::nullopt, /*did_show_prompt=*/false,
      /*did_click_manage=*/false,
      /*did_click_learn_more=*/false,
      /*initial_geolocation_accuracy_selection=*/std::nullopt);
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
  ContentSettingsType content_type =
      content_settings::GeolocationContentSettingsType();
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
  auto dict = base::DictValue().Set(
      permissions::kRevokedKey,
      base::ListValue().Append(static_cast<int32_t>(content_type)));
  // Set expiration to five days before the clean-up threshold to mimic that the
  // permission was revoked five days ago.
  base::Time past(now - base::Days(5));
  content_settings::ContentSettingConstraints constraint(past);
  constraint.set_lifetime(
      safety_check::GetUnusedSitePermissionsRevocationCleanUpThreshold());
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
  const ContentSettingsType type =
      content_settings::GeolocationContentSettingsType();
  content_settings::ContentSettingConstraints constraint(clock.Now());
  constraint.set_track_last_visit_for_autoexpiration(true);

  std::optional<uint32_t> days_since_revocation;

  // Permission has not yet been revoked, so shouldn't return a number of days
  // since revocation.
  days_since_revocation =
      PermissionUmaUtil::GetDaysSinceUnusedSitePermissionRevocation(
          url, content_settings::GeolocationContentSettingsType(), now, hcsm);
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
  auto dict = base::DictValue().Set(
      permissions::kRevokedKey,
      base::ListValue().Append(static_cast<int32_t>(
          content_settings::GeolocationContentSettingsType())));
  hcsm->SetWebsiteSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      base::Value(std::move(dict)), expiration_constraint);
  EXPECT_EQ(GetRevokedUnusedPermissions(hcsm).size(), 1u);

  days_since_revocation =
      PermissionUmaUtil::GetDaysSinceUnusedSitePermissionRevocation(
          url, content_settings::GeolocationContentSettingsType(), clock.Now(),
          hcsm);
  ASSERT_TRUE(days_since_revocation.has_value());
  EXPECT_EQ(days_since_revocation.value(), 0u);

  // Forward the clock for five days, which would be the number of days since
  // revocation.
  clock.Advance(base::Days(5));

  days_since_revocation =
      PermissionUmaUtil::GetDaysSinceUnusedSitePermissionRevocation(
          url, content_settings::GeolocationContentSettingsType(), clock.Now(),
          hcsm);
  ASSERT_TRUE(days_since_revocation.has_value());
  EXPECT_EQ(days_since_revocation.value(), 5u);
}
#endif

TEST_F(PermissionUmaUtilTest, RecordOnPermissionStatusChangedEventSubscribed) {
  base::HistogramTester histograms;
  PermissionUmaUtil::RecordOnPermissionStatusChangedEventSubscribed(
      RequestType::kNotifications, /*subscribed=*/true);
  histograms.ExpectBucketCount(
      "Permissions.PredictionService.Notifications.OnStatusChangeListener",
      true, 1);

  PermissionUmaUtil::RecordOnPermissionStatusChangedEventSubscribed(
      RequestType::kGeolocation, /*subscribed=*/true);
  histograms.ExpectBucketCount(
      "Permissions.PredictionService.Geolocation.OnStatusChangeListener", true,
      1);

  PermissionUmaUtil::RecordOnPermissionStatusChangedEventSubscribed(
      RequestType::kNotifications, /*subscribed=*/false);
  histograms.ExpectBucketCount(
      "Permissions.PredictionService.Notifications.OnStatusChangeListener",
      false, 1);

  PermissionUmaUtil::RecordOnPermissionStatusChangedEventSubscribed(
      RequestType::kGeolocation, /*subscribed=*/false);
  histograms.ExpectBucketCount(
      "Permissions.PredictionService.Geolocation.OnStatusChangeListener", false,
      1);
}

TEST_F(PermissionUmaUtilTest, RecordPageInfoPermissionChange) {
  base::HistogramTester histograms;
  PermissionUmaUtil::RecordPageInfoPermissionChange(
      ContentSettingsType::NOTIFICATIONS, ContentSetting::CONTENT_SETTING_ALLOW,
      ContentSetting::CONTENT_SETTING_BLOCK,
      /*is_subscribed_to_permission_change_event=*/true);
  histograms.ExpectBucketCount(
      "Permissions.PageInfo.Changed.Notifications.OnStatusChangeListener", true,
      1);

  PermissionUmaUtil::RecordPageInfoPermissionChange(
      ContentSettingsType::NOTIFICATIONS, ContentSetting::CONTENT_SETTING_ALLOW,
      ContentSetting::CONTENT_SETTING_BLOCK,
      /*is_subscribed_to_permission_change_event=*/false);
  histograms.ExpectBucketCount(
      "Permissions.PageInfo.Changed.Notifications.OnStatusChangeListener",
      false, 1);

  PermissionUmaUtil::RecordPageInfoPermissionChange(
      ContentSettingsType::GEOLOCATION, ContentSetting::CONTENT_SETTING_ALLOW,
      ContentSetting::CONTENT_SETTING_BLOCK,
      /*is_subscribed_to_permission_change_event=*/true);
  histograms.ExpectBucketCount(
      "Permissions.PageInfo.Changed.Geolocation.OnStatusChangeListener", true,
      1);

  PermissionUmaUtil::RecordPageInfoPermissionChange(
      ContentSettingsType::GEOLOCATION, ContentSetting::CONTENT_SETTING_ALLOW,
      ContentSetting::CONTENT_SETTING_BLOCK,
      /*is_subscribed_to_permission_change_event=*/false);
  histograms.ExpectBucketCount(
      "Permissions.PageInfo.Changed.Geolocation.OnStatusChangeListener", false,
      1);
}

// Inside your PermissionRecorderTest test fixture from earlier
TEST_F(PermissionsDelegationUmaUtilTest, SiteLevelAndOSPromptVariantsTest) {
  std::vector<ElementAnchoredBubbleVariant> variant_vector = {
      ElementAnchoredBubbleVariant::kAsk};

#if BUILDFLAG(IS_MAC)
  variant_vector.push_back(ElementAnchoredBubbleVariant::kOsPrompt);
  variant_vector.push_back(ElementAnchoredBubbleVariant::kOsSystemSettings);
#endif

  std::optional<std::vector<ElementAnchoredBubbleVariant>> variants =
      variant_vector;

  ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto* main_frame = primary_main_frame();

  AddRequest(main_frame,
             CreateRequest(RequestType::kCameraStream, kTopLevelUrl));

  PermissionUmaUtil::PermissionPromptResolved(
      {manager_->Requests()}, browser_context(), PermissionAction::GRANTED,
      /*prompt_options=*/std::monostate(),
      /*time_to_decision*/ base::TimeDelta(),
      PermissionPromptDisposition::ELEMENT_ANCHORED_BUBBLE,
      /*ui_reason=*/std::nullopt, variants,
      /*predicted_grant_likelihood=*/std::nullopt,
      /*permission_request_relevance=*/std::nullopt,
      /*permission_ai_relevance_model=*/std::nullopt,
      /*prediction_decision_held_back=*/std::nullopt,
      /*ignored_reason=*/std::nullopt, /*did_show_prompt=*/true,
      /*did_click_manage=*/false,
      /*did_click_learn_more=*/false,
      /*initial_geolocation_accuracy_selection=*/std::nullopt);

  const auto entries = ukm_recorder.GetEntriesByName("Permission");
  ASSERT_EQ(1u, entries.size());
  const auto* entry = entries.back().get();
  EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "SiteLevelScreen"),
            static_cast<int64_t>(ElementAnchoredBubbleVariant::kAsk));
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "OsPromptScreen"),
            static_cast<int64_t>(ElementAnchoredBubbleVariant::kOsPrompt));
  EXPECT_EQ(
      *ukm_recorder.GetEntryMetric(entry, "OsSystemSettingsScreen"),
      static_cast<int64_t>(ElementAnchoredBubbleVariant::kOsSystemSettings));
#endif
}

TEST_F(PermissionsDelegationUmaUtilTest, PermissionAiRelevanceModelUkmTest) {
  ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto* main_frame = primary_main_frame();
  AddRequest(main_frame,
             CreateRequest(RequestType::kCameraStream, kTopLevelUrl));
  const std::optional<permissions::PermissionAiRelevanceModel>
      test_relvance_model = permissions::PermissionAiRelevanceModel::kAIv4;

  PermissionUmaUtil::PermissionPromptResolved(
      manager_->Requests(), browser_context(), PermissionAction::GRANTED,
      /*prompt_options=*/std::monostate(),
      /*time_to_decision*/ base::TimeDelta(),
      PermissionPromptDisposition::ELEMENT_ANCHORED_BUBBLE,
      /*ui_reason=*/std::nullopt, /*variants*/ {},
      /*predicted_grant_likelihood=*/std::nullopt,
      /*permission_request_relevance=*/std::nullopt,
      /*permission_ai_relevance_model=*/test_relvance_model,
      /*prediction_decision_held_back=*/std::nullopt,
      /*ignored_reason=*/std::nullopt, /*did_show_prompt=*/true,
      /*did_click_manage=*/false,
      /*did_click_learn_more=*/false,
      /*initial_geolocation_accuracy_selection=*/std::nullopt);

  const auto entries = ukm_recorder.GetEntriesByName("Permission");
  ASSERT_EQ(1u, entries.size());
  const auto* entry = entries.back().get();
  EXPECT_EQ(*ukm_recorder.GetEntryMetric(entry, "PermissionAiRelevanceModel"),
            static_cast<int64_t>(test_relvance_model.value()));
}

TEST_F(PermissionsDelegationUmaUtilTest, SameOriginFrame) {
  base::HistogramTester histograms;
  auto* main_frame = primary_main_frame();
  auto* child_frame = AddChildFrameWithPermissionsPolicy(
      main_frame, kSameOriginFrameUrl,
      CreatePermissionsPolicy(
          network::mojom::PermissionsPolicyFeature::kGeolocation,
          {std::string(kTopLevelUrl), std::string(kSameOriginFrameUrl)},
          /*matches_all_origins=*/true));
  histograms.ExpectTotalCount(kGeolocationUsageHistogramName, 0);

  AddRequest(child_frame,
             CreateRequest(RequestType::kGeolocation, kSameOriginFrameUrl));

  PermissionUmaUtil::RecordPermissionsUsageSourceAndPolicyConfiguration(
      content_settings::GeolocationContentSettingsType(), child_frame);
  EXPECT_THAT(histograms.GetAllSamples(kGeolocationUsageHistogramName),
              testing::ElementsAre(base::Bucket(0, 1)));
  histograms.ExpectTotalCount(kGeolocationPermissionsPolicyUsageHistogramName,
                              0);
  PermissionUmaUtil::PermissionPromptResolved(
      manager_->Requests(), browser_context(), PermissionAction::GRANTED,
      /*prompt_options=*/std::monostate(),
      /*time_to_decision*/ base::TimeDelta(),
      PermissionPromptDisposition::NOT_APPLICABLE,
      /*ui_reason=*/std::nullopt,
      /*variants=*/{},
      /*predicted_grant_likelihood=*/std::nullopt,
      /*permission_request_relevance=*/std::nullopt,
      /*permission_ai_relevance_model=*/std::nullopt,
      /*prediction_decision_held_back=*/std::nullopt,
      /*ignored_reason=*/std::nullopt, /*did_show_prompt=*/false,
      /*did_click_manage=*/false,
      /*did_click_learn_more=*/false,
      /*initial_geolocation_accuracy_selection=*/std::nullopt);
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
  auto* main_frame = primary_main_frame();
  auto feature = PermissionUtil::GetPermissionsPolicyFeature(type);
  network::ParsedPermissionsPolicy top_policy;
  if (feature.has_value() &&
      (GetParam().matches_all_origins || !GetParam().origins.empty())) {
    top_policy = CreatePermissionsPolicy(
        GetParam().feature_overriden.has_value()
            ? GetParam().feature_overriden.value()
            : feature.value(),
        GetParam().origins, GetParam().matches_all_origins);
  }

  RefreshAndSetPermissionsPolicy(&main_frame, top_policy);
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
            ContentSettingsType::MEDIASTREAM_MIC, PermissionAction::GRANTED,
            /*feature_overriden=*/std::nullopt,
            /*matches_all_origins=*/true,
            /*origins=*/{},
            PermissionHeaderPolicyForUMA::FEATURE_ALLOWLIST_IS_WILDCARD},

        PermissionsDelegationTestConfig{
            ContentSettingsType::MEDIASTREAM_MIC,
            PermissionAction::GRANTED,
            /*feature_overriden=*/std::nullopt,
            /*matches_all_origins=*/false,
            {std::string(kTopLevelUrl)},
            PermissionHeaderPolicyForUMA::
                FEATURE_ALLOWLIST_EXPLICITLY_MATCHES_ORIGIN},

        PermissionsDelegationTestConfig{
            ContentSettingsType::MEDIASTREAM_MIC, PermissionAction::GRANTED,
            /*feature_overriden=*/std::nullopt,
            /*matches_all_origins=*/false,
            /*origins=*/{},
            PermissionHeaderPolicyForUMA::HEADER_NOT_PRESENT_OR_INVALID},

        PermissionsDelegationTestConfig{
            ContentSettingsType::MEDIASTREAM_MIC,
            PermissionAction::GRANTED,
            std::make_optional<network::mojom::PermissionsPolicyFeature>(
                network::mojom::PermissionsPolicyFeature::kCamera),
            /*matches_all_origins=*/false,
            {std::string(kTopLevelUrl)},
            PermissionHeaderPolicyForUMA::FEATURE_NOT_PRESENT},

        PermissionsDelegationTestConfig{
            ContentSettingsType::MEDIASTREAM_MIC,
            PermissionAction::GRANTED,
            /*feature_overriden=*/std::nullopt,
            /*matches_all_origins=*/false,
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
  auto* main_frame = primary_main_frame();
  auto feature = PermissionUtil::GetPermissionsPolicyFeature(type);
  network::ParsedPermissionsPolicy top_policy;
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
  network::ParsedPermissionsPolicy empty_policy;
  auto* child_frame = AddChildFrameWithPermissionsPolicy(
      main_frame, kCrossOriginFrameUrl,
      feature.has_value()
          ? CreatePermissionsPolicy(feature.value(),
                                    {std::string(kCrossOriginFrameUrl)},
                                    /*matches_all_origins=*/false)
          : empty_policy);
  child_frame = AddChildFrameWithPermissionsPolicy(
      child_frame, kCrossOriginFrameUrl2,
      feature.has_value()
          ? CreatePermissionsPolicy(feature.value(),
                                    {std::string(kCrossOriginFrameUrl2)},
                                    /*matches_all_origins=*/false)
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

  AddRequest(child_frame,
             CreateRequest(permissions::ContentSettingsTypeToRequestType(type),
                           kCrossOriginFrameUrl2));

  PermissionUmaUtil::PermissionPromptResolved(
      manager_->Requests(), browser_context(), GetParam().action,
      /*prompt_options=*/std::monostate(),
      /*time_to_decision*/ base::TimeDelta(),
      PermissionPromptDisposition::NOT_APPLICABLE,
      /*ui_reason=*/std::nullopt,
      /*variants=*/{},
      /*predicted_grant_likelihood=*/std::nullopt,
      /*permission_request_relevance=*/std::nullopt,
      /*permission_ai_relevance_model=*/std::nullopt,
      /*prediction_decision_held_back=*/std::nullopt,
      /*ignored_reason=*/std::nullopt, /*did_show_prompt=*/false,
      /*did_click_manage=*/false,
      /*did_click_learn_more=*/false,
      /*initial_geolocation_accuracy_selection=*/std::nullopt);
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
            ContentSettingsType::MEDIASTREAM_MIC, PermissionAction::GRANTED,
            /*feature_overriden=*/std::nullopt,
            /*matches_all_origins=*/true,
            /*origins=*/{},
            PermissionHeaderPolicyForUMA::FEATURE_ALLOWLIST_IS_WILDCARD},

        PermissionsDelegationTestConfig{
            ContentSettingsType::MEDIASTREAM_MIC,
            PermissionAction::DENIED,
            /*feature_overriden=*/std::nullopt,
            /*matches_all_origins=*/false,
            {std::string(kTopLevelUrl), std::string(kCrossOriginFrameUrl),
             std::string(kCrossOriginFrameUrl2)},
            PermissionHeaderPolicyForUMA::
                FEATURE_ALLOWLIST_EXPLICITLY_MATCHES_ORIGIN},

        PermissionsDelegationTestConfig{
            ContentSettingsType::MEDIASTREAM_MIC, PermissionAction::GRANTED,
            /*feature_overriden=*/std::nullopt,
            /*matches_all_origins=*/false,
            /*origins=*/{},
            PermissionHeaderPolicyForUMA::HEADER_NOT_PRESENT_OR_INVALID},

        PermissionsDelegationTestConfig{
            ContentSettingsType::MEDIASTREAM_MIC,
            PermissionAction::GRANTED,
            std::make_optional<network::mojom::PermissionsPolicyFeature>(
                network::mojom::PermissionsPolicyFeature::kCamera),
            /*matches_all_origins=*/false,
            {std::string(kTopLevelUrl), std::string(kCrossOriginFrameUrl)},
            PermissionHeaderPolicyForUMA::FEATURE_NOT_PRESENT},

        PermissionsDelegationTestConfig{
            ContentSettingsType::MEDIASTREAM_MIC,
            PermissionAction::DENIED,
            /*feature_overriden=*/std::nullopt,
            /*matches_all_origins=*/false,
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
                        content::RenderFrameHost* render_frame_host,
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
  ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  permissions_client_.SetSimulatedHasSourceId(true);
  PermissionUmaUtil::RecordPermissionUsage(
      ContentSettingsType::NOTIFICATIONS, browser_context(),
      web_contents()->GetPrimaryMainFrame(), GURL(kTopLevelUrl));

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

  ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  permissions_client_.SetSimulatedHasSourceId(false);
  PermissionUmaUtil::RecordPermissionUsage(
      ContentSettingsType::NOTIFICATIONS, browser_context(),
      web_contents()->GetPrimaryMainFrame(), GURL(kTopLevelUrl));

  histograms.ExpectBucketCount("Permissions.Usage.Notifications.DidRecordUkm",
                               0, 1);
  const auto entries = ukm_recorder.GetEntriesByName("PermissionUsage");
  ASSERT_EQ(0u, entries.size());
}

struct PredictionServiceActionTestConfig {
  RequestType request_type;
  PermissionRequestGestureType gesture_type;
  PermissionAction action;
  PermissionPromptDisposition disposition;
  std::string histogram_name;
};

class PredictionServiceActionTest
    : public PermissionsDelegationUmaUtilTestBase,
      public testing::WithParamInterface<PredictionServiceActionTestConfig> {};

TEST_P(PredictionServiceActionTest, PredictionServiceAction) {
  base::HistogramTester histogram_tester;
  std::vector<std::unique_ptr<PermissionRequest>> requests;
  requests.push_back(std::make_unique<MockPermissionRequest>(
      GetParam().request_type, GetParam().gesture_type));

  PermissionUmaUtil::PermissionPromptResolved(
      requests, browser_context(), GetParam().action,
      /*prompt_options=*/std::monostate(), base::TimeDelta(),
      GetParam().disposition,
      /*ui_reason=*/std::nullopt,
      /*variants=*/{},
      /*predicted_grant_likelihood=*/std::nullopt,
      /*permission_request_relevance=*/std::nullopt,
      /*permission_ai_relevance_model=*/std::nullopt,
      /*prediction_decision_held_back=*/std::nullopt,
      /*ignored_reason=*/std::nullopt,
      /*did_show_prompt=*/false,
      /*did_click_manage=*/false,
      /*did_click_learn_more=*/false,
      /*initial_geolocation_accuracy_selection=*/std::nullopt);

  histogram_tester.ExpectUniqueSample(GetParam().histogram_name,
                                      GetParam().action, 1);
}

INSTANTIATE_TEST_SUITE_P(
    PredictionServiceAction,
    PredictionServiceActionTest,
    testing::Values(
        PredictionServiceActionTestConfig{
            RequestType::kNotifications, PermissionRequestGestureType::GESTURE,
            PermissionAction::GRANTED,
            PermissionPromptDisposition::ANCHORED_BUBBLE,
            "Permissions.PredictionService.Action.Notifications.Gesture.Loud"},
        PredictionServiceActionTestConfig{
            RequestType::kNotifications,
            PermissionRequestGestureType::NO_GESTURE, PermissionAction::DENIED,
            PermissionPromptDisposition::ANCHORED_BUBBLE,
            "Permissions.PredictionService.Action.Notifications.NoGesture."
            "Loud"},
        PredictionServiceActionTestConfig{
            RequestType::kNotifications, PermissionRequestGestureType::GESTURE,
            PermissionAction::DISMISSED,
            PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_CHIP,
            "Permissions.PredictionService.Action.Notifications.Gesture.Quiet"},
        PredictionServiceActionTestConfig{
            RequestType::kNotifications,
            PermissionRequestGestureType::NO_GESTURE, PermissionAction::IGNORED,
            PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_CHIP,
            "Permissions.PredictionService.Action.Notifications.NoGesture."
            "Quiet"},
        PredictionServiceActionTestConfig{
            RequestType::kGeolocation, PermissionRequestGestureType::GESTURE,
            PermissionAction::GRANTED_ONCE,
            PermissionPromptDisposition::ANCHORED_BUBBLE,
            "Permissions.PredictionService.Action.Geolocation.Gesture.Loud"},
        PredictionServiceActionTestConfig{
            RequestType::kGeolocation, PermissionRequestGestureType::NO_GESTURE,
            PermissionAction::GRANTED,
            PermissionPromptDisposition::ANCHORED_BUBBLE,
            "Permissions.PredictionService.Action.Geolocation.NoGesture.Loud"},
        PredictionServiceActionTestConfig{
            RequestType::kGeolocation, PermissionRequestGestureType::GESTURE,
            PermissionAction::DENIED,
            PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_CHIP,
            "Permissions.PredictionService.Action.Geolocation.Gesture.Quiet"},
        PredictionServiceActionTestConfig{
            RequestType::kGeolocation, PermissionRequestGestureType::NO_GESTURE,
            PermissionAction::DISMISSED,
            PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_CHIP,
            "Permissions.PredictionService.Action.Geolocation.NoGesture."
            "Quiet"}));
}  // namespace permissions
