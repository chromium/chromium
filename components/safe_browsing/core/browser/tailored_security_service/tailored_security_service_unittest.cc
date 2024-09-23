// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service_observer_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::IsEmpty;
using ::testing::Not;

namespace safe_browsing {

namespace {

const char kQueryTailoredSecurityServiceUrl[] =
    "https://history.google.com/history/api/lookup?client=aesb";

// A testing tailored security service that does extra checks and creates a
// TestRequest instead of a normal request.
class TestingTailoredSecurityService : public TailoredSecurityService {
 public:
  TestingTailoredSecurityService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* prefs,
      syncer::SyncService* sync_service)
      // NOTE: Simply pass null object for IdentityManager and SyncService.
      // TailoredSecurityService's only usage of this object is to fetch access
      // tokens via RequestImpl, and TestingTailoredSecurityService deliberately
      // replaces this flow with TestRequest.
      : TailoredSecurityService(/*identity_manager=*/nullptr,
                                /*sync_service=*/sync_service,
                                prefs),
        url_loader_factory_(url_loader_factory) {}
  ~TestingTailoredSecurityService() override = default;

  std::unique_ptr<TailoredSecurityService::Request> CreateRequest(
      const GURL& url,
      CompletionCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;

  // This is sorta an override but override and static don't mix.
  // This function just calls TailoredSecurityService::ReadResponse.
  static base::Value::Dict ReadResponse(Request* request);

  const std::string& GetExpectedPostData(
      TailoredSecurityService::Request* request);

  std::string GetExpectedTailoredSecurityServiceValue();

  void SetTailoredSecurityServiceCallback(bool is_enabled,
                                          base::Time previous_update);

  void GetTailoredSecurityServiceCallback(bool is_enabled,
                                          base::Time previous_update);

  void MultipleRequestsCallback(bool is_enabled, base::Time previous_update);

  void SetExpectedURL(const GURL& expected_url) {
    expected_url_ = expected_url;
  }

  void SetExpectedTailoredSecurityServiceValue(bool expected_value) {
    expected_tailored_security_service_value_ = expected_value;
  }

  void SetExpectedPostData(const std::string& expected_data) {
    current_expected_post_data_ = expected_data;
  }

  void EnsureNoPendingRequestsRemain() {
    EXPECT_EQ(0u, GetNumberOfPendingTailoredSecurityServiceRequests());
  }

  void Shutdown() override;

  void SetNotifySyncUserCallback(base::OnceClosure callback) {
    notify_sync_user_callback_ = std::move(callback);
  }
  bool notify_sync_user_called() const { return notify_sync_user_called_; }
  bool notify_sync_user_called_enabled() const {
    return notify_sync_user_called_enabled_;
  }

 protected:
  void MaybeNotifySyncUser(bool is_enabled,
                           base::Time previous_update) override;

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return url_loader_factory_;
  }

 private:
  GURL expected_url_;
  bool expected_tailored_security_service_value_;
  std::string current_expected_post_data_;
  std::map<Request*, std::string> expected_post_data_;

  // Whether `MaybeNotifySyncUser` was called.
  bool notify_sync_user_called_;

  // The value of `is_enabled` when `MaybeNotifySyncUser` was called.
  bool notify_sync_user_called_enabled_;

  // Called whenever `MaybeNotifySyncUser` is called.
  base::OnceClosure notify_sync_user_callback_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

// A testing request class that allows expected values to be filled in.
class TestRequest : public TailoredSecurityService::Request {
 public:
  TestRequest(const GURL& url,
              TailoredSecurityService::CompletionCallback callback,
              int response_code,
              const std::string& response_body)
      : tailored_security_service_(nullptr),
        url_(url),
        callback_(std::move(callback)),
        response_code_(response_code),
        response_body_(response_body),
        is_pending_(false) {}

