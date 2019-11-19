// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_service.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/json/json_string_value_serializer.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "components/metrics/clean_exit_beacon.h"
#include "components/metrics/client_info.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test_enabled_state_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/web_resource/resource_request_allowed_notifier_test_util.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {

// The below seed and signature pair were generated using the server's
// private key.
const char kBase64SeedData[] =
    "CigxZDI5NDY0ZmIzZDc4ZmYxNTU2ZTViNTUxYzY0NDdjYmM3NGU1ZmQwEr0BCh9VTUEtVW5p"
    "Zm9ybWl0eS1UcmlhbC0xMC1QZXJjZW50GICckqUFOAFCB2RlZmF1bHRKCwoHZGVmYXVsdBAB"
    "SgwKCGdyb3VwXzAxEAFKDAoIZ3JvdXBfMDIQAUoMCghncm91cF8wMxABSgwKCGdyb3VwXzA0"
    "EAFKDAoIZ3JvdXBfMDUQAUoMCghncm91cF8wNhABSgwKCGdyb3VwXzA3EAFKDAoIZ3JvdXBf"
    "MDgQAUoMCghncm91cF8wORAB";
const char kBase64SeedSignature[] =
    "MEQCIDD1IVxjzWYncun+9IGzqYjZvqxxujQEayJULTlbTGA/AiAr0oVmEgVUQZBYq5VLOSvy"
    "96JkMYgzTkHPwbv7K/CmgA==";

// A stub for the metrics state manager.
void StubStoreClientInfo(const metrics::ClientInfo& /* client_info */) {}

// A stub for the metrics state manager.
std::unique_ptr<metrics::ClientInfo> StubLoadClientInfo() {
  return std::unique_ptr<metrics::ClientInfo>();
}

base::Version StubGetVersionForSimulation() {
  return base::Version();
}

class TestVariationsServiceClient : public VariationsServiceClient {
 public:
  TestVariationsServiceClient() {
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
  }
  ~TestVariationsServiceClient() override {}

  // VariationsServiceClient:
  base::Callback<base::Version(void)> GetVersionForSimulationCallback()
      override {
    return base::Bind(&StubGetVersionForSimulation);
  }
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return test_shared_loader_factory_;
  }
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override {
    return nullptr;
  }
  bool OverridesRestrictParameter(std::string* parameter) override {
    if (restrict_parameter_.empty())
      return false;
    *parameter = restrict_parameter_;
    return true;
  }
  bool IsEnterprise() override { return false; }

  void set_restrict_parameter(const std::string& value) {
    restrict_parameter_ = value;
  }

  void set_channel(version_info::Channel channel) { channel_ = channel; }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

 private:
  // VariationsServiceClient:
  version_info::Channel GetChannel() override { return channel_; }

  std::string restrict_parameter_;
  version_info::Channel channel_ = version_info::Channel::UNKNOWN;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestVariationsServiceClient);
};

// A test class used to validate expected functionality in VariationsService.
class TestVariationsService : public VariationsService {
 public:
  TestVariationsService(
      std::unique_ptr<web_resource::TestRequestAllowedNotifier> test_notifier,
      PrefService* local_state,
      metrics::MetricsStateManager* state_manager,
      bool use_secure_url)
      : VariationsService(std::make_unique<TestVariationsServiceClient>(),
                          std::move(test_notifier),
                          local_state,
                          state_manager,
                          UIStringOverrider()),
        intercepts_fetch_(true),
        fetch_attempted_(false),
        seed_stored_(false),
        delta_compressed_seed_(false),
        gzip_compressed_seed_(false),
        insecurely_fetched_seed_(false) {
    interception_url_ =
        GetVariationsServerURL(use_secure_url ? USE_HTTPS : USE_HTTP);
    set_variations_server_url(interception_url_);
  }

  ~TestVariationsService() override {}

  GURL interception_url() { return interception_url_; }
  void set_intercepts_fetch(bool value) {
    intercepts_fetch_ = value;
  }
  void set_insecure_url(const GURL& url) {
    set_insecure_variations_server_url(url);
  }
  void set_last_request_was_retry(bool was_retry) {
    set_last_request_was_http_retry(was_retry);
  }
  bool fetch_attempted() const { return fetch_attempted_; }
  bool seed_stored() const { return seed_stored_; }
  const std::string& stored_country() const { return stored_country_; }
  bool delta_compressed_seed() const { return delta_compressed_seed_; }
  bool gzip_compressed_seed() const { return gzip_compressed_seed_; }
  bool insecurely_fetched_seed() const { return insecurely_fetched_seed_; }

  bool CallMaybeRetryOverHTTP() { return CallMaybeRetryOverHTTPForTesting(); }

  void DoActualFetch() override {
    if (intercepts_fetch_) {
      fetch_attempted_ = true;
      return;
    }

    VariationsService::DoActualFetch();
    base::RunLoop().RunUntilIdle();
  }

  bool DoFetchFromURL(const GURL& url, bool is_http_retry) override {
    if (intercepts_fetch_) {
      fetch_attempted_ = true;
      return true;
    }

    return VariationsService::DoFetchFromURL(url, is_http_retry);
  }

