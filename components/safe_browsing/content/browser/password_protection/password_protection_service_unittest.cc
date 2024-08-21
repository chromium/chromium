// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/safe_browsing/content/browser/password_protection/password_protection_service.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/null_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/safe_browsing/content/browser/client_side_detection_feature_cache.h"
#include "components/safe_browsing/content/browser/password_protection/mock_password_protection_service.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_commit_deferring_condition.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_request_content.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom-forward.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/safe_browsing/core/browser/password_protection/metrics_util.h"
#include "components/safe_browsing/core/browser/password_protection/password_protection_request.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

using base::ASCIIToUTF16;
using testing::_;
using testing::AnyNumber;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

namespace safe_browsing {

namespace {

const char kFormActionUrl[] = "https://form_action.com/";
const char kPasswordFrameUrl[] = "https://password_frame.com/";
const char kSavedDomain[] = "http://saved_domain.com";
const char kSavedDomain2[] = "http://saved_domain2.com";
const char kTargetUrl[] = "http://foo.com/";
const char kUserName[] = "username";

using PasswordReuseEvent = LoginReputationClientRequest::PasswordReuseEvent;

class MockSafeBrowsingTokenFetcher : public SafeBrowsingTokenFetcher {
 public:
  MockSafeBrowsingTokenFetcher() = default;
  MockSafeBrowsingTokenFetcher(const MockSafeBrowsingTokenFetcher&) = delete;
  MockSafeBrowsingTokenFetcher& operator=(const MockSafeBrowsingTokenFetcher&) =
      delete;
  ~MockSafeBrowsingTokenFetcher() override = default;

  MOCK_METHOD1(Start, void(Callback));
  MOCK_METHOD1(OnInvalidAccessToken, void(const std::string&));
};

class MockSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager()
      : TestSafeBrowsingDatabaseManager(content::GetUIThreadTaskRunner({})) {}
  MockSafeBrowsingDatabaseManager(const MockSafeBrowsingDatabaseManager&) =
      delete;
  MockSafeBrowsingDatabaseManager& operator=(
      const MockSafeBrowsingDatabaseManager&) = delete;

  MOCK_METHOD2(CheckCsdAllowlistUrl,
               AsyncMatch(const GURL&, SafeBrowsingDatabaseManager::Client*));

  // Override to silence not implemented warnings.
  bool CanCheckUrl(const GURL& url) const override {
    return (url != GURL("about:blank"));
  }

 protected:
  ~MockSafeBrowsingDatabaseManager() override = default;
};

class TestPhishingDetector : public mojom::PhishingDetector {
 public:
  TestPhishingDetector() = default;
  TestPhishingDetector(const TestPhishingDetector&) = delete;
  TestPhishingDetector& operator=(const TestPhishingDetector&) = delete;
  ~TestPhishingDetector() override = default;

  void BindReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receivers_.Add(this,
                   mojo::PendingAssociatedReceiver<mojom::PhishingDetector>(
                       std::move(handle)));
  }

  void StartPhishingDetection(
      const GURL& url,
      StartPhishingDetectionCallback callback) override {
    if (should_timeout_) {
      deferred_callbacks_.push_back(std::move(callback));
    } else {
      ReturnFeatures(url, std::move(callback));
    }
  }
  void ReturnFeatures(const GURL& url,
                      StartPhishingDetectionCallback callback) {
    ClientPhishingRequest verdict;
    verdict.set_is_phishing(false);
    verdict.set_client_score(0.1);

    std::move(callback).Run(mojom::PhishingDetectorResult::SUCCESS,
                            mojo_base::ProtoWrapper(verdict));
  }

  void set_should_timeout(bool timeout) { should_timeout_ = timeout; }

 private:
  bool should_timeout_ = false;
  std::vector<StartPhishingDetectionCallback> deferred_callbacks_;
  mojo::AssociatedReceiverSet<mojom::PhishingDetector> receivers_;
};

class TestPasswordProtectionService : public MockPasswordProtectionService {
 public:
  TestPasswordProtectionService(
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      scoped_refptr<HostContentSettingsMap> content_setting_map,
      PrefService* pref_service,
      std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
      bool is_off_the_record,
      signin::IdentityManager* identity_manager,
      bool try_token_fetch = true)
      : MockPasswordProtectionService(database_manager,
                                      url_loader_factory,
                                      nullptr,
                                      pref_service,
                                      std::move(token_fetcher),
                                      is_off_the_record,
                                      identity_manager,
                                      try_token_fetch,
                                      nullptr),
        cache_manager_(
            std::make_unique<VerdictCacheManager>(/*history_service=*/nullptr,
                                                  content_setting_map.get(),
                                                  pref_service,
                                                  /*sync_observer=*/nullptr)) {
    cache_manager_->StopCleanUpTimerForTesting();
  }
  TestPasswordProtectionService(const TestPasswordProtectionService&) = delete;
  TestPasswordProtectionService& operator=(
      const TestPasswordProtectionService&) = delete;

  void RequestFinished(
      PasswordProtectionRequest* request,
      RequestOutcome outcome,
      std::unique_ptr<LoginReputationClientResponse> response) override {
    latest_request_ = request;
    latest_response_ = std::move(response);
    run_loop_.Quit();
  }

  LoginReputationClientResponse* latest_response() {
    return latest_response_.get();
  }

  void WaitForResponse() { run_loop_.Run(); }

  ~TestPasswordProtectionService() override = default;

  size_t GetPendingRequestsCount() { return pending_requests_.size(); }

  const LoginReputationClientRequest* GetLatestRequestProto() {
    return latest_request_ ? latest_request_->request_proto() : nullptr;
  }

  void CacheVerdict(const GURL& url,
                    LoginReputationClientRequest::TriggerType trigger_type,
                    ReusedPasswordAccountType password_type,
                    const LoginReputationClientResponse& verdict,
                    const base::Time& receive_time) override {
    if (!CanGetReputationOfURL(url) || IsIncognito())
      return;

    cache_manager_->CachePhishGuardVerdict(trigger_type, password_type, verdict,
                                           receive_time);
  }

  void InitTestApi(content::RenderFrameHost* rfh) {
    rfh->GetRemoteAssociatedInterfaces()->OverrideBinderForTesting(
        mojom::PhishingDetector::Name_,
        base::BindRepeating(&TestPhishingDetector::BindReceiver,
                            base::Unretained(&test_phishing_detector_)));
  }

  LoginReputationClientResponse::VerdictType GetCachedVerdict(
      const GURL& url,
      LoginReputationClientRequest::TriggerType trigger_type,
      ReusedPasswordAccountType password_type,
      LoginReputationClientResponse* out_response) override {
    if (!url.is_valid() || !CanGetReputationOfURL(url))
      return LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED;

    return cache_manager_->GetCachedPhishGuardVerdict(
        url, trigger_type, password_type, out_response);
  }

  int GetStoredVerdictCount(
      LoginReputationClientRequest::TriggerType trigger_type) override {
    return cache_manager_->GetStoredPhishGuardVerdictCount(trigger_type);
  }

  void SetDomFeatureCollectionTimeout(bool should_timeout) {
    test_phishing_detector_.set_should_timeout(should_timeout);
  }

 private:
  raw_ptr<PasswordProtectionRequest> latest_request_;
  base::RunLoop run_loop_;
  std::unique_ptr<LoginReputationClientResponse> latest_response_;
  TestPhishingDetector test_phishing_detector_;

  // The TestPasswordProtectionService manages its own cache, rather than using
  // the global one.
  std::unique_ptr<VerdictCacheManager> cache_manager_;
};

}  // namespace

class PasswordProtectionServiceTest : public ::testing::Test {
 public:
  PasswordProtectionServiceTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  void SetUp() override {
    HostContentSettingsMap::RegisterProfilePrefs(test_pref_service_.registry());
    content_setting_map_ = new HostContentSettingsMap(
        &test_pref_service_, /*is_off_the_record=*/false,
        /*store_last_modified=*/false, /*restore_session=*/false,
        /*should_record_metrics=*/false);
    database_manager_ = new MockSafeBrowsingDatabaseManager();
    password_protection_service_ =
        std::make_unique<NiceMock<TestPasswordProtectionService>>(
            database_manager_,
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_),
            content_setting_map_, nullptr, nullptr, false, nullptr, false);
    web_contents_ =
        base::WrapUnique(content::WebContentsTester::CreateTestWebContents(
            content::WebContents::CreateParams(&browser_context_)));
    const std::vector<password_manager::MatchingReusedCredential>
        matching_reused_credentials = {};
    content::RenderFrameHost* rfh = web_contents_->GetPrimaryMainFrame();
    password_protection_service_->InitTestApi(rfh);
    request_ =
        base::MakeRefCounted<safe_browsing::PasswordProtectionRequestContent>(
            web_contents_.get(), GURL(kTargetUrl),
            /*password_form_action=*/GURL(),
            /*password_form_frame_url=*/GURL(),
            web_contents_->GetContentsMimeType(), kUserName,
            PasswordType::PASSWORD_TYPE_UNKNOWN, matching_reused_credentials,
            LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
            /*password_field_exists=*/true, password_protection_service_.get(),
            /*request_timeout_in_ms=*/10000);
  }

  void TearDown() override {
    password_protection_service_.reset();
    content_setting_map_->ShutdownOnUIThread();
  }

  size_t GetNumberOfDeferredNavigations() {
    return request_ ? request_->deferred_navigations_.size() : 0u;
  }

  // |task_environment_| is needed here because this test involves both UI and
  // IO threads.
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<MockSafeBrowsingDatabaseManager> database_manager_;
  sync_preferences::TestingPrefServiceSyncable test_pref_service_;
  scoped_refptr<HostContentSettingsMap> content_setting_map_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestPasswordProtectionService> password_protection_service_;
  content::TestBrowserContext browser_context_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<content::WebContents> web_contents_;
  scoped_refptr<PasswordProtectionRequestContent> request_;
};

