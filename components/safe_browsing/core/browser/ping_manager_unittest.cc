// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/ping_manager.h"

#include "base/base64.h"
#include "base/base64url.h"
#include "base/files/file_enumerator.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/safe_browsing/core/browser/db/v4_test_util.h"
#include "components/safe_browsing/core/browser/safe_browsing_hats_delegate.h"
#include "components/safe_browsing/core/browser/test_safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using network::GetUploadData;
using safe_browsing::HitReport;
using safe_browsing::ThreatSource;
using ::testing::_;

namespace safe_browsing {

using enum ExtendedReportingLevel;

class FakeSafeBrowsingHatsDelegate : public SafeBrowsingHatsDelegate {
 public:
  void LaunchRedWarningSurvey(
      const SurveyStringData& survey_string_data) override {
    survey_string_data_ = survey_string_data;
  }
  SurveyStringData GetSurveyStringData() { return survey_string_data_; }

 private:
  SurveyStringData survey_string_data_;
};
class MockWebUIDelegate : public PingManager::WebUIDelegate {
 public:
  MOCK_METHOD1(AddToCSBRRsSent,
               void(std::unique_ptr<ClientSafeBrowsingReportRequest> csbrr));
  MOCK_METHOD1(AddToHitReportsSent,
               void(std::unique_ptr<HitReport> hit_report));
};
class PingManagerTest : public testing::Test {
 protected:
  using enum SBThreatType;

  void SetUp() override;
  void TearDown() override;
  void RunReportThreatDetailsTest(
      bool expect_access_token,
      std::optional<ChromeUserPopulation> expected_user_population,
      std::optional<std::string> expected_page_load_token_value,
      bool expect_cookies_removed);
  PingManager* ping_manager();
  void SetNewPingManager(
      std::optional<base::RepeatingCallback<bool()>>
          get_should_fetch_access_token,
      std::optional<base::RepeatingCallback<ChromeUserPopulation()>>
          get_user_population_callback,
      std::optional<
          base::RepeatingCallback<ChromeUserPopulation::PageLoadToken(GURL)>>
          get_page_load_token_callback,
      std::optional<base::RepeatingCallback<bool()>>
          get_should_send_persisted_report);
  void SetUpFeatureList(bool should_enable_remove_cookies);
  // Returns a copy of the serialized persisted report that can be used to
  // verify the data sent through URL loader.
  std::string CallPersistThreatDetails(const std::string& url);

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::string key_param_;
  std::unique_ptr<MockWebUIDelegate> webui_delegate_ =
      std::make_unique<MockWebUIDelegate>();
  base::FilePath persister_root_path_;
  base::FilePath persister_dir_;
  FakeSafeBrowsingHatsDelegate* SetUpHatsDelegate();