  bool StoreSeed(const std::string& seed_data,
                 const std::string& seed_signature,
                 const std::string& country_code,
                 base::Time date_fetched,
                 bool is_delta_compressed,
                 bool is_gzip_compressed,
                 bool fetched_insecurely) override {
    seed_stored_ = true;
    stored_seed_data_ = seed_data;
    stored_country_ = country_code;
    delta_compressed_seed_ = is_delta_compressed;
    gzip_compressed_seed_ = is_gzip_compressed;
    insecurely_fetched_seed_ = fetched_insecurely;
    RecordSuccessfulFetch();
    return true;
  }

  std::unique_ptr<const base::FieldTrial::EntropyProvider>
  CreateLowEntropyProvider() override {
    return std::unique_ptr<const base::FieldTrial::EntropyProvider>(nullptr);
  }

  TestVariationsServiceClient* client() {
    return static_cast<TestVariationsServiceClient*>(
        VariationsService::client());
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return client()->test_url_loader_factory();
  }

 private:
  GURL interception_url_;
  bool intercepts_fetch_;
  bool fetch_attempted_;
  bool seed_stored_;
  std::string stored_seed_data_;
  std::string stored_country_;
  bool delta_compressed_seed_;
  bool gzip_compressed_seed_;
  bool insecurely_fetched_seed_;

  DISALLOW_COPY_AND_ASSIGN(TestVariationsService);
};

class TestVariationsServiceObserver : public VariationsService::Observer {
 public:
  TestVariationsServiceObserver()
      : best_effort_changes_notified_(0), crticial_changes_notified_(0) {}
  ~TestVariationsServiceObserver() override {}

  void OnExperimentChangesDetected(Severity severity) override {
    switch (severity) {
      case BEST_EFFORT:
        ++best_effort_changes_notified_;
        break;
      case CRITICAL:
        ++crticial_changes_notified_;
        break;
    }
  }

  int best_effort_changes_notified() const {
    return best_effort_changes_notified_;
  }

  int crticial_changes_notified() const {
    return crticial_changes_notified_;
  }

 private:
  // Number of notification received with BEST_EFFORT severity.
  int best_effort_changes_notified_;

  // Number of notification received with CRITICAL severity.
  int crticial_changes_notified_;

  DISALLOW_COPY_AND_ASSIGN(TestVariationsServiceObserver);
};

// Constants used to create the test seed.
const char kTestSeedStudyName[] = "test";
const char kTestSeedExperimentName[] = "abc";
const int kTestSeedExperimentProbability = 100;
const char kTestSeedSerialNumber[] = "123";

// Populates |seed| with simple test data. The resulting seed will contain one
// study called "test", which contains one experiment called "abc" with
// probability weight 100. |seed|'s study field will be cleared before adding
// the new study.
VariationsSeed CreateTestSeed() {
  VariationsSeed seed;
  Study* study = seed.add_study();
  study->set_name(kTestSeedStudyName);
  study->set_default_experiment_name(kTestSeedExperimentName);
  Study_Experiment* experiment = study->add_experiment();
  experiment->set_name(kTestSeedExperimentName);
  experiment->set_probability_weight(kTestSeedExperimentProbability);
  seed.set_serial_number(kTestSeedSerialNumber);
  return seed;
}

// Serializes |seed| to protobuf binary format.
std::string SerializeSeed(const VariationsSeed& seed) {
  std::string serialized_seed;
  seed.SerializeToString(&serialized_seed);
  return serialized_seed;
}

// Converts |list_value| to a string, to make it easier for debugging.
std::string ListValueToString(const base::ListValue& list_value) {
  std::string json;
  JSONStringValueSerializer serializer(&json);
  serializer.set_pretty_print(true);
  serializer.Serialize(list_value);
  return json;
}

}  // namespace

class VariationsServiceTest : public ::testing::Test {
 protected:
  VariationsServiceTest()
      : network_tracker_(network::TestNetworkConnectionTracker::GetInstance()),
        enabled_state_provider_(
            new metrics::TestEnabledStateProvider(false, false)) {
    VariationsService::RegisterPrefs(prefs_.registry());
    metrics::CleanExitBeacon::RegisterPrefs(prefs_.registry());
    metrics::MetricsStateManager::RegisterPrefs(prefs_.registry());
  }

  metrics::MetricsStateManager* GetMetricsStateManager() {
    // Lazy-initialize the metrics_state_manager so that it correctly reads the
    // stability state from prefs after tests have a chance to initialize it.
    if (!metrics_state_manager_) {
      metrics_state_manager_ = metrics::MetricsStateManager::Create(
          &prefs_, enabled_state_provider_.get(), base::string16(),
          base::Bind(&StubStoreClientInfo), base::Bind(&StubLoadClientInfo));
    }
    return metrics_state_manager_.get();
  }

 protected:
  TestingPrefServiceSimple prefs_;
  network::TestNetworkConnectionTracker* network_tracker_;

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<metrics::TestEnabledStateProvider> enabled_state_provider_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;