TEST_F(PasswordProtectionServiceTest,
       VerifyCommitDeferringConditionNotRemovedWhenCanceledOnTimeout) {
  request_->Start();
  content::MockNavigationHandle mock_handle;
  auto condition = std::make_unique<PasswordProtectionCommitDeferringCondition>(
      mock_handle, *request_.get());
  EXPECT_EQ(1U, GetNumberOfDeferredNavigations());
  request_->Cancel(/*timed_out=*/true);
  EXPECT_EQ(1U, GetNumberOfDeferredNavigations());
}

TEST_F(PasswordProtectionServiceTest,
       VerifyCommitDeferringConditionRemovedWhenCanceledNotOnTimeout) {
  request_->Start();
  content::MockNavigationHandle mock_handle;
  auto condition = std::make_unique<PasswordProtectionCommitDeferringCondition>(
      mock_handle, *request_.get());
  EXPECT_EQ(1U, GetNumberOfDeferredNavigations());
  request_->Cancel(/*timed_out=*/false);
  EXPECT_EQ(0U, GetNumberOfDeferredNavigations());
}

TEST_F(PasswordProtectionServiceTest, NoSendPingPrivateIpHostname) {
  EXPECT_CALL(*password_protection_service_, IsPingingEnabled(_, _))
      .WillRepeatedly(Return(true));
  ReusedPasswordAccountType reused_password_type;
  reused_password_type.set_account_type(ReusedPasswordAccountType::GMAIL);
  EXPECT_FALSE(password_protection_service_->CanSendPing(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      GURL("http://127.0.0.1"), reused_password_type));
  EXPECT_FALSE(password_protection_service_->CanSendPing(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      GURL("http://192.168.1.1"), reused_password_type));
}