  TestRequest(const GURL& url,
              TailoredSecurityService::CompletionCallback callback,
              TestingTailoredSecurityService* tailored_security_service)
      : tailored_security_service_(tailored_security_service),
        url_(url),
        callback_(std::move(callback)),
        response_code_(net::HTTP_OK),
        is_pending_(false) {
    response_body_ =
        std::string("{\"history_recording_enabled\":") +
        tailored_security_service->GetExpectedTailoredSecurityServiceValue() +
        ("}");
  }

  ~TestRequest() override = default;

  // safe_browsing::Request overrides
  bool IsPending() const override { return is_pending_; }
  int GetResponseCode() const override { return response_code_; }
  const std::string& GetResponseBody() const override { return response_body_; }
  void SetPostData(const std::string& post_data) override {
    post_data_ = post_data;
  }

  void Shutdown() override { is_shut_down_ = true; }

  void Start() override {
    is_pending_ = true;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&TestRequest::MimicReturnFromFetch,
                                  base::Unretained(this)));
  }

  void MimicReturnFromFetch() {
    // Mimic a successful fetch and return. We don't actually send out a request
    // in unittests.
    if (is_shut_down_)
      return;
    EXPECT_EQ(tailored_security_service_->GetExpectedPostData(this),
              post_data_);
    std::move(callback_).Run(this, true);
  }

 private:
  raw_ptr<TestingTailoredSecurityService> tailored_security_service_;
  GURL url_;
  TailoredSecurityService::CompletionCallback callback_;
  int response_code_;
  std::string response_body_;
  std::string post_data_;
  bool is_pending_;
  bool is_shut_down_ = false;
};

std::unique_ptr<TailoredSecurityService::Request>
TestingTailoredSecurityService::CreateRequest(
    const GURL& url,
    CompletionCallback callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  EXPECT_EQ(expected_url_, url);
  std::unique_ptr<TailoredSecurityService::Request> request =
      std::make_unique<TestRequest>(url, std::move(callback), this);
  expected_post_data_[request.get()] = current_expected_post_data_;
  return request;
}

base::Value::Dict TestingTailoredSecurityService::ReadResponse(
    Request* request) {
  return TailoredSecurityService::ReadResponse(request);
}

void TestingTailoredSecurityService::SetTailoredSecurityServiceCallback(
    bool is_enabled,
    base::Time previous_update) {
  EXPECT_EQ(expected_tailored_security_service_value_, is_enabled);
}

void TestingTailoredSecurityService::GetTailoredSecurityServiceCallback(
    bool is_enabled,
    base::Time previous_update) {
  EXPECT_EQ(expected_tailored_security_service_value_, is_enabled);
}

void TestingTailoredSecurityService::MultipleRequestsCallback(
    bool is_enabled,
    base::Time previous_update) {
  EXPECT_EQ(expected_tailored_security_service_value_, is_enabled);
}

const std::string& TestingTailoredSecurityService::GetExpectedPostData(
    Request* request) {
  return expected_post_data_[request];
}

std::string
TestingTailoredSecurityService::GetExpectedTailoredSecurityServiceValue() {
  if (expected_tailored_security_service_value_)
    return "true";
  return "false";
}

void TestingTailoredSecurityService::Shutdown() {
  for (auto request : expected_post_data_) {
    request.first->Shutdown();
  }
  expected_post_data_.clear();
}

void TestingTailoredSecurityService::MaybeNotifySyncUser(
    bool is_enabled,
    base::Time previous_update) {
  notify_sync_user_called_ = true;
  notify_sync_user_called_enabled_ = is_enabled;
  if (notify_sync_user_callback_)
    std::move(notify_sync_user_callback_).Run();
}

}  // namespace