  DISALLOW_COPY_AND_ASSIGN(VariationsServiceTest);
};

TEST_F(VariationsServiceTest, GetVariationsServerURL) {
  const std::string default_variations_url =
      VariationsService::GetDefaultVariationsServerURLForTesting();

  std::string value;
  std::unique_ptr<TestVariationsServiceClient> client =
      std::make_unique<TestVariationsServiceClient>();
  TestVariationsServiceClient* raw_client = client.get();
  VariationsService service(
      std::move(client),
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), UIStringOverrider());
  GURL url = service.GetVariationsServerURL(TestVariationsService::USE_HTTPS);
  EXPECT_TRUE(base::StartsWith(url.spec(), default_variations_url,
                               base::CompareCase::SENSITIVE));
  EXPECT_FALSE(net::GetValueForKeyInQuery(url, "restrict", &value));
  // There should be a fallback URL since restrict mode is not set.
  EXPECT_NE(GURL(),
            service.GetVariationsServerURL(TestVariationsService::USE_HTTP));

  prefs_.SetString(prefs::kVariationsRestrictParameter, "restricted");
  url = service.GetVariationsServerURL(TestVariationsService::USE_HTTPS);
  EXPECT_TRUE(base::StartsWith(url.spec(), default_variations_url,
                               base::CompareCase::SENSITIVE));
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "restrict", &value));
  EXPECT_EQ("restricted", value);
  // No fallback URL because restrict mode is set.
  EXPECT_EQ(GURL(),
            service.GetVariationsServerURL(TestVariationsService::USE_HTTP));

  // A client override should take precedence over what's in prefs_.
  raw_client->set_restrict_parameter("client");
  url = service.GetVariationsServerURL(TestVariationsService::USE_HTTPS);
  EXPECT_TRUE(base::StartsWith(url.spec(), default_variations_url,
                               base::CompareCase::SENSITIVE));
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "restrict", &value));
  EXPECT_EQ("client", value);
  // No fallback URL because restrict mode is set.
  EXPECT_EQ(GURL(),
            service.GetVariationsServerURL(TestVariationsService::USE_HTTP));

  // The value set via SetRestrictMode() should take precedence over what's
  // in prefs_ and a client override.
  service.SetRestrictMode("override");
  url = service.GetVariationsServerURL(TestVariationsService::USE_HTTPS);
  EXPECT_TRUE(base::StartsWith(url.spec(), default_variations_url,
                               base::CompareCase::SENSITIVE));
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "restrict", &value));
  EXPECT_EQ("override", value);
  // No fallback URL because restrict mode is set.
  EXPECT_EQ(GURL(),
            service.GetVariationsServerURL(TestVariationsService::USE_HTTP));
}

TEST_F(VariationsServiceTest, VariationsURLHasParams) {
  std::unique_ptr<TestVariationsServiceClient> client =
      std::make_unique<TestVariationsServiceClient>();
  TestVariationsServiceClient* raw_client = client.get();
  VariationsService service(
      std::move(client),
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), UIStringOverrider());
  raw_client->set_channel(version_info::Channel::UNKNOWN);
  GURL url = service.GetVariationsServerURL(TestVariationsService::USE_HTTPS);

  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "osname", &value));
  EXPECT_FALSE(value.empty());

  std::string milestone;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "milestone", &milestone));
  EXPECT_FALSE(milestone.empty());

  // Channel param should not be present for UNKNOWN channel.
  std::string channel;
  EXPECT_FALSE(net::GetValueForKeyInQuery(url, "channel", &channel));
  EXPECT_TRUE(channel.empty());

  raw_client->set_channel(version_info::Channel::STABLE);
  url = service.GetVariationsServerURL(TestVariationsService::USE_HTTPS);
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "channel", &channel));
  EXPECT_FALSE(channel.empty());
}

TEST_F(VariationsServiceTest, RequestsInitiallyNotAllowed) {
  std::unique_ptr<net::test::MockNetworkChangeNotifier>
      network_change_notifier = net::test::MockNetworkChangeNotifier::Create();
  // Pass ownership to TestVariationsService, but keep a weak pointer to
  // manipulate it for this test.
  std::unique_ptr<web_resource::TestRequestAllowedNotifier> test_notifier =
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_);
  web_resource::TestRequestAllowedNotifier* raw_notifier = test_notifier.get();
  TestVariationsService test_service(std::move(test_notifier), &prefs_,
                                     GetMetricsStateManager(), true);
  test_service.InitResourceRequestedAllowedNotifier();

  // Force the notifier to initially disallow requests.
  raw_notifier->SetRequestsAllowedOverride(false);
  test_service.StartRepeatedVariationsSeedFetch();
  EXPECT_FALSE(test_service.fetch_attempted());

  raw_notifier->NotifyObserver();
  EXPECT_TRUE(test_service.fetch_attempted());
}