 private:
  TestSafeBrowsingTokenFetcher* SetUpTokenFetcher();
  std::unique_ptr<PingManager> ping_manager_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<FakeSafeBrowsingHatsDelegate> hats_delegate_;
};

void PingManagerTest::SetUp() {
  std::string key = google_apis::GetAPIKey();
  if (!key.empty()) {
    key_param_ = base::StringPrintf(
        "&key=%s", base::EscapeQueryParamValue(key, true).c_str());
  }
  persister_root_path_ = base::CreateUniqueTempDirectoryScopedToTest();
  persister_dir_ = persister_root_path_.AppendASCII("DownloadReports");
  SetNewPingManager(std::nullopt, std::nullopt, std::nullopt, std::nullopt);
}

void PingManagerTest::TearDown() {
  base::RunLoop().RunUntilIdle();
  feature_list_.Reset();
}

PingManager* PingManagerTest::ping_manager() {
  return ping_manager_.get();
}

void PingManagerTest::SetNewPingManager(
    std::optional<base::RepeatingCallback<bool()>>
        get_should_fetch_access_token,
    std::optional<base::RepeatingCallback<ChromeUserPopulation()>>
        get_user_population_callback,
    std::optional<
        base::RepeatingCallback<ChromeUserPopulation::PageLoadToken(GURL)>>
        get_page_load_token_callback,
    std::optional<base::RepeatingCallback<bool()>>
        get_should_send_persisted_report) {
  ping_manager_.reset(new PingManager(
      safe_browsing::GetTestV4ProtocolConfig(), nullptr, nullptr,
      get_should_fetch_access_token.value_or(
          base::BindRepeating([]() { return false; })),
      webui_delegate_.get(), base::SequencedTaskRunner::GetCurrentDefault(),
      get_user_population_callback.value_or(base::NullCallback()),
      get_page_load_token_callback.value_or(base::NullCallback()), nullptr,
      persister_root_path_,
      get_should_send_persisted_report.value_or(
          base::BindRepeating([]() { return false; }))));
}

void PingManagerTest::SetUpFeatureList(bool should_enable_remove_cookies) {
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;
  if (should_enable_remove_cookies) {
    enabled_features.push_back(kSafeBrowsingRemoveCookiesInAuthRequests);
  } else {
    disabled_features.push_back(kSafeBrowsingRemoveCookiesInAuthRequests);
  }
  feature_list_.InitWithFeatures(enabled_features, disabled_features);
}

std::string PingManagerTest::CallPersistThreatDetails(const std::string& url) {
  std::unique_ptr<ClientSafeBrowsingReportRequest> report =
      std::make_unique<ClientSafeBrowsingReportRequest>();
  report->set_type(
      ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_PROFILE_CLOSED);
  report->set_url(url);
  std::string serialized_report;
  EXPECT_TRUE(report->SerializeToString(&serialized_report));

  PingManager::PersistThreatDetailsResult result =
      ping_manager()->PersistThreatDetailsAndReportOnNextStartup(
          std::move(report));
  EXPECT_EQ(result,
            PingManager::PersistThreatDetailsResult::kPersistTaskPosted);
  task_environment_.RunUntilIdle();
  return serialized_report;
}

TestSafeBrowsingTokenFetcher* PingManagerTest::SetUpTokenFetcher() {
  auto token_fetcher = std::make_unique<TestSafeBrowsingTokenFetcher>();
  auto* raw_token_fetcher = token_fetcher.get();
  ping_manager()->SetTokenFetcherForTesting(std::move(token_fetcher));
  return raw_token_fetcher;
}

FakeSafeBrowsingHatsDelegate* PingManagerTest::SetUpHatsDelegate() {
  auto hats_delegate = std::make_unique<FakeSafeBrowsingHatsDelegate>();
  auto* raw_hats_delegate = hats_delegate.get();
  ping_manager()->SetHatsDelegateForTesting(std::move(hats_delegate));
  return raw_hats_delegate;
}

void PingManagerTest::RunReportThreatDetailsTest(
    bool expect_access_token,
    std::optional<ChromeUserPopulation> expected_user_population,
    std::optional<std::string> expected_page_load_token_value,
    bool expect_cookies_removed) {
  base::HistogramTester histogram_tester;
  TestSafeBrowsingTokenFetcher* raw_token_fetcher = SetUpTokenFetcher();
  std::string input_report_content;
  std::unique_ptr<ClientSafeBrowsingReportRequest> report =
      std::make_unique<ClientSafeBrowsingReportRequest>();
  // The report must be non-empty. The selected property to set is arbitrary.
  report->set_type(ClientSafeBrowsingReportRequest::URL_PHISHING);
  EXPECT_TRUE(report->SerializeToString(&input_report_content));
  ClientSafeBrowsingReportRequest expected_report;
  expected_report.ParseFromString(input_report_content);
  if (expected_user_population.has_value()) {
    *expected_report.mutable_population() = expected_user_population.value();
  }
  if (expected_page_load_token_value.has_value()) {
    ChromeUserPopulation::PageLoadToken token =
        ChromeUserPopulation::PageLoadToken();
    token.set_token_value(expected_page_load_token_value.value());
    expected_report.mutable_population()
        ->mutable_page_load_tokens()
        ->Add()
        ->Swap(&token);
  }
  std::string expected_report_content;
  EXPECT_TRUE(expected_report.SerializeToString(&expected_report_content));

  std::string access_token = "testing_access_token";
  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_EQ(GetUploadData(request), expected_report_content);
        EXPECT_THAT(
            request.headers.GetHeader(net::HttpRequestHeaders::kAuthorization),
            testing::Conditional(expect_access_token,
                                 testing::Optional("Bearer " + access_token),
                                 std::nullopt));
        EXPECT_EQ(request.credentials_mode,
                  expect_cookies_removed
                      ? network::mojom::CredentialsMode::kOmit
                      : network::mojom::CredentialsMode::kInclude);
        histogram_tester.ExpectUniqueSample(
            "SafeBrowsing.ClientSafeBrowsingReport.RequestHasToken",
            /*sample=*/expect_access_token,
            /*expected_bucket_count=*/1);
      }));
  ping_manager()->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  EXPECT_CALL(*webui_delegate_.get(), AddToCSBRRsSent(_)).Times(1);
  PingManager::ReportThreatDetailsResult result =
      ping_manager()->ReportThreatDetails(std::move(report));
  EXPECT_EQ(result, PingManager::ReportThreatDetailsResult::SUCCESS);
  EXPECT_EQ(raw_token_fetcher->WasStartCalled(), expect_access_token);
  if (expect_access_token) {
    raw_token_fetcher->RunAccessTokenCallback(access_token);
  }
}