class PasswordProtectionServiceBaseTest
    : public ::testing::TestWithParam<bool> {
 public:
  PasswordProtectionServiceBaseTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  LoginReputationClientResponse CreateVerdictProto(
      LoginReputationClientResponse::VerdictType verdict,
      base::TimeDelta cache_duration,
      const std::string& cache_expression) {
    LoginReputationClientResponse verdict_proto;
    verdict_proto.set_verdict_type(verdict);
    verdict_proto.set_cache_duration_sec(cache_duration.InSeconds());
    verdict_proto.set_cache_expression(cache_expression);
    return verdict_proto;
  }

  void SetUp() override {
    HostContentSettingsMap::RegisterProfilePrefs(test_pref_service_.registry());
    safe_browsing::RegisterProfilePrefs(test_pref_service_.registry());
    content_setting_map_ = new HostContentSettingsMap(
        &test_pref_service_, false /* is_off_the_record */,
        false /* store_last_modified */, false /* restore_session*/,
        false /* should_record_metrics */);
    database_manager_ = new MockSafeBrowsingDatabaseManager();
    auto token_fetcher =
        std::make_unique<StrictMock<MockSafeBrowsingTokenFetcher>>();
    raw_token_fetcher_ = token_fetcher.get();
    identity_test_env_.MakePrimaryAccountAvailable(
        "user@gmail.com", signin::ConsentLevel::kSignin);
    password_protection_service_ =
        std::make_unique<NiceMock<TestPasswordProtectionService>>(
            database_manager_,
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_),
            content_setting_map_, &test_pref_service_, std::move(token_fetcher),
            /*is_off_the_record=*/false, identity_test_env_.identity_manager(),
            /*try_token_fetch=*/true);
    EXPECT_CALL(*password_protection_service_, IsExtendedReporting())
        .WillRepeatedly(Return(GetParam()));
    EXPECT_CALL(*password_protection_service_, IsIncognito())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*password_protection_service_,
                IsURLAllowlistedForPasswordEntry(_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*password_protection_service_,
                GetPasswordProtectionWarningTriggerPref(_))
        .WillRepeatedly(Return(PASSWORD_PROTECTION_OFF));
    url_ = PasswordProtectionServiceBase::GetPasswordProtectionRequestUrl();
  }

  void TearDown() override {
    password_protection_service_.reset();
    content_setting_map_->ShutdownOnUIThread();
  }

  // Sets up |database_manager_| and |pending_requests_| as needed.
  void InitializeAndStartPasswordOnFocusRequest(
      bool match_allowlist,
      int timeout_in_ms,
      content::WebContents* web_contents) {
    GURL target_url(kTargetUrl);
    EXPECT_CALL(*database_manager_, CheckCsdAllowlistUrl(target_url, _))
        .WillRepeatedly(
            Return(match_allowlist ? AsyncMatch::MATCH : AsyncMatch::NO_MATCH));
    password_protection_service_->InitTestApi(
        web_contents->GetPrimaryMainFrame());
    request_ = new PasswordProtectionRequestContent(
        web_contents, target_url, GURL(kFormActionUrl), GURL(kPasswordFrameUrl),
        web_contents->GetContentsMimeType(), kUserName,
        PasswordType::PASSWORD_TYPE_UNKNOWN, {},
        LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE, true,
        password_protection_service_.get(), timeout_in_ms);
    request_->Start();
  }

  void InitializeAndStartPasswordEntryRequest(
      PasswordType type,
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials,
      bool match_allowlist,
      int timeout_in_ms,
      content::WebContents* web_contents) {
    GURL target_url(kTargetUrl);
    EXPECT_CALL(*database_manager_, CheckCsdAllowlistUrl(target_url, _))
        .WillRepeatedly(
            Return(match_allowlist ? AsyncMatch::MATCH : AsyncMatch::NO_MATCH));

    request_ = new PasswordProtectionRequestContent(
        web_contents, target_url, GURL(), GURL(),
        web_contents->GetContentsMimeType(), kUserName, type,
        matching_reused_credentials,
        LoginReputationClientRequest::PASSWORD_REUSE_EVENT, true,
        password_protection_service_.get(), timeout_in_ms);
    request_->Start();
  }

  void CacheVerdict(const GURL& url,
                    LoginReputationClientRequest::TriggerType trigger,
                    ReusedPasswordAccountType password_type,
                    LoginReputationClientResponse::VerdictType verdict,
                    base::TimeDelta cache_duration,
                    const std::string& cache_expression,
                    const base::Time& verdict_received_time) {
    ASSERT_FALSE(cache_expression.empty());
    LoginReputationClientResponse response(
        CreateVerdictProto(verdict, cache_duration, cache_expression));
    password_protection_service_->CacheVerdict(url, trigger, password_type,
                                               response, verdict_received_time);
  }

  void CacheInvalidVerdict(ReusedPasswordAccountType password_type) {
    GURL invalid_hostname("http://invalid.com");
    base::Value verdict = content_setting_map_->GetWebsiteSetting(
        invalid_hostname, GURL(), ContentSettingsType::PASSWORD_PROTECTION,
        nullptr);

    auto verdict_dictionary = base::Value::Dict();
    if (verdict.is_dict()) {
      verdict_dictionary = std::move(verdict).TakeDict();
    }
    verdict_dictionary.Set(
        base::NumberToString(static_cast<std::underlying_type_t<PasswordType>>(
            password_protection_service_
                ->ConvertReusedPasswordAccountTypeToPasswordType(
                    password_type))),
        base::Value::Dict().Set(
            "invalid_cache_expression",
            base::Value::Dict().Set("invalid", "invalid_string")));

    content_setting_map_->SetWebsiteSettingDefaultScope(
        invalid_hostname, GURL(), ContentSettingsType::PASSWORD_PROTECTION,
        base::Value(std::move(verdict_dictionary)));
  }

  size_t GetStoredVerdictCount(LoginReputationClientRequest::TriggerType type) {
    return password_protection_service_->GetStoredVerdictCount(type);
  }

  std::unique_ptr<content::WebContents> GetWebContents() {
    std::unique_ptr<content::WebContents> contents =
        base::WrapUnique(content::WebContentsTester::CreateTestWebContents(
            content::WebContents::CreateParams(&browser_context_)));
    // Initiate the connection to a (pretend) renderer process.
    content::WebContentsTester::For(contents.get())
        ->NavigateAndCommit(GURL("about:blank"));
    return contents;
  }

  void SetFeatures(
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features) {
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

// Visual features are not supported on Android.
#if !BUILDFLAG(IS_ANDROID)
  void VerifyContentAreaSizeCollection(
      const LoginReputationClientRequest& request) {
    bool should_report_content_size =
        password_protection_service_->IsExtendedReporting() &&
        !password_protection_service_->IsIncognito();
    EXPECT_EQ(should_report_content_size, request.has_content_area_height());
    EXPECT_EQ(should_report_content_size, request.has_content_area_width());
  }
#endif

  const LoginReputationClientRequest* SetUpFinchActiveGroupsTest(
      std::vector<std::string> feature_names,
      std::string group_name) {
    std::vector<std::string> enable_features_list;
    for (const auto& feature_name : feature_names) {
      base::FieldTrialList::CreateFieldTrial(feature_name, group_name);
      enable_features_list.push_back(
          base::StrCat({feature_name, "<", feature_name, ".", group_name}));
    }
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitFromCommandLine(
        base::JoinString(enable_features_list, ","), "");

    LoginReputationClientResponse expected_response =
        CreateVerdictProto(LoginReputationClientResponse::PHISHING,
                           base::Minutes(10), GURL("about:blank").host());
    test_url_loader_factory_.AddResponse(url_.spec(),
                                         expected_response.SerializeAsString());
    std::unique_ptr<content::WebContents> web_contents = GetWebContents();
    password_protection_service_->StartRequest(
        web_contents.get(), GURL("about:blank"), GURL(), GURL(), kUserName,
        PasswordType::SAVED_PASSWORD, {{"example.com", u"username"}},
        LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE, true);
    base::RunLoop().RunUntilIdle();

    password_protection_service_->WaitForResponse();
    const LoginReputationClientRequest* proto =
        password_protection_service_->GetLatestRequestProto();
    return proto;
  }

 protected:
  // |task_environment_| is needed here because this test involves both UI and
  // IO threads.
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<MockSafeBrowsingDatabaseManager> database_manager_;
  sync_preferences::TestingPrefServiceSyncable test_pref_service_;
  scoped_refptr<HostContentSettingsMap> content_setting_map_;
  GURL url_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestPasswordProtectionService> password_protection_service_;
  scoped_refptr<PasswordProtectionRequest> request_;
  base::HistogramTester histograms_;
  content::TestBrowserContext browser_context_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  raw_ptr<StrictMock<MockSafeBrowsingTokenFetcher>, DanglingUntriaged>
      raw_token_fetcher_ = nullptr;
  base::test::ScopedFeatureList feature_list_;
  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_P(PasswordProtectionServiceBaseTest, TestCachePasswordReuseVerdicts) {
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  EXPECT_CALL(*password_protection_service_, IsPrimaryAccountSignedIn())
      .WillRepeatedly(Return(true));
  // Assume each verdict has a TTL of 10 minutes.
  // Cache a verdict for http://www.test.com/foo/index.html
  ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::GSUITE);
  reused_password_account_type.set_is_account_syncing(true);
  CacheVerdict(GURL("http://www.test.com/foo/index.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, base::Minutes(10),
               "test.com/foo/", base::Time::Now());

  EXPECT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  // Cache another verdict with the some origin and cache_expression should
  // override the cache.
  CacheVerdict(GURL("http://www.test.com/foo/index2.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::PHISHING, base::Minutes(10),
               "test.com/foo/", base::Time::Now());
  EXPECT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  LoginReputationClientResponse out_verdict;
  EXPECT_EQ(LoginReputationClientResponse::PHISHING,
            password_protection_service_->GetCachedVerdict(
                GURL("http://www.test.com/foo/index2.html"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                reused_password_account_type, &out_verdict));

  // Cache a password reuse verdict with a different password type but same
  // origin and cache expression should add a new entry.
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  CacheVerdict(GURL("http://www.test.com/foo/index2.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::PHISHING, base::Minutes(10),
               "test.com/foo/", base::Time::Now());
  EXPECT_EQ(2U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  EXPECT_EQ(LoginReputationClientResponse::PHISHING,
            password_protection_service_->GetCachedVerdict(
                GURL("http://www.test.com/foo/index2.html"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                reused_password_account_type, &out_verdict));

  // Cache another verdict with the same origin but different cache_expression
  // will not increase setting count, but will increase the number of verdicts
  // in the given origin.
  CacheVerdict(GURL("http://www.test.com/bar/index2.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, base::Minutes(10),
               "test.com/bar/", base::Time::Now());
  EXPECT_EQ(3U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  // Now cache a UNFAMILIAR_LOGIN_PAGE verdict, stored verdict count for
  // PASSWORD_REUSE_EVENT should be the same.
  CacheVerdict(GURL("http://www.test.com/foobar/index3.html"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, base::Minutes(10),
               "test.com/foobar/", base::Time::Now());
  EXPECT_EQ(3U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  EXPECT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
}

TEST_P(PasswordProtectionServiceBaseTest,
       TestCachePasswordReuseVerdictsIncognito) {
  EXPECT_CALL(*password_protection_service_, IsIncognito())
      .WillRepeatedly(Return(true));
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::GSUITE);
  reused_password_account_type.set_is_account_syncing(true);
  // No verdict will be cached for incognito profile.
  CacheVerdict(GURL("http://www.test.com/foo/index.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, base::Minutes(10),
               "test.com/foo/", base::Time::Now());

  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  // Try cache another verdict with the some origin and cache_expression.
  // Verdict count should not increase.
  CacheVerdict(GURL("http://www.test.com/foo/index2.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::PHISHING, base::Minutes(10),
               "test.com/foo/", base::Time::Now());
  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));

  // Now cache a UNFAMILIAR_LOGIN_PAGE verdict, verdict count should not
  // increase.
  CacheVerdict(GURL("http://www.test.com/foobar/index3.html"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, base::Minutes(10),
               "test.com/foobar/", base::Time::Now());
  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
}

TEST_P(PasswordProtectionServiceBaseTest, TestCacheUnfamiliarLoginVerdicts) {
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
  ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::UNKNOWN);
  reused_password_account_type.set_is_account_syncing(true);
  // Assume each verdict has a TTL of 10 minutes.
  // Cache a verdict for http://www.test.com/foo/index.html
  CacheVerdict(GURL("http://www.test.com/foo/index.html"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, base::Minutes(10),
               "test.com/foo/", base::Time::Now());

  EXPECT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  // Cache another verdict with the same origin but different cache_expression
  // will not increase setting count, but will increase the number of verdicts
  // in the given origin.
  CacheVerdict(GURL("http://www.test.com/bar/index2.html"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, base::Minutes(10),
               "test.com/bar/", base::Time::Now());
  EXPECT_EQ(2U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  // Now cache a PASSWORD_REUSE_EVENT verdict, stored verdict count for
  // UNFAMILIAR_LOGIN_PAGE should be the same.
  CacheVerdict(GURL("http://www.test.com/foobar/index3.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, base::Minutes(10),
               "test.com/foobar/", base::Time::Now());
  EXPECT_EQ(2U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
  EXPECT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
}

TEST_P(PasswordProtectionServiceBaseTest,
       TestCacheUnfamiliarLoginVerdictsIncognito) {
  EXPECT_CALL(*password_protection_service_, IsIncognito())
      .WillRepeatedly(Return(true));
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::UNKNOWN);
  reused_password_account_type.set_is_account_syncing(true);
  // No verdict will be cached for incognito profile.
  CacheVerdict(GURL("http://www.test.com/foo/index.html"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, base::Minutes(10),
               "test.com/foo/", base::Time::Now());

  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  CacheVerdict(GURL("http://www.test.com/bar/index2.html"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, base::Minutes(10),
               "test.com/bar/", base::Time::Now());
  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  // Now cache a PASSWORD_REUSE_EVENT verdict. Verdict count should not
  // increase.
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::GSUITE);
  reused_password_account_type.set_is_account_syncing(true);
  CacheVerdict(GURL("http://www.test.com/foobar/index3.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, base::Minutes(10),
               "test.com/foobar/", base::Time::Now());
  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
}

TEST_P(PasswordProtectionServiceBaseTest, TestGetCachedVerdicts) {
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));
  ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::GSUITE);
  reused_password_account_type.set_is_account_syncing(true);
  // Prepare 4 verdicts of the same origin with different cache expressions,
  // or password type, one is expired, one is not, one is of a different
  // trigger type, and the other is with a different password type.
  base::Time now = base::Time::Now();
  CacheVerdict(GURL("http://test.com/login.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, base::Minutes(10),
               "test.com/", now);
  CacheVerdict(GURL("http://test.com/def/index.jsp"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::PHISHING, base::Minutes(10),
               "test.com/def/", now - base::Days(1));  // Yesterday, expired.
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::UNKNOWN);
  CacheVerdict(GURL("http://test.com/bar/login.html"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::PHISHING, base::Minutes(10),
               "test.com/bar/", now);
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  CacheVerdict(GURL("http://test.com/login.html"),
               LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, base::Minutes(10),
               "test.com/", now);

  ASSERT_EQ(3U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  ASSERT_EQ(1U, GetStoredVerdictCount(
                    LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE));

  // Return VERDICT_TYPE_UNSPECIFIED if look up for a URL with unknown origin.
  LoginReputationClientResponse actual_verdict;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::GSUITE);
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            password_protection_service_->GetCachedVerdict(
                GURL("http://www.unknown.com/"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                reused_password_account_type, &actual_verdict));
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  EXPECT_EQ(LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
            password_protection_service_->GetCachedVerdict(
                GURL("http://www.unknown.com/"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                reused_password_account_type, &actual_verdict));

  // Return SAFE if look up for a URL that matches "test.com" cache expression.
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::GSUITE);
  EXPECT_EQ(LoginReputationClientResponse::SAFE,
            password_protection_service_->GetCachedVerdict(
                GURL("http://test.com/xyz/foo.jsp"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                reused_password_account_type, &actual_verdict));
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  EXPECT_EQ(LoginReputationClientResponse::SAFE,
            password_protection_service_->GetCachedVerdict(
                GURL("http://test.com/xyz/foo.jsp"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                reused_password_account_type, &actual_verdict));

  // Return SAFE if look up for a URL whose variants match
  // test.com/def, but the corresponding verdict is expired, so the most
  // matching unexpired verdict will return SAFE
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::GSUITE);
  EXPECT_EQ(LoginReputationClientResponse::SAFE,
            password_protection_service_->GetCachedVerdict(
                GURL("http://test.com/def/ghi/index.html"),
                LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                reused_password_account_type, &actual_verdict));

  // Return PHISHING. Matches "test.com/bar/" cache expression.
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::UNKNOWN);
  EXPECT_EQ(LoginReputationClientResponse::PHISHING,
            password_protection_service_->GetCachedVerdict(
                GURL("http://test.com/bar/foo.jsp"),
                LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                reused_password_account_type, &actual_verdict));

  // Now cache SAFE verdict for the full path.
  CacheVerdict(GURL("http://test.com/bar/foo.jsp"),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::SAFE, base::Minutes(10),
               "test.com/bar/foo.jsp", now);

  // Return SAFE now. Matches the full cache expression.
  EXPECT_EQ(LoginReputationClientResponse::SAFE,
            password_protection_service_->GetCachedVerdict(
                GURL("http://test.com/bar/foo.jsp"),
                LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
                reused_password_account_type, &actual_verdict));
}

TEST_P(PasswordProtectionServiceBaseTest, TestDoesNotCacheAboutBlank) {
  ASSERT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
  ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::UNKNOWN);

  // Should not actually cache, since about:blank is not valid for reputation
  // computing.
  CacheVerdict(
      GURL("about:blank"), LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
      reused_password_account_type, LoginReputationClientResponse::SAFE,
      base::Minutes(10), "about:blank", base::Time::Now());

  EXPECT_EQ(0U, GetStoredVerdictCount(
                    LoginReputationClientRequest::PASSWORD_REUSE_EVENT));
}

TEST_P(PasswordProtectionServiceBaseTest, VerifyCanGetReputationOfURL) {
  // Invalid main frame URL.
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(GURL()));

  // Main frame URL scheme is not HTTP or HTTPS.
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("data:text/html, <p>hellow")));

  // Main frame URL is a local host.
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://localhost:80")));
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://127.0.0.1")));

  // Main frame URL is a private IP address or anything in an IANA-reserved
  // range.
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://192.168.1.0/")));
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://10.0.1.0/")));
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://[FEED::BEEF]")));

  // Main frame URL is a no-yet-assigned y ICANN gTLD.
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://intranet")));
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://host.intranet.example")));

  // Main frame URL is a dotless domain.
  EXPECT_FALSE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://go/example")));

  // Main frame URL is anything else.
  EXPECT_TRUE(PasswordProtectionService::CanGetReputationOfURL(
      GURL("http://www.chromium.org")));
}

TEST_P(PasswordProtectionServiceBaseTest, TestNoRequestSentForAllowlistedURL) {
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestOutcomeHistogram, 0);
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();
  content::WebContentsTester::For(web_contents.get())
      ->SetLastCommittedURL(GURL("http://safe.com/"));
  InitializeAndStartPasswordOnFocusRequest(true /* match allowlist */,
                                           10000 /* timeout in ms */,
                                           web_contents.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, password_protection_service_->latest_response());
  EXPECT_THAT(
      histograms_.GetAllSamples(kPasswordOnFocusRequestOutcomeHistogram),
      ElementsAre(base::Bucket(4 /* MATCHED_ALLOWLIST */, 1)));
}

TEST_P(PasswordProtectionServiceBaseTest,
       TestNoRequestSentIfVerdictAlreadyCached) {
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestOutcomeHistogram, 0);
  ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::UNKNOWN);
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();
  CacheVerdict(GURL(kTargetUrl),
               LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
               reused_password_account_type,
               LoginReputationClientResponse::LOW_REPUTATION, base::Minutes(10),
               GURL(kTargetUrl).host().append("/"), base::Time::Now());
  InitializeAndStartPasswordOnFocusRequest(/*match_allowlist=*/false,
                                           /*timeout_in_ms=*/10000,
                                           web_contents.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      histograms_.GetAllSamples(kPasswordOnFocusRequestOutcomeHistogram),
      ElementsAre(base::Bucket(5 /* RESPONSE_ALREADY_CACHED */, 1)));
  ASSERT_TRUE(password_protection_service_->latest_response());
  EXPECT_EQ(LoginReputationClientResponse::LOW_REPUTATION,
            password_protection_service_->latest_response()->verdict_type());
}

TEST_P(PasswordProtectionServiceBaseTest, TestResponseFetchFailed) {
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestOutcomeHistogram, 0);
  // Set up failed response.
  network::URLLoaderCompletionStatus status(net::ERR_FAILED);
  test_url_loader_factory_.AddResponse(
      url_, network::mojom::URLResponseHead::New(), std::string(), status);
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();

  InitializeAndStartPasswordOnFocusRequest(/*match_allowlist=*/false,
                                           /*timeout_in_ms=*/10000,
                                           web_contents.get());
  password_protection_service_->WaitForResponse();
  EXPECT_EQ(nullptr, password_protection_service_->latest_response());
  EXPECT_THAT(
      histograms_.GetAllSamples(kPasswordOnFocusRequestOutcomeHistogram),
      ElementsAre(base::Bucket(9 /* FETCH_FAILED */, 1)));
}

TEST_P(PasswordProtectionServiceBaseTest, TestMalformedResponse) {
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestOutcomeHistogram, 0);
  // Set up malformed response.
  test_url_loader_factory_.AddResponse(url_.spec(), "invalid response");
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();

  InitializeAndStartPasswordOnFocusRequest(/*match_allowlist=*/false,
                                           /*timeout_in_ms=*/10000,
                                           web_contents.get());
  password_protection_service_->WaitForResponse();
  EXPECT_EQ(nullptr, password_protection_service_->latest_response());
  EXPECT_THAT(
      histograms_.GetAllSamples(kPasswordOnFocusRequestOutcomeHistogram),
      ElementsAre(base::Bucket(10 /* RESPONSE_MALFORMED */, 1)));
}

TEST_P(PasswordProtectionServiceBaseTest, TestRequestTimedout) {
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestOutcomeHistogram, 0);
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();
  InitializeAndStartPasswordOnFocusRequest(/*match_allowlist=*/false,
                                           /*timeout_in_ms=*/0,
                                           web_contents.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, password_protection_service_->latest_response());
  EXPECT_THAT(
      histograms_.GetAllSamples(kPasswordOnFocusRequestOutcomeHistogram),
      ElementsAre(base::Bucket(3 /* TIMEDOUT */, 1)));
}

TEST_P(PasswordProtectionServiceBaseTest,
       TestPasswordOnFocusRequestAndResponseSuccessfull) {
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestOutcomeHistogram, 0);
  // Set up valid response.
  LoginReputationClientResponse expected_response =
      CreateVerdictProto(LoginReputationClientResponse::PHISHING,
                         base::Minutes(10), GURL(kTargetUrl).host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       expected_response.SerializeAsString());
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();

  InitializeAndStartPasswordOnFocusRequest(/*match_allowlist=*/false,
                                           /*timeout_in_ms=*/10000,
                                           web_contents.get());
  password_protection_service_->WaitForResponse();
  EXPECT_THAT(
      histograms_.GetAllSamples(kPasswordOnFocusRequestOutcomeHistogram),
      ElementsAre(base::Bucket(1 /* SUCCEEDED */, 1)));
  EXPECT_THAT(histograms_.GetAllSamples(kPasswordOnFocusVerdictHistogram),
              ElementsAre(base::Bucket(3 /* PHISHING */, 1)));
  LoginReputationClientResponse* actual_response =
      password_protection_service_->latest_response();
  EXPECT_EQ(expected_response.verdict_type(), actual_response->verdict_type());
  EXPECT_EQ(expected_response.cache_expression(),
            actual_response->cache_expression());
  EXPECT_EQ(expected_response.cache_duration_sec(),
            actual_response->cache_duration_sec());
}

TEST_P(PasswordProtectionServiceBaseTest,
       TestPasswordOnFocusRequestEnhancedProtectionShouldHaveToken) {
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestWithTokenHistogram, 0);
  SetEnhancedProtectionPrefForTests(&test_pref_service_, true);
  SetFeatures(
      /*enable_features*/ {kSafeBrowsingRemoveCookiesInAuthRequests},
      /*disable_features*/ {});
  std::string access_token = "fake access token";
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_THAT(
            request.headers.GetHeader(net::HttpRequestHeaders::kAuthorization),
            testing::Optional("Bearer " + access_token));
        // Cookies should be removed when token is set.
        EXPECT_EQ(request.credentials_mode,
                  network::mojom::CredentialsMode::kOmit);
      }));
  // Set up mock call to token fetcher.
  SafeBrowsingTokenFetcher::Callback cb;
  EXPECT_CALL(*raw_token_fetcher_, Start(_)).WillOnce(MoveArg<0>(&cb));

  std::unique_ptr<content::WebContents> web_contents = GetWebContents();
  InitializeAndStartPasswordOnFocusRequest(/*match_allowlist=*/false,
                                           /*timeout_in_ms=*/10000,
                                           web_contents.get());
  // Wait for token fetcher to be called.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClear(raw_token_fetcher_));

  ASSERT_FALSE(cb.is_null());
  std::move(cb).Run(access_token);
  histograms_.ExpectUniqueSample(kPasswordOnFocusRequestWithTokenHistogram,
                                 1 /* Attached token */, 1);
}