TEST_F(VariationsServiceTest, RequestsInitiallyAllowed) {
  // Pass ownership to TestVariationsService, but keep a weak pointer to
  // manipulate it for this test.
  std::unique_ptr<web_resource::TestRequestAllowedNotifier> test_notifier =
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_);
  web_resource::TestRequestAllowedNotifier* raw_notifier = test_notifier.get();
  TestVariationsService test_service(std::move(test_notifier), &prefs_,
                                     GetMetricsStateManager(), true);

  raw_notifier->SetRequestsAllowedOverride(true);
  test_service.StartRepeatedVariationsSeedFetch();
  EXPECT_TRUE(test_service.fetch_attempted());
}

TEST_F(VariationsServiceTest, SeedStoredWhenOKStatus) {
  VariationsService::EnableFetchForTesting();

  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);

  EXPECT_FALSE(service.seed_stored());

  service.test_url_loader_factory()->AddResponse(
      service.interception_url().spec(), SerializeSeed(CreateTestSeed()));
  service.set_intercepts_fetch(false);
  service.DoActualFetch();

  EXPECT_TRUE(service.seed_stored());
}

TEST_F(VariationsServiceTest, SeedNotStoredWhenNonOKStatus) {
  const net::HttpStatusCode non_ok_status_codes[] = {
      net::HTTP_NO_CONTENT,          net::HTTP_NOT_MODIFIED,
      net::HTTP_NOT_FOUND,           net::HTTP_INTERNAL_SERVER_ERROR,
      net::HTTP_SERVICE_UNAVAILABLE,
  };

  VariationsService::EnableFetchForTesting();

  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);
  service.set_intercepts_fetch(false);
  for (size_t i = 0; i < base::size(non_ok_status_codes); ++i) {
    EXPECT_TRUE(prefs_.FindPreference(prefs::kVariationsCompressedSeed)
                    ->IsDefaultValue());
    service.test_url_loader_factory()->ClearResponses();
    service.test_url_loader_factory()->AddResponse(
        service.interception_url().spec(), "", non_ok_status_codes[i]);
    service.DoActualFetch();

    EXPECT_TRUE(prefs_.FindPreference(prefs::kVariationsCompressedSeed)
                    ->IsDefaultValue());
  }
}

TEST_F(VariationsServiceTest, RequestGzipCompressedSeed) {
  VariationsService::EnableFetchForTesting();

  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);
  service.set_intercepts_fetch(false);
  net::HttpRequestHeaders intercepted_headers;
  service.test_url_loader_factory()->AddResponse(
      service.interception_url().spec(), "");
  service.test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        intercepted_headers = request.headers;
      }));
  service.DoActualFetch();

  std::string field;
  ASSERT_TRUE(intercepted_headers.GetHeader("A-IM", &field));
  EXPECT_EQ("gzip", field);
}

TEST_F(VariationsServiceTest, InstanceManipulations) {
  struct  {
    std::string im;
    bool delta_compressed;
    bool gzip_compressed;
    bool seed_stored;
  } cases[] = {
    {"", false, false, true},
    {"IM:gzip", false, true, true},
    {"IM:x-bm", true, false, true},
    {"IM:x-bm,gzip", true, true, true},
    {"IM: x-bm, gzip", true, true, true},
    {"IM:gzip,x-bm", false, false, false},
    {"IM:deflate,x-bm,gzip", false, false, false},
  };

  std::string serialized_seed = SerializeSeed(CreateTestSeed());
  VariationsService::EnableFetchForTesting();
  for (size_t i = 0; i < base::size(cases); ++i) {
    TestVariationsService service(
        std::make_unique<web_resource::TestRequestAllowedNotifier>(
            &prefs_, network_tracker_),
        &prefs_, GetMetricsStateManager(), true);
    service.set_intercepts_fetch(false);

    std::string headers("HTTP/1.1 200 OK\n\n");
    auto head = network::mojom::URLResponseHead::New();
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(headers));
    if (!cases[i].im.empty())
      head->headers->AddHeader(cases[i].im);
    network::URLLoaderCompletionStatus status;
    status.decoded_body_length = serialized_seed.size();
    service.test_url_loader_factory()->AddResponse(
        service.interception_url(), std::move(head), serialized_seed, status);

    service.DoActualFetch();

    EXPECT_EQ(cases[i].seed_stored, service.seed_stored());
    EXPECT_EQ(cases[i].delta_compressed, service.delta_compressed_seed());
    EXPECT_EQ(cases[i].gzip_compressed, service.gzip_compressed_seed());
  }
}

TEST_F(VariationsServiceTest, CountryHeader) {
  std::string serialized_seed = SerializeSeed(CreateTestSeed());
  VariationsService::EnableFetchForTesting();

  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);
  EXPECT_FALSE(service.seed_stored());
  service.set_intercepts_fetch(false);

  std::string headers("HTTP/1.1 200 OK\n\n");
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  head->headers->AddHeader("X-Country: test");
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = serialized_seed.size();
  service.test_url_loader_factory()->AddResponse(
      service.interception_url(), std::move(head), serialized_seed, status);

  service.DoActualFetch();

  EXPECT_TRUE(service.seed_stored());
  EXPECT_EQ("test", service.stored_country());
}