TEST_F(PingManagerTest, TestSafeBrowsingHitUrl) {
  HitReport base_hp;
  base_hp.malicious_url = GURL("http://malicious.url.com");
  base_hp.page_url = GURL("http://page.url.com");
  base_hp.referrer_url = GURL("http://referrer.url.com");

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_MALWARE;
    hp.threat_source = ThreatSource::LOCAL_PVER4;
    hp.is_subresource = true;
    hp.extended_reporting_level = SBER_LEVEL_LEGACY;
    hp.is_metrics_reporting_active = true;
    hp.is_enhanced_protection = true;

    EXPECT_EQ(
        "https://safebrowsing.google.com/safebrowsing/report?client=unittest&"
        "appver=1.0&pver=4.0" +
            key_param_ +
            "&ext=1&enh=1&evts=malblhit&evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=1&src=l4&m=1",
        ping_manager()->SafeBrowsingHitUrl(&hp).spec());
  }

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_PHISHING;
    hp.threat_source = ThreatSource::LOCAL_PVER4;
    hp.is_subresource = false;
    hp.extended_reporting_level = SBER_LEVEL_LEGACY;
    hp.is_metrics_reporting_active = true;
    hp.is_enhanced_protection = false;
    EXPECT_EQ(
        "https://safebrowsing.google.com/safebrowsing/report?client=unittest&"
        "appver=1.0&pver=4.0" +
            key_param_ +
            "&ext=1&evts=phishblhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0&src=l4&m=1",
        ping_manager()->SafeBrowsingHitUrl(&hp).spec());
  }

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_PHISHING;
    hp.threat_source = ThreatSource::LOCAL_PVER4;
    hp.is_subresource = false;
    hp.extended_reporting_level = SBER_LEVEL_SCOUT;
    hp.is_metrics_reporting_active = true;
    hp.is_enhanced_protection = true;
    EXPECT_EQ(
        "https://safebrowsing.google.com/safebrowsing/report?client=unittest&"
        "appver=1.0&pver=4.0" +
            key_param_ +
            "&ext=2&enh=1&evts=phishblhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0&src=l4&m=1",
        ping_manager()->SafeBrowsingHitUrl(&hp).spec());
  }

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING;
    hp.threat_source = ThreatSource::LOCAL_PVER4;
    hp.extended_reporting_level = SBER_LEVEL_OFF;
    hp.is_metrics_reporting_active = false;
    hp.is_subresource = false;
    hp.is_enhanced_protection = true;
    EXPECT_EQ(
        "https://safebrowsing.google.com/safebrowsing/report?client=unittest&"
        "appver=1.0&pver=4.0" +
            key_param_ +
            "&ext=0&enh=1&evts=phishcsdhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0&src=l4&m=0",
        ping_manager()->SafeBrowsingHitUrl(&hp).spec());
  }

  // Threat source is URL real-time check.
  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_PHISHING;
    hp.threat_source = ThreatSource::URL_REAL_TIME_CHECK;
    hp.is_subresource = false;
    hp.extended_reporting_level = SBER_LEVEL_SCOUT;
    hp.is_metrics_reporting_active = true;
    hp.is_enhanced_protection = true;
    EXPECT_EQ(
        "https://safebrowsing.google.com/safebrowsing/report?client=unittest&"
        "appver=1.0&pver=4.0" +
            key_param_ +
            "&ext=2&enh=1&evts=phishblhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0&src=rt&m=1",
        ping_manager()->SafeBrowsingHitUrl(&hp).spec());
  }

  // Threat source is native hash real-time check.
  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_PHISHING;
    hp.threat_source = ThreatSource::NATIVE_PVER5_REAL_TIME;
    hp.is_subresource = false;
    hp.extended_reporting_level = SBER_LEVEL_SCOUT;
    hp.is_metrics_reporting_active = false;
    hp.is_enhanced_protection = false;
    EXPECT_EQ(
        "https://safebrowsing.google.com/safebrowsing/report?client=unittest&"
        "appver=1.0&pver=4.0" +
            key_param_ +
            "&ext=2&evts=phishblhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0&src=n5rt&m=0",
        ping_manager()->SafeBrowsingHitUrl(&hp).spec());
  }

  // Threat source is Android hash real-time check.
  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_PHISHING;
    hp.threat_source = ThreatSource::ANDROID_SAFEBROWSING_REAL_TIME;
    hp.is_subresource = false;
    hp.extended_reporting_level = SBER_LEVEL_SCOUT;
    hp.is_metrics_reporting_active = false;
    hp.is_enhanced_protection = false;
    EXPECT_EQ(
        "https://safebrowsing.google.com/safebrowsing/report?client=unittest&"
        "appver=1.0&pver=4.0" +
            key_param_ +
            "&ext=2&evts=phishblhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0&src=asbrt&m=0",
        ping_manager()->SafeBrowsingHitUrl(&hp).spec());
  }

  // Threat source is Android local blocklist check.
  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_PHISHING;
    hp.threat_source = ThreatSource::ANDROID_SAFEBROWSING;
    hp.is_subresource = false;
    hp.extended_reporting_level = SBER_LEVEL_SCOUT;
    hp.is_metrics_reporting_active = false;
    hp.is_enhanced_protection = false;
    EXPECT_EQ(
        "https://safebrowsing.google.com/safebrowsing/report?client=unittest&"
        "appver=1.0&pver=4.0" +
            key_param_ +
            "&ext=2&evts=phishblhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0&src=asb&m=0",
        ping_manager()->SafeBrowsingHitUrl(&hp).spec());
  }
}