// A test class used for testing the TailoredSecurityService class.
class TailoredSecurityServiceTest : public testing::Test {
 public:
  TailoredSecurityServiceTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  ~TailoredSecurityServiceTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(kTailoredSecurityIntegration);
    prefs_.registry()->RegisterTimePref(
        prefs::kAccountTailoredSecurityUpdateTimestamp, base::Time());
    prefs_.registry()->RegisterBooleanPref(prefs::kSafeBrowsingEnhanced, false);
    prefs_.registry()->RegisterBooleanPref(prefs::kSafeBrowsingEnabled, true);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kEnhancedProtectionEnabledViaTailoredSecurity, false);
    prefs_.registry()->RegisterIntegerPref(
        prefs::kTailoredSecuritySyncFlowLastUserInteractionState,
        TailoredSecurityRetryState::UNSET);
    prefs_.registry()->RegisterIntegerPref(
        prefs::kTailoredSecuritySyncFlowRetryState,
        TailoredSecurityRetryState::UNSET);
    prefs_.registry()->RegisterTimePref(
        prefs::kTailoredSecuritySyncFlowLastRunTime, base::Time());

    tailored_security_service_ =
        std::make_unique<TestingTailoredSecurityService>(
            test_shared_loader_factory_, &prefs_, /*sync_service=*/nullptr);
  }

  void TearDown() override {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  TestingTailoredSecurityService* tailored_security_service() {
    return tailored_security_service_.get();
  }

  void Shutdown() { tailored_security_service_->Shutdown(); }

  PrefService* prefs() { return &prefs_; }

  scoped_refptr<network::SharedURLLoaderFactory> URLLoaderFactory() {
    return test_shared_loader_factory_;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<TestingTailoredSecurityService> tailored_security_service_;
  std::vector<bool> notify_sync_calls_;
};

TEST_F(TailoredSecurityServiceTest, GetTailoredSecurityServiceEnabled) {
  tailored_security_service()->SetExpectedURL(
      GURL(kQueryTailoredSecurityServiceUrl));
  tailored_security_service()->SetExpectedTailoredSecurityServiceValue(true);
  tailored_security_service()->StartRequest(base::BindOnce(
      &TestingTailoredSecurityService::GetTailoredSecurityServiceCallback,
      base::Unretained(tailored_security_service())));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TestingTailoredSecurityService::EnsureNoPendingRequestsRemain,
          base::Unretained(tailored_security_service())));
}

TEST_F(TailoredSecurityServiceTest,
       SetTailoredSecurityBitEnabledForTestingTrue) {
  tailored_security_service()->SetExpectedURL(
      GURL(kQueryTailoredSecurityServiceUrl));
  tailored_security_service()->SetExpectedTailoredSecurityServiceValue(true);
  tailored_security_service()->SetExpectedPostData(
      "{\"history_recording_enabled\":true}");
  tailored_security_service()->SetTailoredSecurityBitForTesting(
      true,
      base::BindOnce(
          &TestingTailoredSecurityService::SetTailoredSecurityServiceCallback,
          base::Unretained(tailored_security_service())),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TestingTailoredSecurityService::EnsureNoPendingRequestsRemain,
          base::Unretained(tailored_security_service())));
}

TEST_F(TailoredSecurityServiceTest, SetTailoredSecurityBitForTestingFalse) {
  tailored_security_service()->SetExpectedURL(
      GURL(kQueryTailoredSecurityServiceUrl));
  tailored_security_service()->SetExpectedTailoredSecurityServiceValue(false);
  tailored_security_service()->SetExpectedPostData(
      "{\"history_recording_enabled\":false}");
  tailored_security_service()->SetTailoredSecurityBitForTesting(
      false,
      base::BindOnce(
          &TestingTailoredSecurityService::SetTailoredSecurityServiceCallback,
          base::Unretained(tailored_security_service())),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TestingTailoredSecurityService::EnsureNoPendingRequestsRemain,
          base::Unretained(tailored_security_service())));
}