TEST_F(VariationsServiceTest, Observer) {
  VariationsService service(
      std::make_unique<TestVariationsServiceClient>(),
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), UIStringOverrider());

  struct {
    int normal_count;
    int best_effort_count;
    int critical_count;
    int expected_best_effort_notifications;
    int expected_crtical_notifications;
  } cases[] = {
      {0, 0, 0, 0, 0},
      {1, 0, 0, 0, 0},
      {10, 0, 0, 0, 0},
      {0, 1, 0, 1, 0},
      {0, 10, 0, 1, 0},
      {0, 0, 1, 0, 1},
      {0, 0, 10, 0, 1},
      {0, 1, 1, 0, 1},
      {1, 1, 1, 0, 1},
      {1, 1, 0, 1, 0},
      {1, 0, 1, 0, 1},
  };

  for (size_t i = 0; i < base::size(cases); ++i) {
    TestVariationsServiceObserver observer;
    service.AddObserver(&observer);

    VariationsSeedSimulator::Result result;
    result.normal_group_change_count = cases[i].normal_count;
    result.kill_best_effort_group_change_count = cases[i].best_effort_count;
    result.kill_critical_group_change_count = cases[i].critical_count;
    service.NotifyObservers(result);

    EXPECT_EQ(cases[i].expected_best_effort_notifications,
              observer.best_effort_changes_notified()) << i;
    EXPECT_EQ(cases[i].expected_crtical_notifications,
              observer.crticial_changes_notified()) << i;

    service.RemoveObserver(&observer);
  }
}