TEST_F(PingManagerTest, TestThreatDetailsUrl) {
  EXPECT_EQ(
      "https://safebrowsing.google.com/safebrowsing/clientreport/malware?"
      "client=unittest&appver=1.0&pver=4.0" +
          key_param_,
      ping_manager()->ThreatDetailsUrl().spec());
}

TEST_F(PingManagerTest, TestReportThreatDetails_EmptyReport) {
  std::unique_ptr<ClientSafeBrowsingReportRequest> report =
      std::make_unique<ClientSafeBrowsingReportRequest>();
  PingManager::ReportThreatDetailsResult result =
      ping_manager()->ReportThreatDetails(std::move(report));
  EXPECT_EQ(result, PingManager::ReportThreatDetailsResult::EMPTY_REPORT);
}

TEST_F(PingManagerTest, TestSanitizeThreatDetailsReport) {
  // Blank report.
  {
    std::unique_ptr<ClientSafeBrowsingReportRequest> report =
        std::make_unique<ClientSafeBrowsingReportRequest>();
    ping_manager()->SanitizeThreatDetailsReport(report.get());
    EXPECT_EQ(report->url(), "");
  }
  // One field needs sanitizing.
  {
    std::unique_ptr<ClientSafeBrowsingReportRequest> report =
        std::make_unique<ClientSafeBrowsingReportRequest>();
    report->set_url("http://user1:pass2@some.url.com/");
    report->set_page_url("http://some.page.url.com/");
    ping_manager()->SanitizeThreatDetailsReport(report.get());
    EXPECT_EQ(report->url(), "http://some.url.com/");
    EXPECT_EQ(report->page_url(), "http://some.page.url.com/");
  }
  // Multiple fields need sanitizing.
  {
    std::unique_ptr<ClientSafeBrowsingReportRequest> report =
        std::make_unique<ClientSafeBrowsingReportRequest>();
    report->set_url("http://user1:pass2@some.url.com/");
    report->set_page_url("http://u1:p2@some.page.url.com/");
    report->set_referrer_url("http://a:b@some.referrer.url.com/");
    report->add_resources()->set_url("http://c:d@first.resource.com/");
    report->add_resources();  // second resource has blank URL
    report->add_resources()->set_url("http://e:f@third.resource.com/");
    ping_manager()->SanitizeThreatDetailsReport(report.get());
    EXPECT_EQ(report->url(), "http://some.url.com/");
    EXPECT_EQ(report->page_url(), "http://some.page.url.com/");
    EXPECT_EQ(report->referrer_url(), "http://some.referrer.url.com/");
    EXPECT_EQ(report->resources(0).url(), "http://first.resource.com/");
    EXPECT_EQ(report->resources(1).url(), "");
    EXPECT_EQ(report->resources(2).url(), "http://third.resource.com/");
  }
}