TEST_P(PasswordProtectionServiceBaseTest,
       TestPasswordOnFocusRequestNoEnhancedProtectionShouldNotHaveToken) {
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestWithTokenHistogram, 0);
  std::string access_token = "fake access token";
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_EQ(
            request.headers.GetHeader(net::HttpRequestHeaders::kAuthorization),
            std::nullopt);
        // Cookies should be attached when token is empty.
        EXPECT_EQ(request.credentials_mode,
                  network::mojom::CredentialsMode::kInclude);
      }));

  // Never call token fetcher
  EXPECT_CALL(*raw_token_fetcher_, Start(_)).Times(0);

  std::unique_ptr<content::WebContents> web_contents = GetWebContents();
  InitializeAndStartPasswordOnFocusRequest(/*match_allowlist=*/false,
                                           /*timeout_in_ms=*/10000,
                                           web_contents.get());
  base::RunLoop().RunUntilIdle();
  histograms_.ExpectUniqueSample(kPasswordOnFocusRequestWithTokenHistogram,
                                 0 /* No attached token */, 1);
}

TEST_P(PasswordProtectionServiceBaseTest,
       TestProtectedPasswordEntryRequestAndResponseSuccessfull) {
  histograms_.ExpectTotalCount(kAnyPasswordEntryRequestOutcomeHistogram, 0);
  histograms_.ExpectTotalCount(kSyncPasswordEntryRequestOutcomeHistogram, 0);
  histograms_.ExpectTotalCount(kNonSyncPasswordEntryRequestOutcomeHistogram, 0);
  // Set up valid response.
  LoginReputationClientResponse expected_response =
      CreateVerdictProto(LoginReputationClientResponse::PHISHING,
                         base::Minutes(10), GURL(kTargetUrl).host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       expected_response.SerializeAsString());
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();

  // Initiate a saved password entry request (w/ no sync password).
  AccountInfo account_info;
  account_info.account_id = CoreAccountId::FromGaiaId("gaia");
  account_info.email = "email";
  account_info.gaia = "gaia";
  account_info.hosted_domain = "example.com";
  EXPECT_CALL(*password_protection_service_, GetAccountInfoForUsername(_))
      .WillRepeatedly(Return(account_info));

  InitializeAndStartPasswordEntryRequest(
      PasswordType::OTHER_GAIA_PASSWORD, {{"gmail.com", u"username"}},
      /*match_allowlist=*/false,
      /*timeout_in_ms=*/10000, web_contents.get());
  password_protection_service_->WaitForResponse();

  // UMA: request outcomes
  EXPECT_THAT(
      histograms_.GetAllSamples(kAnyPasswordEntryRequestOutcomeHistogram),
      ElementsAre(base::Bucket(1 /* SUCCEEDED */, 1)));
  histograms_.ExpectTotalCount(kSyncPasswordEntryRequestOutcomeHistogram, 0);
  EXPECT_THAT(
      histograms_.GetAllSamples(kNonSyncPasswordEntryRequestOutcomeHistogram),
      ElementsAre(base::Bucket(1 /* SUCCEEDED */, 1)));
  EXPECT_THAT(histograms_.GetAllSamples(kNonSyncPasswordEntryVerdictHistogram),
              ElementsAre(base::Bucket(3 /* PHISHING */, 1)));

  // UMA: verdicts
  EXPECT_THAT(histograms_.GetAllSamples(kAnyPasswordEntryVerdictHistogram),
              ElementsAre(base::Bucket(3 /* PHISHING */, 1)));
  histograms_.ExpectTotalCount(kSyncPasswordEntryVerdictHistogram, 0);
  EXPECT_THAT(histograms_.GetAllSamples(kNonSyncPasswordEntryVerdictHistogram),
              ElementsAre(base::Bucket(3 /* PHISHING */, 1)));
}