TEST_F(VariationsServiceTest, LoadPermanentConsistencyCountry) {
  struct {
    const char* permanent_overridden_country_before;
    // Comma separated list, NULL if the pref isn't set initially.
    const char* permanent_consistency_country_before;
    const char* version;
    // NULL indicates that no latest country code is present.
    const char* latest_country_code;
    // Comma separated list.
    const char* permanent_consistency_country_after;
    std::string expected_country;
    LoadPermanentConsistencyCountryResult expected_result;
  } test_cases[] = {
      // Existing permanent overridden country.
      {"ca", "20.0.0.0,us", "20.0.0.0", "us", "20.0.0.0,us", "ca",
       LOAD_COUNTRY_HAS_PERMANENT_OVERRIDDEN_COUNTRY},
      {"us", "20.0.0.0,us", "20.0.0.0", "us", "20.0.0.0,us", "us",
       LOAD_COUNTRY_HAS_PERMANENT_OVERRIDDEN_COUNTRY},
      {"ca", nullptr, "20.0.0.0", nullptr, nullptr, "ca",
       LOAD_COUNTRY_HAS_PERMANENT_OVERRIDDEN_COUNTRY},

      // Existing pref value present for this version.
      {"", "20.0.0.0,us", "20.0.0.0", "ca", "20.0.0.0,us", "us",
       LOAD_COUNTRY_HAS_BOTH_VERSION_EQ_COUNTRY_NEQ},
      {"", "20.0.0.0,us", "20.0.0.0", "us", "20.0.0.0,us", "us",
       LOAD_COUNTRY_HAS_BOTH_VERSION_EQ_COUNTRY_EQ},
      {"", "20.0.0.0,us", "20.0.0.0", nullptr, "20.0.0.0,us", "us",
       LOAD_COUNTRY_HAS_PREF_NO_SEED_VERSION_EQ},

      // Existing pref value present for a different version.
      {"", "19.0.0.0,ca", "20.0.0.0", "us", "20.0.0.0,us", "us",
       LOAD_COUNTRY_HAS_BOTH_VERSION_NEQ_COUNTRY_NEQ},
      {"", "19.0.0.0,us", "20.0.0.0", "us", "20.0.0.0,us", "us",
       LOAD_COUNTRY_HAS_BOTH_VERSION_NEQ_COUNTRY_EQ},
      {"", "19.0.0.0,ca", "20.0.0.0", nullptr, "19.0.0.0,ca", "",
       LOAD_COUNTRY_HAS_PREF_NO_SEED_VERSION_NEQ},

      // No existing pref value present.
      {"", nullptr, "20.0.0.0", "us", "20.0.0.0,us", "us",
       LOAD_COUNTRY_NO_PREF_HAS_SEED},
      {"", nullptr, "20.0.0.0", nullptr, "", "", LOAD_COUNTRY_NO_PREF_NO_SEED},
      {"", "", "20.0.0.0", "us", "20.0.0.0,us", "us",
       LOAD_COUNTRY_NO_PREF_HAS_SEED},
      {"", "", "20.0.0.0", nullptr, "", "", LOAD_COUNTRY_NO_PREF_NO_SEED},

      // Invalid existing pref value.
      {"", "20.0.0.0", "20.0.0.0", "us", "20.0.0.0,us", "us",
       LOAD_COUNTRY_INVALID_PREF_HAS_SEED},
      {"", "20.0.0.0", "20.0.0.0", nullptr, "", "",
       LOAD_COUNTRY_INVALID_PREF_NO_SEED},
      {"", "20.0.0.0,us,element3", "20.0.0.0", "us", "20.0.0.0,us", "us",
       LOAD_COUNTRY_INVALID_PREF_HAS_SEED},
      {"", "20.0.0.0,us,element3", "20.0.0.0", nullptr, "", "",
       LOAD_COUNTRY_INVALID_PREF_NO_SEED},
      {"", "badversion,ca", "20.0.0.0", "us", "20.0.0.0,us", "us",
       LOAD_COUNTRY_INVALID_PREF_HAS_SEED},
      {"", "badversion,ca", "20.0.0.0", nullptr, "", "",
       LOAD_COUNTRY_INVALID_PREF_NO_SEED},
  };

  for (const auto& test : test_cases) {
    VariationsService service(
        std::make_unique<TestVariationsServiceClient>(),
        std::make_unique<web_resource::TestRequestAllowedNotifier>(
            &prefs_, network_tracker_),
        &prefs_, GetMetricsStateManager(), UIStringOverrider());

    if (!test.permanent_overridden_country_before) {
      prefs_.ClearPref(prefs::kVariationsPermanentOverriddenCountry);
    } else {
      prefs_.SetString(prefs::kVariationsPermanentOverriddenCountry,
                       test.permanent_overridden_country_before);
    }

    if (!test.permanent_consistency_country_before) {
      prefs_.ClearPref(prefs::kVariationsPermanentConsistencyCountry);
    } else {
      base::ListValue list_value;
      for (const std::string& component :
           base::SplitString(test.permanent_consistency_country_before, ",",
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
        list_value.AppendString(component);
      }
      prefs_.Set(prefs::kVariationsPermanentConsistencyCountry, list_value);
    }

    VariationsSeed seed(CreateTestSeed());
    std::string latest_country;
    if (test.latest_country_code)
      latest_country = test.latest_country_code;

    base::HistogramTester histogram_tester;
    EXPECT_EQ(test.expected_country,
              service.LoadPermanentConsistencyCountry(
                  base::Version(test.version), latest_country))
        << test.permanent_consistency_country_before << ", " << test.version
        << ", " << test.latest_country_code;

    base::ListValue expected_list_value;
    for (const std::string& component :
         base::SplitString(test.permanent_consistency_country_after, ",",
                           base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      expected_list_value.AppendString(component);
    }
    const base::ListValue* pref_value =
        prefs_.GetList(prefs::kVariationsPermanentConsistencyCountry);
    EXPECT_EQ(ListValueToString(expected_list_value),
              ListValueToString(*pref_value))
        << test.permanent_consistency_country_before << ", " << test.version
        << ", " << test.latest_country_code;

    histogram_tester.ExpectUniqueSample(
        "Variations.LoadPermanentConsistencyCountryResult",
        test.expected_result, 1);
  }
}

TEST_F(VariationsServiceTest, GetStoredPermanentCountry) {
  struct {
    // The old overridden country, empty string if the pref isn't set initially.
    const std::string permanent_overridden_country_before;
    // Comma separated list, NULL if the pref isn't set initially.
    const std::string permanent_consistency_country_before;
    const std::string expected_country;
  } test_cases[] = {
      {"", "20.0.0.0,us", "us"},
      {"us", "20.0.0.0,us", "us"},
      {"ca", "20.0.0.0,us", "ca"},
      {"ca", "", "ca"},
  };

  for (const auto& test : test_cases) {
    TestVariationsService service(
        std::make_unique<web_resource::TestRequestAllowedNotifier>(
            &prefs_, network_tracker_),
        &prefs_, GetMetricsStateManager(), true);

    if (test.permanent_overridden_country_before.empty()) {
      prefs_.ClearPref(prefs::kVariationsPermanentOverriddenCountry);
    } else {
      prefs_.SetString(prefs::kVariationsPermanentOverriddenCountry,
                       test.permanent_overridden_country_before);
    }

    if (test.permanent_consistency_country_before.empty()) {
      prefs_.ClearPref(prefs::kVariationsPermanentConsistencyCountry);
    } else {
      base::ListValue list_value;
      for (const std::string& component :
           base::SplitString(test.permanent_consistency_country_before, ",",
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
        list_value.AppendString(component);
      }
      prefs_.Set(prefs::kVariationsPermanentConsistencyCountry, list_value);
    }

    VariationsSeed seed(CreateTestSeed());

    EXPECT_EQ(test.expected_country, service.GetStoredPermanentCountry())
        << test.permanent_overridden_country_before << ", "
        << test.permanent_consistency_country_before;
  }
}

TEST_F(VariationsServiceTest, OverrideStoredPermanentCountry) {
  const std::string kPrefCa = "ca";
  const std::string kPrefUs = "us";

  struct {
    // The old overridden country, empty string if the pref isn't set initially.
    const std::string pref_value_before;
    const std::string country_code_override;
    // The expected override country.
    const std::string expected_pref_value_after;
    // Is the pref expected to be updated or not.
    const bool has_updated;
  } test_cases[] = {
      {kPrefUs, "ca", kPrefCa, true},
      {kPrefUs, "us", kPrefUs, false},
      {kPrefUs, "", "", true},
      {"", "ca", kPrefCa, true},
  };

  for (const auto& test : test_cases) {
    TestVariationsService service(
        std::make_unique<web_resource::TestRequestAllowedNotifier>(
            &prefs_, network_tracker_),
        &prefs_, GetMetricsStateManager(), true);

    if (test.pref_value_before.empty()) {
      prefs_.ClearPref(prefs::kVariationsPermanentOverriddenCountry);
    } else {
      prefs_.SetString(prefs::kVariationsPermanentOverriddenCountry,
                       test.pref_value_before);
    }

    VariationsSeed seed(CreateTestSeed());

    EXPECT_EQ(test.has_updated, service.OverrideStoredPermanentCountry(
                                    test.country_code_override))
        << test.pref_value_before << ", " << test.country_code_override;

    const std::string pref_value =
        prefs_.GetString(prefs::kVariationsPermanentOverriddenCountry);
    EXPECT_EQ(test.expected_pref_value_after, pref_value)
        << test.pref_value_before << ", " << test.country_code_override;
  }
}

TEST_F(VariationsServiceTest, SafeMode_StartingRequestIncrementsFetchFailures) {
  prefs_.SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 1);

  VariationsService::EnableFetchForTesting();

  // Create a variations service and start the fetch.
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);
  service.set_intercepts_fetch(false);
  service.DoActualFetch();

  EXPECT_EQ(2, prefs_.GetInteger(prefs::kVariationsFailedToFetchSeedStreak));
}

TEST_F(VariationsServiceTest, SafeMode_SuccessfulFetchClearsFailureStreaks) {
  prefs_.SetInteger(prefs::kVariationsCrashStreak, 2);
  prefs_.SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 1);

  VariationsService::EnableFetchForTesting();

  std::unique_ptr<net::test::MockNetworkChangeNotifier>
      network_change_notifier = net::test::MockNetworkChangeNotifier::Create();

  // Create a variations service and perform a successful fetch.
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);
  service.set_intercepts_fetch(false);

  std::string response;
  ASSERT_TRUE(base::Base64Decode(kBase64SeedData, &response));
  const std::string seed_signature_header =
      std::string("X-Seed-Signature:") + kBase64SeedSignature;

  std::string headers("HTTP/1.1 200 OK\n\n");
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  head->headers->AddHeader(seed_signature_header);
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = response.size();
  service.test_url_loader_factory()->AddResponse(
      service.interception_url(), std::move(head), response, status);

  service.DoActualFetch();

  // Verify that the streaks were reset.
  EXPECT_EQ(0, prefs_.GetInteger(prefs::kVariationsCrashStreak));
  EXPECT_EQ(0, prefs_.GetInteger(prefs::kVariationsFailedToFetchSeedStreak));
}