TEST_F(PingManagerTest, TestSanitizeHitReport) {
  // Blank report.
  {
    std::unique_ptr<HitReport> hit_report = std::make_unique<HitReport>();
    ping_manager()->SanitizeHitReport(hit_report.get());
    EXPECT_EQ(hit_report->malicious_url.spec(), "");
    EXPECT_EQ(hit_report->page_url.spec(), "");
    EXPECT_EQ(hit_report->referrer_url.spec(), "");
  }
  // One field needs sanitizing.
  {
    std::unique_ptr<HitReport> hit_report = std::make_unique<HitReport>();
    hit_report->malicious_url = GURL("http://user1:pass2@malicious.url.com/");
    hit_report->page_url = GURL("http://page.url.com/");
    // Sanity check it is indeed the SanitizeHitReport method responsible for
    // the user1:pass2 being removed and not something about the URL being
    // invalid.
    EXPECT_EQ(hit_report->malicious_url.spec(),
              "http://user1:pass2@malicious.url.com/");
    ping_manager()->SanitizeHitReport(hit_report.get());
    EXPECT_EQ(hit_report->malicious_url.spec(), "http://malicious.url.com/");
    EXPECT_EQ(hit_report->page_url.spec(), "http://page.url.com/");
    EXPECT_EQ(hit_report->referrer_url.spec(), "");
  }
  // Multiple fields need sanitizing.
  {
    std::unique_ptr<HitReport> hit_report = std::make_unique<HitReport>();
    hit_report->malicious_url = GURL("http://user1:pass2@malicious.url.com/");
    hit_report->page_url = GURL("http://u1:p2@page.url.com/");
    hit_report->referrer_url = GURL("http://a:b@referrer.url.com/");
    ping_manager()->SanitizeHitReport(hit_report.get());
    EXPECT_EQ(hit_report->malicious_url.spec(), "http://malicious.url.com/");
    EXPECT_EQ(hit_report->page_url.spec(), "http://page.url.com/");
    EXPECT_EQ(hit_report->referrer_url.spec(), "http://referrer.url.com/");
  }
}