TEST_F(TailoredSecurityServiceTest, MultipleRequests) {
  tailored_security_service()->SetExpectedURL(
      GURL(kQueryTailoredSecurityServiceUrl));
  tailored_security_service()->SetExpectedTailoredSecurityServiceValue(false);
  tailored_security_service()->SetExpectedPostData(
      "{\"history_recording_enabled\":false}");
  tailored_security_service()->SetTailoredSecurityBitForTesting(
      false,
      base::BindOnce(&TestingTailoredSecurityService::MultipleRequestsCallback,
                     base::Unretained(tailored_security_service())),
      TRAFFIC_ANNOTATION_FOR_TESTS);

  tailored_security_service()->SetExpectedURL(
      GURL(kQueryTailoredSecurityServiceUrl));
  tailored_security_service()->SetExpectedPostData("");
  tailored_security_service()->StartRequest(
      base::BindOnce(&TestingTailoredSecurityService::MultipleRequestsCallback,
                     base::Unretained(tailored_security_service())));

  // Check that both requests are no longer pending.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TestingTailoredSecurityService::EnsureNoPendingRequestsRemain,
          base::Unretained(tailored_security_service())));
}

TEST_F(TailoredSecurityServiceTest, VerifyReadResponse) {
  // Test that properly formatted response with good response code returns true
  // as expected.
  std::unique_ptr<TailoredSecurityService::Request> request(
      new TestRequest(GURL(kQueryTailoredSecurityServiceUrl), base::DoNothing(),
                      net::HTTP_OK, /* response code */
                      "{\n"         /* response body */
                      "  \"history_recording_enabled\": true\n"
                      "}"));
  // ReadResponse deletes the request
  base::Value::Dict response_value =
      TestingTailoredSecurityService::ReadResponse(request.get());
  EXPECT_TRUE(response_value.FindBool("history_recording_enabled").value());
  // Test that properly formatted response with good response code returns false
  // as expected.
  std::unique_ptr<TailoredSecurityService::Request> request2(new TestRequest(
      GURL(kQueryTailoredSecurityServiceUrl), base::DoNothing(), net::HTTP_OK,
      "{\n"
      "  \"history_recording_enabled\": false\n"
      "}"));
  // ReadResponse deletes the request
  base::Value::Dict response_value2 =
      TestingTailoredSecurityService::ReadResponse(request2.get());
  EXPECT_FALSE(response_value2.FindBool("history_recording_enabled").value());

  // Test that a bad response code returns an empty dictionary.
  std::unique_ptr<TailoredSecurityService::Request> request3(
      new TestRequest(GURL(kQueryTailoredSecurityServiceUrl), base::DoNothing(),
                      net::HTTP_UNAUTHORIZED,
                      "{\n"
                      "  \"history_recording_enabled\": true\n"
                      "}"));
  // ReadResponse deletes the request
  base::Value::Dict response_value3 =
      TestingTailoredSecurityService::ReadResponse(request3.get());
  EXPECT_THAT(response_value3, IsEmpty());

  // Test that improperly formatted response returns an empty dictionary.
  // Note: we expect to see a warning when running this test similar to
  //   "Non-JSON response received from history server".
  // This test tests how that situation is handled.
  std::unique_ptr<TailoredSecurityService::Request> request4(new TestRequest(
      GURL(kQueryTailoredSecurityServiceUrl), base::DoNothing(), net::HTTP_OK,
      "{\n"
      "  \"history_recording_enabled\": not true\n"
      "}"));
  // ReadResponse deletes the request
  base::Value::Dict response_value4 =
      TestingTailoredSecurityService::ReadResponse(request4.get());
  EXPECT_THAT(response_value4, IsEmpty());

  // Test that reading a response that doesn't contain the
  // `history_recording_enabled` key, will not contain that key. This way
  // callers of ReadResponse can distinguish between not present and false and
  // can handle those situations appropriately.
  std::unique_ptr<TailoredSecurityService::Request> request5(new TestRequest(
      GURL(kQueryTailoredSecurityServiceUrl), base::DoNothing(), net::HTTP_OK,
      "{\n"
      "  \"history_recording\": true\n"
      "}"));
  // ReadResponse deletes the request
  base::Value::Dict response_value5 =
      TestingTailoredSecurityService::ReadResponse(request5.get());
  EXPECT_THAT(response_value5, Not(IsEmpty()));
  EXPECT_FALSE(
      response_value5.FindBool("history_recording_enabled").has_value());
}