TEST_F(VariationsServiceTest, SafeMode_NotModifiedFetchClearsFailureStreaks) {
  prefs_.SetInteger(prefs::kVariationsCrashStreak, 2);
  prefs_.SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 1);

  VariationsService::EnableFetchForTesting();

  // Create a variations service and perform a successful fetch.
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);
  service.set_intercepts_fetch(false);

  std::string headers("HTTP/1.1 304 Not Modified\n\n");
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  network::URLLoaderCompletionStatus status;
  service.test_url_loader_factory()->AddResponse(service.interception_url(),
                                                 std::move(head), "", status);

  service.DoActualFetch();

  EXPECT_EQ(0, prefs_.GetInteger(prefs::kVariationsCrashStreak));
  EXPECT_EQ(0, prefs_.GetInteger(prefs::kVariationsFailedToFetchSeedStreak));
}

TEST_F(VariationsServiceTest, FieldTrialCreatorInitializedCorrectly) {
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);

  // Call will crash in service's VariationsFieldTrialCreator if not initialized
  // correctly.
  service.GetClientFilterableStateForVersionCalledForTesting();
}

TEST_F(VariationsServiceTest, InsecurelyFetchedSetWhenHTTP) {
  std::string serialized_seed = SerializeSeed(CreateTestSeed());
  VariationsService::EnableFetchForTesting();
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), false);
  service.set_intercepts_fetch(false);
  service.test_url_loader_factory()->AddResponse(
      service.interception_url().spec(), serialized_seed);
  base::HistogramTester histogram_tester;
  // Note: We call DoFetchFromURL() here instead of DoActualFetch() since the
  // latter doesn't pass true to |http_retry|.
  service.DoFetchFromURL(service.interception_url(), true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(service.insecurely_fetched_seed());
  histogram_tester.ExpectUniqueSample(
      "Variations.SeedFetchResponseOrErrorCode.HTTP", net::HTTP_OK, 1);
}

TEST_F(VariationsServiceTest, InsecurelyFetchedNotSetWhenHTTPS) {
  std::string serialized_seed = SerializeSeed(CreateTestSeed());
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);
  VariationsService::EnableFetchForTesting();
  service.set_intercepts_fetch(false);
  service.test_url_loader_factory()->AddResponse(
      service.interception_url().spec(), serialized_seed);
  base::HistogramTester histogram_tester;
  service.DoActualFetch();
  EXPECT_FALSE(service.insecurely_fetched_seed());
  histogram_tester.ExpectUniqueSample("Variations.SeedFetchResponseOrErrorCode",
                                      net::HTTP_OK, 1);
}

