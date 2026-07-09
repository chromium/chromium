// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_metrics_helper.h"

#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/common/content_features.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

class IsolatedWebAppMetricsHelperTest : public WebAppTest {
 public:
  IsolatedWebAppMetricsHelperTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kIsolatedWebApps);
  }
  ~IsolatedWebAppMetricsHelperTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
};

TEST_F(IsolatedWebAppMetricsHelperTest, ReportNumInstalledSubApps) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> iwa =
      IsolatedWebAppBuilder(ManifestBuilder().SetName("IWA Parent App"))
          .BuildBundle();
  iwa->TrustSigningKey();
  iwa->FakeInstallPageState(profile());
  ASSERT_OK_AND_ASSIGN(auto url_info, iwa->Install(profile()));

  webapps::AppId parent_app_id = url_info.app_id();
  GURL parent_url = url_info.origin().GetURL();

  int num_sub_apps = 5;
  for (int i = 0; i < num_sub_apps; ++i) {
    GURL sub_app_url = parent_url.Resolve("sub-app" + base::NumberToString(i));
    auto sub_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(sub_app_url);
    sub_app_info->parent_app_id = parent_app_id;
    test::InstallWebApp(profile(), std::move(sub_app_info));
  }

  IsolatedWebAppMetricsHelper::ReportNumInstalledSubApps(
      fake_provider().registrar_unsafe());

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::SubApp_CountPerParent::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  const auto* entry = entries[0].get();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, parent_url);
  test_ukm_recorder_.ExpectEntryMetric(
      entry, ukm::builders::SubApp_CountPerParent::kSubAppsCountName, 5);
}

TEST_F(IsolatedWebAppMetricsHelperTest, ReportSubAppInstallResults) {
  GURL parent_url("isolated-app://foo/");
  url::Origin parent_origin = url::Origin::Create(parent_url);
  std::vector<IsolatedWebAppMetricsHelper::LogSubAppInstallResult> results = {
      IsolatedWebAppMetricsHelper::LogSubAppInstallResult::kSuccess,
      IsolatedWebAppMetricsHelper::LogSubAppInstallResult::kFailureGeneral,
  };

  IsolatedWebAppMetricsHelper::ReportSubAppInstallResults(parent_origin,
                                                          results);

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::SubApp_InstallResult::kEntryName);
  ASSERT_EQ(entries.size(), 2u);

  {
    const auto* entry = entries[0].get();
    test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, parent_url);
    test_ukm_recorder_.ExpectEntryMetric(
        entry, ukm::builders::SubApp_InstallResult::kResultName,
        static_cast<int64_t>(results[0]));
  }
  {
    const auto* entry = entries[1].get();
    test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, parent_url);
    test_ukm_recorder_.ExpectEntryMetric(
        entry, ukm::builders::SubApp_InstallResult::kResultName,
        static_cast<int64_t>(results[1]));
  }
}

TEST_F(IsolatedWebAppMetricsHelperTest, ReportSubAppSilentUpdateResult) {
  GURL sub_app_url("isolated-app://bar/");
  url::Origin sub_app_origin = url::Origin::Create(sub_app_url);
  auto result = ManifestSilentUpdateCheckResult::kAppSilentlyUpdated;

  IsolatedWebAppMetricsHelper::ReportSubAppSilentUpdateResult(sub_app_origin,
                                                              result);

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::SubApp_Update_ManifestSilentUpdateCheckResult::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  const auto* entry = entries[0].get();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, sub_app_url);
  test_ukm_recorder_.ExpectEntryMetric(
      entry,
      ukm::builders::SubApp_Update_ManifestSilentUpdateCheckResult::kResultName,
      static_cast<int64_t>(result));
}

TEST_F(IsolatedWebAppMetricsHelperTest, ReportSubAppPendingUpdateResult) {
  GURL sub_app_url("isolated-app://bar/");
  url::Origin sub_app_origin = url::Origin::Create(sub_app_url);
  auto result =
      ApplyPendingManifestUpdateResult::kAppNameAndIconsUpdatedSuccessfully;

  IsolatedWebAppMetricsHelper::ReportSubAppPendingUpdateResult(sub_app_origin,
                                                               result);

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::SubApp_Update_ApplyPendingManifestUpdateResult::
          kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  const auto* entry = entries[0].get();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, sub_app_url);
  test_ukm_recorder_.ExpectEntryMetric(
      entry,
      ukm::builders::SubApp_Update_ApplyPendingManifestUpdateResult::
          kResultName,
      static_cast<int64_t>(result));
}

}  // namespace web_app