TEST_P(PasswordProtectionServiceBaseTest,
       TestSyncPasswordEntryRequestAndResponseSuccessfull) {
  histograms_.ExpectTotalCount(kAnyPasswordEntryRequestOutcomeHistogram, 0);
  histograms_.ExpectTotalCount(kSyncPasswordEntryRequestOutcomeHistogram, 0);
  histograms_.ExpectTotalCount(kNonSyncPasswordEntryRequestOutcomeHistogram, 0);
  // Set up valid response.
  LoginReputationClientResponse expected_response =
      CreateVerdictProto(LoginReputationClientResponse::PHISHING,
                         base::Minutes(10), GURL(kTargetUrl).host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       expected_response.SerializeAsString());
  EXPECT_CALL(*password_protection_service_, IsPrimaryAccountSyncingHistory())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*password_protection_service_, IsPrimaryAccountSignedIn())
      .WillRepeatedly(Return(true));
  // Initiate a sync password entry request (w/ no saved password).
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();
  InitializeAndStartPasswordEntryRequest(
      PasswordType::PRIMARY_ACCOUNT_PASSWORD, {},
      /*match_allowlist=*/false,
      /*timeout_in_ms=*/10000, web_contents.get());
  password_protection_service_->WaitForResponse();

  // UMA: request outcomes
  EXPECT_THAT(
      histograms_.GetAllSamples(kAnyPasswordEntryRequestOutcomeHistogram),
      ElementsAre(base::Bucket(1 /* SUCCEEDED */, 1)));
  EXPECT_THAT(
      histograms_.GetAllSamples(kSyncPasswordEntryRequestOutcomeHistogram),
      ElementsAre(base::Bucket(1 /* SUCCEEDED */, 1)));
  histograms_.ExpectTotalCount(kNonSyncPasswordEntryRequestOutcomeHistogram, 0);

  // UMA: verdicts
  EXPECT_THAT(histograms_.GetAllSamples(kAnyPasswordEntryVerdictHistogram),
              ElementsAre(base::Bucket(3 /* PHISHING */, 1)));
  EXPECT_THAT(histograms_.GetAllSamples(kSyncPasswordEntryVerdictHistogram),
              ElementsAre(base::Bucket(3 /* PHISHING */, 1)));
  histograms_.ExpectTotalCount(kNonSyncPasswordEntryVerdictHistogram, 0);
}

TEST_P(PasswordProtectionServiceBaseTest, TestTearDownWithPendingRequests) {
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestOutcomeHistogram, 0);
  GURL target_url(kTargetUrl);
  EXPECT_CALL(*database_manager_, CheckCsdAllowlistUrl(target_url, _))
      .WillRepeatedly(Return(AsyncMatch::NO_MATCH));
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();
  password_protection_service_->StartRequest(
      web_contents.get(), target_url, GURL("http://foo.com/submit"),
      GURL("http://foo.com/frame"), "username", PasswordType::SAVED_PASSWORD,
      {}, LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE, true);

  // Destroy password_protection_service_ while there is one request pending.
  password_protection_service_.reset();
  base::RunLoop().RunUntilIdle();

  // We should not log on TearDown, since that can dispatch calls to pure
  // virtual methods.
  EXPECT_THAT(
      histograms_.GetAllSamples(kPasswordOnFocusRequestOutcomeHistogram),
      IsEmpty());
}

TEST_P(PasswordProtectionServiceBaseTest, VerifyPasswordOnFocusRequestProto) {
  // Set up valid response.
  LoginReputationClientResponse expected_response =
      CreateVerdictProto(LoginReputationClientResponse::PHISHING,
                         base::Minutes(10), GURL(kTargetUrl).host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       expected_response.SerializeAsString());
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();

  InitializeAndStartPasswordOnFocusRequest(/*match_allowlist=*/false,
                                           /*timeout_in_ms=*/10000,
                                           web_contents.get());
  password_protection_service_->WaitForResponse();

  const LoginReputationClientRequest* actual_request =
      password_protection_service_->GetLatestRequestProto();
  EXPECT_EQ(kTargetUrl, actual_request->page_url());
  EXPECT_EQ(LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
            actual_request->trigger_type());
  ASSERT_EQ(2, actual_request->frames_size());
  EXPECT_EQ(kTargetUrl, actual_request->frames(0).url());
  EXPECT_EQ(kPasswordFrameUrl, actual_request->frames(1).url());
  EXPECT_EQ(true, actual_request->frames(1).has_password_field());
  ASSERT_EQ(1, actual_request->frames(1).forms_size());
  EXPECT_EQ(kFormActionUrl, actual_request->frames(1).forms(0).action_url());
#if !BUILDFLAG(IS_ANDROID)
  VerifyContentAreaSizeCollection(*actual_request);
#endif
}