TEST_F(PingManagerTest, ReportThreatDetailsWithAccessToken) {
  SetNewPingManager(
      /*get_should_fetch_access_token=*/base::BindRepeating(
          []() { return true; }),
      /*get_user_population_callback=*/std::nullopt,
      /*get_page_load_token_callback=*/std::nullopt,
      /*get_should_send_persisted_report=*/std::nullopt);
  SetUpFeatureList(/*should_enable_remove_cookies=*/true);
  RunReportThreatDetailsTest(/*expect_access_token=*/true,
                             /*expected_user_population=*/std::nullopt,
                             /*expected_page_load_token_value=*/std::nullopt,
                             /*expect_cookies_removed=*/true);
}
TEST_F(PingManagerTest,
       ReportThreatDetailsWithAccessToken_RemoveCookiesFeatureDisabled) {
  SetNewPingManager(
      /*get_should_fetch_access_token=*/base::BindRepeating(
          []() { return true; }),
      /*get_user_population_callback=*/std::nullopt,
      /*get_page_load_token_callback=*/std::nullopt,
      /*get_should_send_persisted_report=*/std::nullopt);
  SetUpFeatureList(/*should_enable_remove_cookies=*/false);
  RunReportThreatDetailsTest(/*expect_access_token=*/true,
                             /*expected_user_population=*/std::nullopt,
                             /*expected_page_load_token_value=*/std::nullopt,
                             /*expect_cookies_removed=*/false);
}
TEST_F(PingManagerTest, ReportThreatDetailsWithUserPopulation) {
  SetNewPingManager(
      /*get_should_fetch_access_token=*/std::nullopt,
      /*get_user_population_callback=*/base::BindRepeating([]() {
        auto population = ChromeUserPopulation();
        population.set_user_population(ChromeUserPopulation::SAFE_BROWSING);
        return population;
      }),
      /*get_page_load_token_callback=*/std::nullopt,
      /*get_should_send_persisted_report=*/std::nullopt);
  auto population = ChromeUserPopulation();
  population.set_user_population(ChromeUserPopulation::SAFE_BROWSING);
  RunReportThreatDetailsTest(/*expect_access_token=*/false,
                             /*expected_user_population=*/population,
                             /*expected_page_load_token_value=*/std::nullopt,
                             /*expect_cookies_removed=*/false);
}
TEST_F(PingManagerTest, ReportThreatDetailsWithPageLoadToken) {
  base::HistogramTester histogram_tester;
  SetNewPingManager(
      /*get_should_fetch_access_token=*/std::nullopt,
      /*get_user_population_callback=*/std::nullopt,
      /*get_page_load_token_callback=*/base::BindRepeating([](GURL url) {
        ChromeUserPopulation::PageLoadToken token;
        token.set_token_value("testing_page_load_token");
        return token;
      }),
      /*get_should_send_persisted_report=*/std::nullopt);
  RunReportThreatDetailsTest(
      /*expect_access_token=*/false,
      /*expected_user_population=*/std::nullopt,
      /*expected_page_load_token_value=*/"testing_page_load_token",
      /*expect_cookies_removed=*/false);
}

TEST_F(PingManagerTest, PersistThreatDetailsAtShutdown) {
  base::HistogramTester histogram_tester;

  CallPersistThreatDetails("https://some.url.com/");
  ASSERT_TRUE(base::PathExists(persister_dir_));
  base::FileEnumerator directory_enumerator(persister_dir_,
                                            /*recursive=*/false,
                                            base::FileEnumerator::FILES);
  int number_of_files = 0;
  for (base::FilePath file_name = directory_enumerator.Next();
       !file_name.empty(); file_name = directory_enumerator.Next()) {
    number_of_files++;
    std::string persisted_report_string;
    bool success = base::ReadFileToString(file_name, &persisted_report_string);
    ASSERT_TRUE(success);
    auto persisted_report = std::make_unique<ClientSafeBrowsingReportRequest>();
    bool parse_successful =
        persisted_report->ParseFromString(persisted_report_string);
    ASSERT_TRUE(parse_successful);
    EXPECT_EQ(
        persisted_report->type(),
        ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_PROFILE_CLOSED);
    EXPECT_EQ(persisted_report->url(), "https://some.url.com/");
  }
  EXPECT_EQ(number_of_files, 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.ClientSafeBrowsingReport.PersisterWriteResult",
      /*sample=*/PingManager::Persister::WriteResult::kSuccess,
      /*expected_bucket_count=*/1);
}

TEST_F(PingManagerTest, PersistThreatDetailsAtShutdown_EmptyReport) {
  std::unique_ptr<ClientSafeBrowsingReportRequest> report =
      std::make_unique<ClientSafeBrowsingReportRequest>();
  PingManager::PersistThreatDetailsResult result =
      ping_manager()->PersistThreatDetailsAndReportOnNextStartup(
          std::move(report));
  EXPECT_EQ(result, PingManager::PersistThreatDetailsResult::kEmptyReport);
}