TEST_F(TailoredSecurityServiceTest, TestShutdown) {
  tailored_security_service()->SetExpectedURL(
      GURL(kQueryTailoredSecurityServiceUrl));
  tailored_security_service()->SetExpectedTailoredSecurityServiceValue(false);
  tailored_security_service()->SetTailoredSecurityBitForTesting(
      false,
      base::BindOnce(&TestingTailoredSecurityService::MultipleRequestsCallback,
                     base::Unretained(tailored_security_service())),
      TRAFFIC_ANNOTATION_FOR_TESTS);

  tailored_security_service()->SetExpectedURL(
      GURL(kQueryTailoredSecurityServiceUrl));
  tailored_security_service()->StartRequest(
      base::BindOnce(&TestingTailoredSecurityService::MultipleRequestsCallback,
                     base::Unretained(tailored_security_service())));

  Shutdown();
}

TEST_F(TailoredSecurityServiceTest, NotifiesSyncForEnabled) {
  tailored_security_service()->SetExpectedURL(
      GURL(kQueryTailoredSecurityServiceUrl));
  tailored_security_service()->SetExpectedTailoredSecurityServiceValue(true);

  base::RunLoop run_loop;
  tailored_security_service()->SetNotifySyncUserCallback(
      run_loop.QuitClosure());
  SetSafeBrowsingState(prefs(), SafeBrowsingState::STANDARD_PROTECTION);
  prefs()->SetTime(prefs::kAccountTailoredSecurityUpdateTimestamp,
                   base::Time::Now());
  run_loop.Run();
  EXPECT_TRUE(tailored_security_service()->notify_sync_user_called());
  EXPECT_TRUE(tailored_security_service()->notify_sync_user_called_enabled());
}

TEST_F(TailoredSecurityServiceTest, NotifiesSyncForDisabled) {
  tailored_security_service()->SetExpectedURL(
      GURL(kQueryTailoredSecurityServiceUrl));
  tailored_security_service()->SetExpectedTailoredSecurityServiceValue(false);

  base::RunLoop run_loop;
  tailored_security_service()->SetNotifySyncUserCallback(
      run_loop.QuitClosure());
  SetSafeBrowsingState(prefs(), SafeBrowsingState::ENHANCED_PROTECTION,
                       /*is_esb_enabled_in_sync=*/true);
  prefs()->SetTime(prefs::kAccountTailoredSecurityUpdateTimestamp,
                   base::Time::Now());
  run_loop.Run();
  EXPECT_TRUE(tailored_security_service()->notify_sync_user_called());
  EXPECT_FALSE(tailored_security_service()->notify_sync_user_called_enabled());
}

TEST_F(TailoredSecurityServiceTest,
       RetryEnabledTimestampUpdateCallbackSetsStateToRetryNeeded) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {safe_browsing::kTailoredSecurityRetryForSyncUsers}, {});
  {
    tailored_security_service()->SetExpectedURL(
        GURL(kQueryTailoredSecurityServiceUrl));
    tailored_security_service()->SetExpectedTailoredSecurityServiceValue(true);

    EXPECT_NE(prefs()->GetInteger(prefs::kTailoredSecuritySyncFlowRetryState),
              TailoredSecurityRetryState::RETRY_NEEDED);

    tailored_security_service()->TailoredSecurityTimestampUpdateCallback();

    EXPECT_EQ(prefs()->GetInteger(prefs::kTailoredSecuritySyncFlowRetryState),
              TailoredSecurityRetryState::RETRY_NEEDED);
  }
}

TEST_F(TailoredSecurityServiceTest, RetryDisabledStateRemainsUnset) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {}, {safe_browsing::kTailoredSecurityRetryForSyncUsers});
  {
    tailored_security_service()->SetExpectedURL(
        GURL(kQueryTailoredSecurityServiceUrl));
    tailored_security_service()->SetExpectedTailoredSecurityServiceValue(true);

    EXPECT_EQ(prefs()->GetInteger(prefs::kTailoredSecuritySyncFlowRetryState),
              TailoredSecurityRetryState::UNSET);

    tailored_security_service()->TailoredSecurityTimestampUpdateCallback();

    EXPECT_EQ(prefs()->GetInteger(prefs::kTailoredSecuritySyncFlowRetryState),
              TailoredSecurityRetryState::UNSET);
  }
}