TEST_F(VariationsServiceTest, RetryOverHTTPIfURLisSet) {
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);
  service.set_intercepts_fetch(true);
  service.set_last_request_was_retry(false);
  service.set_insecure_url(GURL("http://example.test"));
  EXPECT_TRUE(service.CallMaybeRetryOverHTTP());
  EXPECT_TRUE(service.fetch_attempted());
}

TEST_F(VariationsServiceTest, DoNotRetryAfterARetry) {
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);
  service.set_intercepts_fetch(true);
  service.set_last_request_was_retry(true);
  service.set_insecure_url(GURL("http://example.test"));
  EXPECT_FALSE(service.CallMaybeRetryOverHTTP());
  EXPECT_FALSE(service.fetch_attempted());
}

TEST_F(VariationsServiceTest, DoNotRetryIfInsecureURLIsHTTPS) {
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);
  service.set_intercepts_fetch(true);
  service.set_last_request_was_retry(false);
  service.set_insecure_url(GURL("https://example.test"));
  EXPECT_FALSE(service.CallMaybeRetryOverHTTP());
  EXPECT_FALSE(service.fetch_attempted());
}

TEST_F(VariationsServiceTest, SeedNotStoredWhenRedirected) {
  VariationsService::EnableFetchForTesting();

  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);

  EXPECT_FALSE(service.seed_stored());

  net::RedirectInfo redirect_info;
  redirect_info.status_code = 301;
  redirect_info.new_url = service.interception_url();
  network::TestURLLoaderFactory::Redirects redirects;
  redirects.push_back({redirect_info, network::mojom::URLResponseHead::New()});

  auto head = network::CreateURLResponseHead(net::HTTP_OK);

  service.test_url_loader_factory()->AddResponse(
      service.interception_url(), std::move(head),
      SerializeSeed(CreateTestSeed()), network::URLLoaderCompletionStatus(),
      std::move(redirects));

  service.set_intercepts_fetch(false);
  service.DoActualFetch();
  EXPECT_FALSE(service.seed_stored());
}

TEST_F(VariationsServiceTest, NullResponseReceivedWithHTTPOk) {
  VariationsService::EnableFetchForTesting();

  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);
  service.set_intercepts_fetch(false);

  std::string response;
  ASSERT_TRUE(base::Base64Decode(kBase64SeedData, &response));
  const std::string seed_signature_header =
      std::string("X-Seed-Signature:") + kBase64SeedSignature;

  std::string headers("HTTP/1.1 200 OK\n\n");
  auto head = network::mojom::URLResponseHead::New();
  auto http_response_headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  head->headers = http_response_headers;
  EXPECT_EQ(net::HTTP_OK, http_response_headers->response_code());
  http_response_headers->AddHeader(seed_signature_header);
  // Set ERR_FAILED status code despite the 200 response code.
  network::URLLoaderCompletionStatus status(net::ERR_FAILED);
  status.decoded_body_length = response.size();
  service.test_url_loader_factory()->AddResponse(
      service.interception_url(), std::move(head), response, status,
      network::TestURLLoaderFactory::Redirects(),
      // We pass the flag below to preserve the 200 code with an error response.
      network::TestURLLoaderFactory::kSendHeadersOnNetworkError);
  EXPECT_EQ(net::HTTP_OK, http_response_headers->response_code());

  base::HistogramTester histogram_tester;
  service.DoActualFetch();
  EXPECT_FALSE(service.seed_stored());
  histogram_tester.ExpectUniqueSample("Variations.SeedFetchResponseOrErrorCode",
                                      net::ERR_FAILED, 1);
}

TEST_F(VariationsServiceTest,
       VariationsServiceStartsRequestOnNetworkChange) {
  // Verifies VariationsService does a request when network status changes from
  // none to connected. This is a regression test for https://crbug.com/826930.
  VariationsService::EnableFetchForTesting();
  network_tracker_->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);
  service.set_intercepts_fetch(false);
  service.CancelCurrentRequestForTesting();
  base::RunLoop().RunUntilIdle();
  // Simulate starting Chrome browser.
  service.StartRepeatedVariationsSeedFetchForTesting();
  const int initial_request_count = service.request_count();
  // The variations seed can not be fetched if disconnected. So even we start
  // repeated variations seed fetch (on Chrome start), no requests will be made.
  EXPECT_EQ(0, initial_request_count);

  service.GetResourceRequestAllowedNotifierForTesting()
      ->SetObserverRequestedForTesting(true);
  network_tracker_->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  base::RunLoop().RunUntilIdle();

  const int final_request_count = service.request_count();
  // The request will be made once Chrome gets online.
  EXPECT_EQ(initial_request_count + 1, final_request_count);
}

// TODO(isherman): Add an integration test for saving and loading a safe seed,
// once the loading functionality is implemented on the seed store.

}  // namespace variations