TEST_F(PingManagerTest, SendPersistedThreatDetailsOnStartup) {
  base::HistogramTester histogram_tester;
  std::string persisted_report1 =
      CallPersistThreatDetails("https://some.url1.com/");
  std::string persisted_report2 =
      CallPersistThreatDetails("https://some.url2.com/");
  EXPECT_TRUE(base::PathExists(persister_dir_));

  // Create a new ping manager instance to simulate browser startup.
  SetNewPingManager(
      /*get_should_fetch_access_token=*/std::nullopt,
      /*get_user_population_callback=*/std::nullopt,
      /*get_page_load_token_callback=*/std::nullopt,
      /*get_should_send_persisted_report=*/base::BindRepeating([]() {
        return true;
      }));

  network::TestURLLoaderFactory test_url_loader_factory;
  bool report1_sent = false, report2_sent = false;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        std::string upload_data = GetUploadData(request);
        if (upload_data == persisted_report1) {
          report1_sent = true;
        } else if (upload_data == persisted_report2) {
          report2_sent = true;
        }
      }));
  ping_manager()->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));
  EXPECT_CALL(*webui_delegate_.get(), AddToCSBRRsSent(_)).Times(2);

  // Request is sent after delay.
  task_environment_.FastForwardBy(base::Seconds(14));
  EXPECT_EQ(test_url_loader_factory.total_requests(), 0u);
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(test_url_loader_factory.total_requests(), 2u);
  EXPECT_TRUE(report1_sent);
  EXPECT_TRUE(report2_sent);
  // Directory is deleted.
  EXPECT_FALSE(base::PathExists(persister_dir_));
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.ClientSafeBrowsingReport.PersisterReadReportSuccessful",
      /*sample=*/true,
      /*expected_bucket_count=*/2);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.ClientSafeBrowsingReport.PersisterReportCountOnStartup",
      /*sample=*/2,
      /*expected_bucket_count=*/1);
}

TEST_F(PingManagerTest, SendPersistedThreatDetailsOnStartup_MalformedReports) {
  base::CreateDirectory(persister_dir_);
  base::FilePath empty_file = persister_dir_.AppendASCII("empty");
  base::WriteFile(empty_file, "");
  base::FilePath malformed_file = persister_dir_.AppendASCII("malformed");
  base::WriteFile(malformed_file, "malformed_report");

  // Create a new ping manager instance to simulate browser startup.
  SetNewPingManager(
      /*get_should_fetch_access_token=*/std::nullopt,
      /*get_user_population_callback=*/std::nullopt,
      /*get_page_load_token_callback=*/std::nullopt,
      /*get_should_send_persisted_report=*/base::BindRepeating([]() {
        return true;
      }));

  network::TestURLLoaderFactory test_url_loader_factory;
  ping_manager()->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  task_environment_.FastForwardBy(base::Seconds(15));
  // Persisted report should not be sent because their content is invalid.
  EXPECT_EQ(test_url_loader_factory.total_requests(), 0u);
  // Persisted report should still be deleted from disk.
  EXPECT_FALSE(base::PathExists(persister_dir_));
}

TEST_F(PingManagerTest, SendPersistedThreatDetailsOnStartup_EmptyDirectory) {
  base::CreateDirectory(persister_dir_);

  // Create a new ping manager instance to simulate browser startup.
  SetNewPingManager(
      /*get_should_fetch_access_token=*/std::nullopt,
      /*get_user_population_callback=*/std::nullopt,
      /*get_page_load_token_callback=*/std::nullopt,
      /*get_should_send_persisted_report=*/base::BindRepeating([]() {
        return true;
      }));

  network::TestURLLoaderFactory test_url_loader_factory;
  ping_manager()->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  task_environment_.FastForwardBy(base::Seconds(15));
  // The directory should be cleaned up.
  EXPECT_FALSE(base::PathExists(persister_dir_));
}