TEST_P(PasswordProtectionServiceBaseTest,
       VerifyPasswordOnFocusRequestProtoForAllowlistMatch) {
  // Set up valid response.
  LoginReputationClientResponse expected_response =
      CreateVerdictProto(LoginReputationClientResponse::PHISHING,
                         base::Minutes(10), GURL(kTargetUrl).host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       expected_response.SerializeAsString());
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();

  EXPECT_CALL(*password_protection_service_, CanSendSamplePing())
      .WillRepeatedly(Return(true));
  InitializeAndStartPasswordOnFocusRequest(/*match_allowlist=*/true,
                                           /*timeout_in_ms=*/10000,
                                           web_contents.get());
  password_protection_service_->WaitForResponse();

  const LoginReputationClientRequest* actual_request =
      password_protection_service_->GetLatestRequestProto();
  EXPECT_EQ(kTargetUrl, actual_request->page_url());
  ASSERT_EQ(1, actual_request->frames_size());
  EXPECT_EQ(kTargetUrl, actual_request->frames(0).url());
}

TEST_P(PasswordProtectionServiceBaseTest,
       VerifySyncPasswordProtectionRequestProto) {
  // Set up valid response.
  LoginReputationClientResponse expected_response =
      CreateVerdictProto(LoginReputationClientResponse::PHISHING,
                         base::Minutes(10), GURL(kTargetUrl).host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       expected_response.SerializeAsString());
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();

  // Initialize request triggered by chrome sync password reuse.
  InitializeAndStartPasswordEntryRequest(
      PasswordType::PRIMARY_ACCOUNT_PASSWORD, {}, false /* match allowlist */,
      100000 /* timeout in ms*/, web_contents.get());
  password_protection_service_->WaitForResponse();

  const LoginReputationClientRequest* actual_request =
      password_protection_service_->GetLatestRequestProto();
  EXPECT_EQ(kTargetUrl, actual_request->page_url());
  EXPECT_EQ(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
            actual_request->trigger_type());
  EXPECT_EQ(1, actual_request->frames_size());
  EXPECT_EQ(kTargetUrl, actual_request->frames(0).url());
  EXPECT_TRUE(actual_request->frames(0).has_password_field());
  ASSERT_TRUE(actual_request->has_password_reuse_event());
  const auto& reuse_event = actual_request->password_reuse_event();
  EXPECT_EQ(0, reuse_event.domains_matching_password_size());
#if !BUILDFLAG(IS_ANDROID)
  VerifyContentAreaSizeCollection(*actual_request);
#endif
}

TEST_P(PasswordProtectionServiceBaseTest,
       VerifySavePasswordProtectionRequestProto) {
  // Set up valid response.
  LoginReputationClientResponse expected_response =
      CreateVerdictProto(LoginReputationClientResponse::PHISHING,
                         base::Minutes(10), GURL(kTargetUrl).host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       expected_response.SerializeAsString());
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();

  // Initialize request triggered by saved password reuse.
  InitializeAndStartPasswordEntryRequest(
      PasswordType::SAVED_PASSWORD,
      {{kSavedDomain, u"username"},
       {kSavedDomain2, u"username"},
       {"http://localhost:8080", u"username"}},
      false /* match allowlist */, 100000 /* timeout in ms*/,
      web_contents.get());
  password_protection_service_->WaitForResponse();

  const LoginReputationClientRequest* actual_request =
      password_protection_service_->GetLatestRequestProto();
  ASSERT_TRUE(actual_request->has_password_reuse_event());
  const auto& reuse_event = actual_request->password_reuse_event();

  if (password_protection_service_->IsExtendedReporting() &&
      !password_protection_service_->IsIncognito()) {
    ASSERT_EQ(3, reuse_event.domains_matching_password_size());
    EXPECT_EQ("localhost:8080", reuse_event.domains_matching_password(0));
    EXPECT_EQ("saved_domain.com", reuse_event.domains_matching_password(1));
    EXPECT_EQ("saved_domain2.com", reuse_event.domains_matching_password(2));
  } else {
    EXPECT_EQ(0, reuse_event.domains_matching_password_size());
  }
#if !BUILDFLAG(IS_ANDROID)
  VerifyContentAreaSizeCollection(*actual_request);
#endif
}

TEST_P(PasswordProtectionServiceBaseTest, VerifyShouldShowModalWarning) {
  EXPECT_CALL(*password_protection_service_,
              GetPasswordProtectionWarningTriggerPref(_))
      .WillRepeatedly(Return(PHISHING_REUSE));
  EXPECT_CALL(*password_protection_service_, IsPrimaryAccountSignedIn())
      .WillRepeatedly(Return(true));
  AccountInfo account_info;
  account_info.account_id = CoreAccountId::FromGaiaId("gaia");
  account_info.email = "email";
  account_info.gaia = "gaia";
  EXPECT_CALL(*password_protection_service_, GetAccountInfoForUsername(_))
      .WillRepeatedly(Return(account_info));

  // Don't show modal warning if it is not a password reuse ping.
  ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::UNKNOWN);
  reused_password_account_type.set_is_account_syncing(true);
  EXPECT_FALSE(password_protection_service_->ShouldShowModalWarning(
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE,
      reused_password_account_type, LoginReputationClientResponse::PHISHING));

  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::SAVED_PASSWORD);

  EXPECT_TRUE(password_protection_service_->ShouldShowModalWarning(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_account_type,
      LoginReputationClientResponse::LOW_REPUTATION));

  {
    reused_password_account_type.set_account_type(
        ReusedPasswordAccountType::GMAIL);
    reused_password_account_type.set_is_account_syncing(false);
    EXPECT_TRUE(password_protection_service_->ShouldShowModalWarning(
        LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
        reused_password_account_type, LoginReputationClientResponse::PHISHING));
  }

  // Don't show modal warning if reused password type unknown.
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::UNKNOWN);
  EXPECT_FALSE(password_protection_service_->ShouldShowModalWarning(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_account_type, LoginReputationClientResponse::PHISHING));

  {
    reused_password_account_type.set_account_type(
        ReusedPasswordAccountType::GMAIL);
    reused_password_account_type.set_is_account_syncing(true);
    EXPECT_TRUE(password_protection_service_->ShouldShowModalWarning(
        LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
        reused_password_account_type, LoginReputationClientResponse::PHISHING));
  }

  // For a GSUITE account, don't show warning if password protection is set to
  // off by enterprise policy.
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::GSUITE);
  EXPECT_CALL(*password_protection_service_,
              GetPasswordProtectionWarningTriggerPref(_))
      .WillRepeatedly(Return(PASSWORD_PROTECTION_OFF));
  EXPECT_FALSE(password_protection_service_->ShouldShowModalWarning(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_account_type, LoginReputationClientResponse::PHISHING));

  // For a GSUITE account, show warning if password protection is set to
  // PHISHING_REUSE.
  EXPECT_CALL(*password_protection_service_,
              GetPasswordProtectionWarningTriggerPref(_))
      .WillRepeatedly(Return(PHISHING_REUSE));
  EXPECT_EQ(
      PHISHING_REUSE,
      password_protection_service_->GetPasswordProtectionWarningTriggerPref(
          reused_password_account_type));
// Currently password reuse warnings are not supported for GSUITE passwords on
// Android.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(password_protection_service_->ShouldShowModalWarning(
#else
  EXPECT_TRUE(password_protection_service_->ShouldShowModalWarning(
#endif
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_account_type, LoginReputationClientResponse::PHISHING));

  // Modal dialog warning is also shown on LOW_REPUTATION verdict.
// Currently password reuse warnings are not supported for GSUITE passwords on
// Android.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(password_protection_service_->ShouldShowModalWarning(
#else
  EXPECT_TRUE(password_protection_service_->ShouldShowModalWarning(
#endif
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_account_type,
      LoginReputationClientResponse::LOW_REPUTATION));

  // Modal dialog warning should not be shown for enterprise password reuse
  // if it is turned off by policy.
  EXPECT_CALL(*password_protection_service_,
              GetPasswordProtectionWarningTriggerPref(_))
      .WillRepeatedly(Return(PASSWORD_PROTECTION_OFF));
  EXPECT_FALSE(password_protection_service_->ShouldShowModalWarning(
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_account_type, LoginReputationClientResponse::PHISHING));

  // Show modal warning for enterprise password reuse if the trigger is
  // configured to PHISHING_REUSE.
  reused_password_account_type.set_account_type(
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  EXPECT_CALL(*password_protection_service_,
              GetPasswordProtectionWarningTriggerPref(_))
      .WillRepeatedly(Return(PHISHING_REUSE));
// Currently password reuse warnings are not supported for enterprise passwords
// on Android.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(password_protection_service_->ShouldShowModalWarning(
#else
  EXPECT_TRUE(password_protection_service_->ShouldShowModalWarning(
#endif
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
      reused_password_account_type, LoginReputationClientResponse::PHISHING));
}

TEST_P(PasswordProtectionServiceBaseTest, VerifyContentTypeIsPopulated) {
  LoginReputationClientResponse response =
      CreateVerdictProto(LoginReputationClientResponse::SAFE, base::Minutes(10),
                         GURL(kTargetUrl).host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       response.SerializeAsString());

  std::unique_ptr<content::WebContents> web_contents = GetWebContents();

  content::WebContentsTester::For(web_contents.get())
      ->SetMainFrameMimeType("application/pdf");

  InitializeAndStartPasswordOnFocusRequest(false /* match allowlist */,
                                           10000 /* timeout in ms */,
                                           web_contents.get());

  password_protection_service_->WaitForResponse();

  EXPECT_EQ(
      "application/pdf",
      password_protection_service_->GetLatestRequestProto()->content_type());
}