TEST_F(TailoredSecurityServiceTest,
       RetryEnabledTimestampUpdateCallbackRecordsStartTime) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {safe_browsing::kTailoredSecurityRetryForSyncUsers}, {});
  {
    tailored_security_service()->SetExpectedURL(
        GURL(kQueryTailoredSecurityServiceUrl));
    tailored_security_service()->SetExpectedTailoredSecurityServiceValue(true);

    EXPECT_NE(prefs()->GetTime(prefs::kTailoredSecuritySyncFlowLastRunTime),
              base::Time::Now());

    tailored_security_service()->TailoredSecurityTimestampUpdateCallback();

    EXPECT_EQ(prefs()->GetTime(prefs::kTailoredSecuritySyncFlowLastRunTime),
              base::Time::Now());
  }
}

TEST_F(TailoredSecurityServiceTest,
       RetryDisabledTimestampUpdateCallbackDoesNotRecordStartTime) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {}, {safe_browsing::kTailoredSecurityRetryForSyncUsers});
  {
    tailored_security_service()->SetExpectedURL(
        GURL(kQueryTailoredSecurityServiceUrl));
    tailored_security_service()->SetExpectedTailoredSecurityServiceValue(true);

    EXPECT_EQ(prefs()->GetTime(prefs::kTailoredSecuritySyncFlowLastRunTime),
              base::Time());

    tailored_security_service()->TailoredSecurityTimestampUpdateCallback();

    EXPECT_EQ(prefs()->GetTime(prefs::kTailoredSecuritySyncFlowLastRunTime),
              base::Time());
  }
}

TEST_F(TailoredSecurityServiceTest,
       HistorySyncEnabledForUserReturnsFalseWhenSyncServiceIsNull) {
  // One production case where the sync service is not provided to the
  // TailoredSecurityService is on creation for a Guest profile.
  auto tailored_security_service =
      std::make_unique<TestingTailoredSecurityService>(
          URLLoaderFactory(), prefs(),
          /*sync_service=*/nullptr);
  EXPECT_FALSE(tailored_security_service->HistorySyncEnabledForUser());
}

TEST_F(TailoredSecurityServiceTest, CanQueryTailoredSecurityForUrl) {
  // Test cases for URLs that should be allowed.
  EXPECT_TRUE(CanQueryTailoredSecurityForUrl(GURL("https://google.com")));
  EXPECT_TRUE(CanQueryTailoredSecurityForUrl(GURL("https://google.ae")));
  EXPECT_TRUE(CanQueryTailoredSecurityForUrl(GURL("https://google.com.bz")));
  EXPECT_TRUE(CanQueryTailoredSecurityForUrl(GURL("https://google.se")));
  EXPECT_TRUE(CanQueryTailoredSecurityForUrl(GURL("https://www.google.com")));
  EXPECT_TRUE(
      CanQueryTailoredSecurityForUrl(GURL("https://subdomain.google.com")));
  // Non-standard port
  EXPECT_TRUE(
      CanQueryTailoredSecurityForUrl(GURL("https://www.google.com:8080")));
  EXPECT_TRUE(CanQueryTailoredSecurityForUrl(GURL("https://youtube.com")));
  EXPECT_TRUE(CanQueryTailoredSecurityForUrl(GURL("https://www.youtube.com")));
  // Test cases for URLs that should not be allowed.
  EXPECT_FALSE(CanQueryTailoredSecurityForUrl(GURL("https://example.com")));
  EXPECT_FALSE(
      CanQueryTailoredSecurityForUrl(GURL("https://google.com.example.com")));
}

}  // namespace safe_browsing