TEST_F(PingManagerTest,
       SendPersistedThreatDetailsOnStartup_ShouldNotSendReport) {
  CallPersistThreatDetails("https://some.url1.com/");
  EXPECT_TRUE(base::PathExists(persister_dir_));

  // Create a new ping manager instance to simulate browser startup.
  SetNewPingManager(
      /*get_should_fetch_access_token=*/std::nullopt,
      /*get_user_population_callback=*/std::nullopt,
      /*get_page_load_token_callback=*/std::nullopt,
      /*get_should_send_persisted_report=*/base::BindRepeating([]() {
        return false;
      }));

  network::TestURLLoaderFactory test_url_loader_factory;
  ping_manager()->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  task_environment_.FastForwardBy(base::Seconds(15));
  // Persisted report should not be sent because
  // get_should_send_persisted_report is false.
  EXPECT_EQ(test_url_loader_factory.total_requests(), 0u);
  // Persisted report should still be deleted from disk.
  EXPECT_FALSE(base::PathExists(persister_dir_));
}

TEST_F(PingManagerTest, ReportSafeBrowsingHit) {
  std::unique_ptr<HitReport> hit_report = std::make_unique<HitReport>();
  std::string post_data = "testing_hit_report_post_data";
  hit_report->post_data = post_data;
  // Threat type, threat source and other fields are arbitrary but specified so
  // that determining the URL does not throw an error due to input validation.
  hit_report->threat_type = SB_THREAT_TYPE_URL_PHISHING;
  hit_report->threat_source = ThreatSource::LOCAL_PVER4;
  hit_report->is_subresource = false;
  hit_report->extended_reporting_level = SBER_LEVEL_SCOUT;
  hit_report->is_metrics_reporting_active = false;
  hit_report->is_enhanced_protection = false;

  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_EQ(GetUploadData(request), post_data);
      }));
  ping_manager()->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  EXPECT_CALL(*webui_delegate_.get(), AddToHitReportsSent(_)).Times(1);
  ping_manager()->ReportSafeBrowsingHit(std::move(hit_report));
}

TEST_F(PingManagerTest, AttachThreatDetailsAndLaunchSurvey) {
  auto report = std::make_unique<ClientSafeBrowsingReportRequest>();
  report->set_type(ClientSafeBrowsingReportRequest::URL_CLIENT_SIDE_PHISHING);
  report->set_url("http://url.com");
  report->set_page_url("http://page-url.com");
  report->set_referrer_url("http://referrer-url.com");
  SetNewPingManager(
      /*get_should_fetch_access_token=*/base::BindRepeating(
          []() { return true; }),
      /*get_user_population_callback=*/base::BindRepeating([]() {
        auto population = ChromeUserPopulation();
        population.set_user_population(ChromeUserPopulation::SAFE_BROWSING);
        return population;
      }),
      /*get_page_load_token_callback=*/base::BindRepeating([](GURL url) {
        ChromeUserPopulation::PageLoadToken token;
        token.set_token_value("testing_page_load_token");
        return token;
      }),
      /*get_should_send_persisted_report=*/std::nullopt);
  FakeSafeBrowsingHatsDelegate* raw_fake_sb_hats_delegate = SetUpHatsDelegate();
  ping_manager()->AttachThreatDetailsAndLaunchSurvey(std::move(report));
  std::string deserialized_report_string;
  EXPECT_TRUE(base::Base64UrlDecode(
      raw_fake_sb_hats_delegate->GetSurveyStringData()[kUserActivityWithUrls],
      base::Base64UrlDecodePolicy::IGNORE_PADDING,
      &deserialized_report_string));
  ClientSafeBrowsingReportRequest actual_report;
  actual_report.ParseFromString(deserialized_report_string);
  EXPECT_EQ(actual_report.type(),
            ClientSafeBrowsingReportRequest::URL_CLIENT_SIDE_PHISHING);
  EXPECT_EQ(actual_report.population().user_population(),
            ChromeUserPopulation::SAFE_BROWSING);
  EXPECT_EQ(actual_report.population().page_load_tokens()[0].token_value(),
            "testing_page_load_token");
  EXPECT_EQ(raw_fake_sb_hats_delegate->GetSurveyStringData()[kFlaggedUrl],
            "http://url.com/");
  EXPECT_EQ(raw_fake_sb_hats_delegate->GetSurveyStringData()[kMainFrameUrl],
            "http://page-url.com/");
  EXPECT_EQ(raw_fake_sb_hats_delegate->GetSurveyStringData()[kReferrerUrl],
            "http://referrer-url.com/");
}

}  // namespace safe_browsing