TEST_P(PasswordProtectionServiceBaseTest,
       VerifyIsSupportedPasswordTypeForPinging) {
  EXPECT_CALL(*password_protection_service_, IsPrimaryAccountSignedIn())
      .WillRepeatedly(Return(true));
  AccountInfo account_info;
  account_info.account_id = CoreAccountId::FromGaiaId("gaia");
  account_info.email = "email";
  account_info.gaia = "gaia";
  EXPECT_CALL(*password_protection_service_, GetAccountInfoForUsername(_))
      .WillRepeatedly(Return(account_info));

  EXPECT_TRUE(password_protection_service_->IsSupportedPasswordTypeForPinging(
      PasswordType::PRIMARY_ACCOUNT_PASSWORD));
  EXPECT_TRUE(password_protection_service_->IsSupportedPasswordTypeForPinging(
      PasswordType::OTHER_GAIA_PASSWORD));
  EXPECT_TRUE(password_protection_service_->IsSupportedPasswordTypeForPinging(
      PasswordType::ENTERPRISE_PASSWORD));
  EXPECT_TRUE(password_protection_service_->IsSupportedPasswordTypeForPinging(
      PasswordType::SAVED_PASSWORD));
}

TEST_P(PasswordProtectionServiceBaseTest, TestPingsForAboutBlank) {
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestOutcomeHistogram, 0);
  LoginReputationClientResponse expected_response =
      CreateVerdictProto(LoginReputationClientResponse::PHISHING,
                         base::Minutes(10), GURL("about:blank").host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       expected_response.SerializeAsString());
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();
  password_protection_service_->InitTestApi(
      web_contents->GetPrimaryMainFrame());
  password_protection_service_->StartRequest(
      web_contents.get(), GURL("about:blank"), GURL(), GURL(), "username",
      PasswordType::SAVED_PASSWORD, {{"example1.com", u"username"}},
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE, true);
  base::RunLoop().RunUntilIdle();
  histograms_.ExpectTotalCount(kPasswordOnFocusRequestOutcomeHistogram, 1);
}

// DOM features and visual features are not supported on Android.
#if !BUILDFLAG(IS_ANDROID)
TEST_P(PasswordProtectionServiceBaseTest,
       TestVisualFeaturesPopulatedInOnFocusPing) {
  LoginReputationClientResponse expected_response =
      CreateVerdictProto(LoginReputationClientResponse::PHISHING,
                         base::Minutes(10), GURL("about:blank").host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       expected_response.SerializeAsString());
  EXPECT_CALL(*password_protection_service_, GetCurrentContentAreaSize())
      .Times(AnyNumber())
      .WillOnce(Return(gfx::Size(1000, 1000)));
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();
  password_protection_service_->StartRequest(
      web_contents.get(), GURL("about:blank"), GURL(), GURL(), kUserName,
      PasswordType::SAVED_PASSWORD, {{"example.com", u"username"}},
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE, true);
  base::RunLoop().RunUntilIdle();

  bool is_sber = GetParam();
  if (is_sber) {
    password_protection_service_->WaitForResponse();
    ASSERT_NE(nullptr, password_protection_service_->GetLatestRequestProto());
    EXPECT_TRUE(password_protection_service_->GetLatestRequestProto()
                    ->has_visual_features());
  }
}

TEST_P(PasswordProtectionServiceBaseTest, TestDomFeaturesPopulated) {
  LoginReputationClientResponse expected_response =
      CreateVerdictProto(LoginReputationClientResponse::PHISHING,
                         base::Minutes(10), GURL("about:blank").host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       expected_response.SerializeAsString());
  EXPECT_CALL(*password_protection_service_, GetCurrentContentAreaSize())
      .Times(AnyNumber())
      .WillOnce(Return(gfx::Size(1000, 1000)));
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();
  password_protection_service_->InitTestApi(
      web_contents->GetPrimaryMainFrame());
  password_protection_service_->StartRequest(
      web_contents.get(), GURL("about:blank"), GURL(), GURL(), kUserName,
      PasswordType::SAVED_PASSWORD, {{"example.com", u"username"}},
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE, true);
  base::RunLoop().RunUntilIdle();

  password_protection_service_->WaitForResponse();
  ASSERT_NE(nullptr, password_protection_service_->GetLatestRequestProto());
  EXPECT_TRUE(password_protection_service_->GetLatestRequestProto()
                  ->has_dom_features());
}

TEST_P(PasswordProtectionServiceBaseTest, TestDomFeaturesTimeout) {
  password_protection_service_->SetDomFeatureCollectionTimeout(true);
  LoginReputationClientResponse expected_response =
      CreateVerdictProto(LoginReputationClientResponse::PHISHING,
                         base::Minutes(10), GURL("about:blank").host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       expected_response.SerializeAsString());
  EXPECT_CALL(*password_protection_service_, GetCurrentContentAreaSize())
      .Times(AnyNumber())
      .WillOnce(Return(gfx::Size(1000, 1000)));
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();
  password_protection_service_->StartRequest(
      web_contents.get(), GURL("about:blank"), GURL(), GURL(), kUserName,
      PasswordType::SAVED_PASSWORD, {{"example.com", u"username"}},
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE, true);
  task_environment_.RunUntilIdle();

  password_protection_service_->WaitForResponse();
  ASSERT_NE(nullptr, password_protection_service_->GetLatestRequestProto());
  EXPECT_FALSE(password_protection_service_->GetLatestRequestProto()
                   ->has_dom_features());
}
#endif

TEST_P(PasswordProtectionServiceBaseTest, TestWebContentsDestroyed) {
  std::unique_ptr<content::WebContents> web_contents = GetWebContents();
  InitializeAndStartPasswordOnFocusRequest(false /* match allowlist */,
                                           10000 /* timeout in ms */,
                                           web_contents.get());
  web_contents.reset();
  task_environment_.RunUntilIdle();
}

// TODO(crbug.com/40918301): [Also TODO(thefrog)] Remove test case once
// kHashPrefixRealTimeLookups is launched.
TEST_P(PasswordProtectionServiceBaseTest,
       TestHashPrefixRealTimeLookupsFeatureEnabled) {
  const LoginReputationClientRequest* proto = SetUpFinchActiveGroupsTest(
      {"SafeBrowsingHashPrefixRealTimeLookups"}, "Enabled");
  ASSERT_NE(nullptr, proto);
  EXPECT_TRUE(base::Contains(proto->population().finch_active_groups(),
                             "SafeBrowsingHashPrefixRealTimeLookups.Enabled"));
  EXPECT_FALSE(base::Contains(proto->population().finch_active_groups(),
                              "SafeBrowsingHashPrefixRealTimeLookups.Control"));
  EXPECT_FALSE(base::Contains(proto->population().finch_active_groups(),
                              "SafeBrowsingHashPrefixRealTimeLookups.Default"));
}

// TODO(crbug.com/40918301): [Also TODO(thefrog)] Remove test case once
// kHashPrefixRealTimeLookups is launched.
TEST_P(PasswordProtectionServiceBaseTest,
       TestHashPrefixRealTimeLookupsFeatureControl) {
  const LoginReputationClientRequest* proto = SetUpFinchActiveGroupsTest(
      {"SafeBrowsingHashPrefixRealTimeLookups"}, "Control");
  ASSERT_NE(nullptr, proto);
  EXPECT_FALSE(base::Contains(proto->population().finch_active_groups(),
                              "SafeBrowsingHashPrefixRealTimeLookups.Enabled"));
  EXPECT_TRUE(base::Contains(proto->population().finch_active_groups(),
                             "SafeBrowsingHashPrefixRealTimeLookups.Control"));
  EXPECT_FALSE(base::Contains(proto->population().finch_active_groups(),
                              "SafeBrowsingHashPrefixRealTimeLookups.Default"));
}

// TODO(crbug.com/40918301): [Also TODO(thefrog)] Remove test case once
// kHashPrefixRealTimeLookups is launched.
TEST_P(PasswordProtectionServiceBaseTest,
       TestHashPrefixRealTimeLookupsFeatureDefault) {
  const LoginReputationClientRequest* proto = SetUpFinchActiveGroupsTest(
      {"SafeBrowsingHashPrefixRealTimeLookups"}, "Default");
  ASSERT_NE(nullptr, proto);
  EXPECT_FALSE(base::Contains(proto->population().finch_active_groups(),
                              "SafeBrowsingHashPrefixRealTimeLookups.Enabled"));
  EXPECT_FALSE(base::Contains(proto->population().finch_active_groups(),
                              "SafeBrowsingHashPrefixRealTimeLookups.Control"));
  EXPECT_FALSE(base::Contains(proto->population().finch_active_groups(),
                              "SafeBrowsingHashPrefixRealTimeLookups.Default"));
}

TEST_P(PasswordProtectionServiceBaseTest,
       TestAsyncRealTimeCheckFeatureEnabled) {
  const LoginReputationClientRequest* proto =
      SetUpFinchActiveGroupsTest({"SafeBrowsingAsyncRealTimeCheck"}, "Enabled");
  bool is_sber = GetParam();
  ASSERT_NE(nullptr, proto);
  EXPECT_EQ(base::Contains(proto->population().finch_active_groups(),
                           "SafeBrowsingAsyncRealTimeCheck.Enabled"),
            is_sber);
  EXPECT_FALSE(base::Contains(proto->population().finch_active_groups(),
                              "SafeBrowsingAsyncRealTimeCheck.Control"));
  EXPECT_FALSE(base::Contains(proto->population().finch_active_groups(),
                              "SafeBrowsingAsyncRealTimeCheck.Default"));
}

TEST_P(PasswordProtectionServiceBaseTest,
       TestAsyncRealTimeCheckFeatureEnabled_Incognito) {
  EXPECT_CALL(*password_protection_service_, IsIncognito())
      .WillRepeatedly(Return(true));
  const LoginReputationClientRequest* proto =
      SetUpFinchActiveGroupsTest({"SafeBrowsingAsyncRealTimeCheck"}, "Enabled");
  ASSERT_NE(nullptr, proto);
  EXPECT_FALSE(base::Contains(proto->population().finch_active_groups(),
                              "SafeBrowsingAsyncRealTimeCheck.Enabled"));
  EXPECT_FALSE(base::Contains(proto->population().finch_active_groups(),
                              "SafeBrowsingAsyncRealTimeCheck.Control"));
  EXPECT_FALSE(base::Contains(proto->population().finch_active_groups(),
                              "SafeBrowsingAsyncRealTimeCheck.Default"));
}

TEST_P(PasswordProtectionServiceBaseTest,
       TestAsyncRealTimeCheckFeatureControl) {
  const LoginReputationClientRequest* proto =
      SetUpFinchActiveGroupsTest({"SafeBrowsingAsyncRealTimeCheck"}, "Control");
  bool is_sber = GetParam();
  ASSERT_NE(nullptr, proto);
  EXPECT_FALSE(base::Contains(proto->population().finch_active_groups(),
                              "SafeBrowsingAsyncRealTimeCheck.Enabled"));
  EXPECT_EQ(base::Contains(proto->population().finch_active_groups(),
                           "SafeBrowsingAsyncRealTimeCheck.Control"),
            is_sber);
  EXPECT_FALSE(base::Contains(proto->population().finch_active_groups(),
                              "SafeBrowsingAsyncRealTimeCheck.Default"));
}

TEST_P(PasswordProtectionServiceBaseTest,
       TestAsyncRealTimeCheckFeatureDefault) {
  const LoginReputationClientRequest* proto =
      SetUpFinchActiveGroupsTest({"SafeBrowsingAsyncRealTimeCheck"}, "Default");
  ASSERT_NE(nullptr, proto);
  EXPECT_FALSE(base::Contains(proto->population().finch_active_groups(),
                              "SafeBrowsingAsyncRealTimeCheck.Enabled"));
  EXPECT_FALSE(base::Contains(proto->population().finch_active_groups(),
                              "SafeBrowsingAsyncRealTimeCheck.Control"));
  EXPECT_FALSE(base::Contains(proto->population().finch_active_groups(),
                              "SafeBrowsingAsyncRealTimeCheck.Default"));
}

// TODO(crbug.com/40918301): [Also TODO(thefrog)] Remove test case once
// kHashPrefixRealTimeLookups is launched.
TEST_P(PasswordProtectionServiceBaseTest,
       TestAsyncRealTimeCheckAndHashPrefixRealTimeLookupsFeaturesEnabled) {
  const LoginReputationClientRequest* proto =
      SetUpFinchActiveGroupsTest({"SafeBrowsingAsyncRealTimeCheck",
                                  "SafeBrowsingHashPrefixRealTimeLookups"},
                                 "Enabled");
  ASSERT_NE(nullptr, proto);
  EXPECT_TRUE(base::Contains(proto->population().finch_active_groups(),
                             "SafeBrowsingHashPrefixRealTimeLookups.Enabled"));
  EXPECT_FALSE(base::Contains(proto->population().finch_active_groups(),
                              "SafeBrowsingHashPrefixRealTimeLookups.Control"));
  EXPECT_FALSE(base::Contains(proto->population().finch_active_groups(),
                              "SafeBrowsingHashPrefixRealTimeLookups.Default"));
  bool is_sber = GetParam();
  EXPECT_EQ(base::Contains(proto->population().finch_active_groups(),
                           "SafeBrowsingAsyncRealTimeCheck.Enabled"),
            is_sber);
  EXPECT_FALSE(base::Contains(proto->population().finch_active_groups(),
                              "SafeBrowsingAsyncRealTimeCheck.Control"));
  EXPECT_FALSE(base::Contains(proto->population().finch_active_groups(),
                              "SafeBrowsingAsyncRealTimeCheck.Default"));
}

TEST_P(PasswordProtectionServiceBaseTest, TestCSDVerdictInCache) {
  LoginReputationClientResponse expected_response =
      CreateVerdictProto(LoginReputationClientResponse::PHISHING,
                         base::Minutes(10), GURL(kTargetUrl).host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       expected_response.SerializeAsString());

  std::unique_ptr<content::WebContents> web_contents = GetWebContents();
  std::unique_ptr<ClientPhishingRequest> verdict =
      std::make_unique<ClientPhishingRequest>();

  VisualFeatures* visual_feature = verdict->mutable_visual_features();
  visual_feature->mutable_image()->set_height(1);
  visual_feature->mutable_image()->set_width(2);

  ClientSideDetectionFeatureCache::CreateForWebContents(web_contents.get());
  ClientSideDetectionFeatureCache::FromWebContents(web_contents.get())
      ->InsertVerdict(GURL(kTargetUrl), std::move(verdict));

  histograms_.ExpectTotalCount("PasswordProtection.CSDCacheContainsImages", 0);

  InitializeAndStartPasswordEntryRequest(
      PasswordType::PRIMARY_ACCOUNT_PASSWORD, {}, false /* match allowlist */,
      100000 /* timeout in ms*/, web_contents.get());
  password_protection_service_->WaitForResponse();

  histograms_.ExpectTotalCount("PasswordProtection.CSDCacheContainsImages", 1);
  EXPECT_THAT(
      histograms_.GetAllSamples("PasswordProtection.CSDCacheContainsImages"),
      ElementsAre(base::Bucket(1, 1)));

  ASSERT_NE(nullptr, password_protection_service_->GetLatestRequestProto());
  EXPECT_TRUE(password_protection_service_->GetLatestRequestProto()
                  ->has_visual_features());
}

TEST_P(PasswordProtectionServiceBaseTest, TestCSDDebuggingMetadataInCache) {
  if (!password_protection_service_->IsExtendedReporting()) {
    return;
  }

  std::vector<base::test::FeatureRef> enabled_features = {};
  enabled_features.push_back(kClientSideDetectionDebuggingMetadataCache);
  SetFeatures(enabled_features, {});

  LoginReputationClientResponse expected_response =
      CreateVerdictProto(LoginReputationClientResponse::PHISHING,
                         base::Minutes(10), GURL(kTargetUrl).host());
  test_url_loader_factory_.AddResponse(url_.spec(),
                                       expected_response.SerializeAsString());

  std::unique_ptr<content::WebContents> web_contents = GetWebContents();

  ClientSideDetectionFeatureCache::CreateForWebContents(web_contents.get());
  ClientSideDetectionFeatureCache* feature_map =
      ClientSideDetectionFeatureCache::FromWebContents(web_contents.get());
  LoginReputationClientRequest::DebuggingMetadata* debugging_metadata =
      feature_map->GetOrCreateDebuggingMetadataForURL(GURL(kTargetUrl));
  debugging_metadata->set_preclassification_check_result(
      PreClassificationCheckResult::CLASSIFY);
  debugging_metadata->set_csd_model_version(34);
  debugging_metadata->set_local_model_detects_phishing(true);
  debugging_metadata->set_network_result(200);

  InitializeAndStartPasswordEntryRequest(
      PasswordType::PRIMARY_ACCOUNT_PASSWORD, {}, false /* match allowlist */,
      100000 /* timeout in ms*/, web_contents.get());
  password_protection_service_->WaitForResponse();

  const LoginReputationClientRequest* actual_request =
      password_protection_service_->GetLatestRequestProto();
  EXPECT_EQ(kTargetUrl, actual_request->page_url());
  EXPECT_EQ(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
            actual_request->trigger_type());

  EXPECT_EQ(
      actual_request->csd_debugging_metadata().preclassification_check_result(),
      PreClassificationCheckResult::CLASSIFY);
  EXPECT_EQ(actual_request->csd_debugging_metadata().csd_model_version(), 34);
  EXPECT_EQ(
      actual_request->csd_debugging_metadata().local_model_detects_phishing(),
      true);
  EXPECT_EQ(actual_request->csd_debugging_metadata().network_result(), 200);

  // There should not exist one after the request has been made.
  EXPECT_NE(feature_map->GetOrCreateDebuggingMetadataForURL(GURL(kTargetUrl))
                ->csd_model_version(),
            34);
  EXPECT_NE(feature_map->GetOrCreateDebuggingMetadataForURL(GURL(kTargetUrl))
                ->local_model_detects_phishing(),
            true);
  EXPECT_NE(feature_map->GetOrCreateDebuggingMetadataForURL(GURL(kTargetUrl))
                ->network_result(),
            200);
}

INSTANTIATE_TEST_SUITE_P(Regular,
                         PasswordProtectionServiceBaseTest,
                         ::testing::Values(false));
INSTANTIATE_TEST_SUITE_P(SBER,
                         PasswordProtectionServiceBaseTest,
                         ::testing::Values(true));
}  // namespace safe_browsing
