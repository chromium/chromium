// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_service.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/byte_size.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/runtime_field_trial_overrides.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "base/version.h"
#include "base/version_info/version_info.h"
#include "components/metrics/clean_exit_beacon.h"
#include "components/metrics/client_info.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/startup_visibility.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_seed_simulator.h"
#include "components/variations/variations_switches.h"
#include "components/version_info/channel.h"
#include "components/web_resource/resource_request_allowed_notifier_test_util.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// A fake version of RuntimeMutableFeaturesHandlerBase, to generate PassKeys for
// testing purposes.  The real RuntimeMutableFeaturesHandlerBase class is not
// defined in `components/`. We're creating a surrogate of it here so that we
// can generate PassKeys for testing, without violating dependency layering.
namespace metrics {
class RuntimeMutableFeaturesHandlerBase {
 public:
  using PassKey = base::PassKey<RuntimeMutableFeaturesHandlerBase>;
  static PassKey CreatePassKeyForTesting() { return PassKey(); }
};
}  // namespace metrics

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

const char kApplyRuntimeMutableChangesResultMetric[] =
    "Variations.ApplyRuntimeMutableChanges.Result";

// TODO(crbug.com/40742801): Remove when fake VariationsServiceClient created.
class TestVariationsServiceClient : public VariationsServiceClient {
 public:
  TestVariationsServiceClient() {
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
  }

  TestVariationsServiceClient(const TestVariationsServiceClient&) = delete;
  TestVariationsServiceClient& operator=(const TestVariationsServiceClient&) =
      delete;

  ~TestVariationsServiceClient() override = default;

  // VariationsServiceClient:
  base::Version GetVersionForSimulation() override { return base::Version(); }
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return test_shared_loader_factory_;
  }
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override {
    return nullptr;
  }
  bool OverridesRestrictParameter(std::string* parameter) override {
    if (restrict_parameter_.empty()) {
      return false;
    }
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
                          state_manager) {
    interception_url_ =
        GetVariationsServerURL(use_secure_url ? USE_HTTPS : USE_HTTP);
    set_variations_server_url(interception_url_);
  }

  TestVariationsService(const TestVariationsService&) = delete;
  TestVariationsService& operator=(const TestVariationsService&) = delete;

  ~TestVariationsService() override = default;

  GURL interception_url() { return interception_url_; }
  void set_intercepts_fetch(bool value) { intercepts_fetch_ = value; }
  void set_insecure_url(const GURL& url) {
    set_insecure_variations_server_url(url);
  }
  void set_last_request_was_retry(bool was_retry) {
    set_last_request_was_http_retry(was_retry);
  }
  void set_latest_serial_number(const std::string& serial_number) {
    latest_serial_number_ = serial_number;
  }
  void set_seed_stores_succeed(bool value) { seed_stores_succeed_ = value; }
  bool fetch_attempted() const { return fetch_attempted_; }
  bool seed_stored() const { return seed_stored_; }
  const std::string& stored_country() const { return stored_country_; }
  const std::string& stored_geo_level() const { return stored_geo_level_; }
  bool delta_compressed_seed() const { return delta_compressed_seed_; }
  bool gzip_compressed_seed() const { return gzip_compressed_seed_; }
  bool runtime_simulation_called() const { return runtime_simulation_called_; }

  bool CallMaybeRetryOverHTTP() { return CallMaybeRetryOverHTTPForTesting(); }
  void SimulateAndApplyRuntimeMutableChanges(
      const VariationsSeed& seed) override {
    runtime_simulation_called_ = true;
    VariationsService::SimulateAndApplyRuntimeMutableChanges(seed);
  }

  const std::string& GetLatestSerialNumber() override {
    return latest_serial_number_;
  }

  void DoActualFetch() override {
    if (intercepts_fetch_) {
      fetch_attempted_ = true;
      return;
    }

    VariationsService::DoActualFetch();
    base::RunLoop().RunUntilIdle();
  }

  void DoFetchFromURL(const GURL& url,
                      std::string header_serial_number) override {
    last_header_serial_number_ = header_serial_number;
    if (intercepts_fetch_) {
      fetch_attempted_ = true;
      if (fetch_intercepted_callback_) {
        std::move(fetch_intercepted_callback_).Run();
      }
      return;
    }

    VariationsService::DoFetchFromURL(url, std::move(header_serial_number));
  }

  const std::string& last_header_serial_number() const {
    return last_header_serial_number_;
  }

  void set_fetch_intercepted_callback(base::OnceClosure callback) {
    fetch_intercepted_callback_ = std::move(callback);
  }

  void StoreSeed(std::string seed_data,
                 std::string seed_signature,
                 std::string country_code,
                 std::string geo_level1,
                 base::Time date_fetched,
                 bool is_delta_compressed,
                 bool is_gzip_compressed) override {
    seed_stored_ = true;
    stored_seed_data_ = seed_data;
    stored_country_ = country_code;
    stored_geo_level_ = geo_level1;
    delta_compressed_seed_ = is_delta_compressed;
    gzip_compressed_seed_ = is_gzip_compressed;
    OnSeedStoreResult(is_delta_compressed, seed_stores_succeed_,
                      VariationsSeed());
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
  bool intercepts_fetch_ = true;
  bool fetch_attempted_ = false;
  std::string latest_serial_number_;
  bool seed_stores_succeed_ = true;
  bool seed_stored_ = false;
  std::string stored_seed_data_;
  std::string stored_country_;
  std::string stored_geo_level_;
  bool delta_compressed_seed_ = false;
  bool gzip_compressed_seed_ = false;
  bool runtime_simulation_called_ = false;
  base::OnceClosure fetch_intercepted_callback_;
  std::string last_header_serial_number_;
};

class TestVariationsServiceObserver : public VariationsService::Observer {
 public:
  TestVariationsServiceObserver() = default;

  TestVariationsServiceObserver(const TestVariationsServiceObserver&) = delete;
  TestVariationsServiceObserver& operator=(
      const TestVariationsServiceObserver&) = delete;

  ~TestVariationsServiceObserver() override = default;

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

  int crticial_changes_notified() const { return crticial_changes_notified_; }

 private:
  // Number of notification received with BEST_EFFORT severity.
  int best_effort_changes_notified_ = 0;

  // Number of notification received with CRITICAL severity.
  int crticial_changes_notified_ = 0;
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

// Adds an OK response to the test_url_loader_factory with IM headers.
void AddOKResponseWithIM(
    const GURL& interception_url,
    const std::string& body,
    const std::string& im,
    network::TestURLLoaderFactory* test_url_loader_factory) {
  std::string headers("HTTP/1.1 200 OK\n\n");
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  if (!im.empty())
    head->headers->SetHeader("IM", im);
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = base::ByteSize(body.size());
  test_url_loader_factory->AddResponse(interception_url, std::move(head), body,
                                       status);
}

}  // namespace

BASE_FEATURE(kTestRegularFeature, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_RUNTIME_MUTABLE_FEATURE(kTestRuntimeFeatureA,
                             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_RUNTIME_MUTABLE_FEATURE(kTestRuntimeFeatureB,
                             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_RUNTIME_MUTABLE_FEATURE(kTestRuntimeFeatureC,
                             base::FEATURE_ENABLED_BY_DEFAULT);

class VariationsServiceTest : public ::testing::Test {
 public:
  VariationsServiceTest(const VariationsServiceTest&) = delete;
  VariationsServiceTest& operator=(const VariationsServiceTest&) = delete;

 protected:
  VariationsServiceTest()
      : network_tracker_(network::TestNetworkConnectionTracker::GetInstance()),
        enabled_state_provider_(
            new metrics::TestEnabledStateProvider(false, false)) {
    metrics::MetricsStateManager::RegisterPrefs(prefs_.registry());
    VariationsService::RegisterPrefs(prefs_.registry());
  }

  void TearDown() override {
    base::FeatureList::ClearFeatureCachedValueForTesting(kTestRuntimeFeatureA);
    base::FeatureList::ClearFeatureCachedValueForTesting(kTestRuntimeFeatureB);
    base::FeatureList::ClearFeatureCachedValueForTesting(kTestRuntimeFeatureC);
    base::RuntimeFieldTrialOverrides::GetInstance()->ResetForTesting();
  }

  metrics::MetricsStateManager* GetMetricsStateManager(
      metrics::StartupVisibility startup_visibility =
          metrics::StartupVisibility::kUnknown) {
    // Lazy-initialize the metrics_state_manager so that it correctly reads the
    // stability state from prefs after tests have a chance to initialize it.
    if (!metrics_state_manager_) {
      metrics_state_manager_ = metrics::MetricsStateManager::Create(
          &prefs_, enabled_state_provider_.get(), std::wstring(),
          base::FilePath(), startup_visibility);
      metrics_state_manager_->InstantiateFieldTrialList();
    }
    return metrics_state_manager_.get();
  }

 protected:
  TestingPrefServiceSimple prefs_;
  raw_ptr<network::TestNetworkConnectionTracker> network_tracker_;

 private:
  base::test::TaskEnvironment task_environment_;
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  std::unique_ptr<metrics::TestEnabledStateProvider> enabled_state_provider_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
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
      &prefs_, GetMetricsStateManager());
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
      &prefs_, GetMetricsStateManager());
  raw_client->set_channel(version_info::Channel::UNKNOWN);
  GURL url = service.GetVariationsServerURL(TestVariationsService::USE_HTTPS);

  // Corpus param should not be present by default.
  std::string corpus;
  EXPECT_FALSE(net::GetValueForKeyInQuery(url, "corpus", &corpus));

  std::string osname;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "osname", &osname));
  EXPECT_FALSE(osname.empty());

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

  // Corpus param should be present if set via command line.
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      variations::switches::kVariationsSeedCorpus, "test_corpus");
  url = service.GetVariationsServerURL(TestVariationsService::USE_HTTPS);
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "corpus", &corpus));
  EXPECT_EQ(corpus, "test_corpus");
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
  for (const net::HttpStatusCode code : non_ok_status_codes) {
    EXPECT_TRUE(prefs_.FindPreference(prefs::kVariationsCompressedSeed)
                    ->IsDefaultValue());
    service.test_url_loader_factory()->ClearResponses();
    service.test_url_loader_factory()->AddResponse(
        service.interception_url().spec(), "", code);
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

  EXPECT_THAT(intercepted_headers.GetHeader("A-IM"),
              ::testing::Optional(std::string("gzip")));
}

TEST_F(VariationsServiceTest, RequestDeltaCompressedSeed) {
  VariationsService::EnableFetchForTesting();

  std::string serialized_seed = SerializeSeed(CreateTestSeed());

  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);
  service.set_intercepts_fetch(false);
  net::HttpRequestHeaders intercepted_headers;
  service.test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        intercepted_headers = request.headers;
      }));

  // Set a serial number to allow delta compression.
  service.set_latest_serial_number("abc");

  // Prepare a delta response that fails to store.
  service.set_seed_stores_succeed(false);
  AddOKResponseWithIM(service.interception_url(), serialized_seed, "x-bm",
                      service.test_url_loader_factory());
  service.DoActualFetch();

  // Make sure the initial request was generated with correct delta headers.
  EXPECT_THAT(intercepted_headers.GetHeader("A-IM"),
              ::testing::Optional(std::string("x-bm,gzip")));
  EXPECT_THAT(intercepted_headers.GetHeader("If-None-Match"),
              ::testing::Optional(std::string("abc")));

  // Do a retry.
  service.set_seed_stores_succeed(true);
  AddOKResponseWithIM(service.interception_url(), serialized_seed, "",
                      service.test_url_loader_factory());
  service.DoActualFetch();

  // The retry request should not request delta compression.
  EXPECT_THAT(intercepted_headers.GetHeader("A-IM"),
              ::testing::Optional(std::string("gzip")));
  // It should still provide the serial number.
  EXPECT_THAT(intercepted_headers.GetHeader("If-None-Match"),
              ::testing::Optional(std::string("abc")));
}

TEST_F(VariationsServiceTest, InstanceManipulations) {
  struct {
    std::string im;
    bool delta_compressed;
    bool gzip_compressed;
    bool seed_stored;
  } cases[] = {
      {"", false, false, true},
      {"gzip", false, true, true},
      {"x-bm", true, false, true},
      {"x-bm,gzip", true, true, true},
      {" x-bm, gzip", true, true, true},
      {"gzip,x-bm", false, false, false},
      {"deflate,x-bm,gzip", false, false, false},
  };

  std::string serialized_seed = SerializeSeed(CreateTestSeed());
  VariationsService::EnableFetchForTesting();
  for (const auto& test_case : cases) {
    TestVariationsService service(
        std::make_unique<web_resource::TestRequestAllowedNotifier>(
            &prefs_, network_tracker_),
        &prefs_, GetMetricsStateManager(), true);
    service.set_intercepts_fetch(false);

    AddOKResponseWithIM(service.interception_url(), serialized_seed,
                        test_case.im, service.test_url_loader_factory());

    service.DoActualFetch();

    EXPECT_EQ(test_case.seed_stored, service.seed_stored());
    EXPECT_EQ(test_case.delta_compressed, service.delta_compressed_seed());
    EXPECT_EQ(test_case.gzip_compressed, service.gzip_compressed_seed());
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
  head->headers->SetHeader("X-Country", "test");
  head->headers->SetHeader("X-Geo-Level-1", "test-geo-level");
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = base::ByteSize(serialized_seed.size());
  service.test_url_loader_factory()->AddResponse(
      service.interception_url(), std::move(head), serialized_seed, status);

  service.DoActualFetch();

  EXPECT_TRUE(service.seed_stored());
  EXPECT_EQ("test", service.stored_country());
  EXPECT_EQ("test-geo-level", service.stored_geo_level());
}

TEST_F(VariationsServiceTest, CountryHeaderNotTrustedOverHTTP) {
  std::string serialized_seed = SerializeSeed(CreateTestSeed());
  VariationsService::EnableFetchForTesting();

  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), /*use_secure_url=*/false);
  EXPECT_FALSE(service.seed_stored());
  service.set_intercepts_fetch(false);

  std::string headers("HTTP/1.1 200 OK\n\n");
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  head->headers->SetHeader("X-Country", "test");
  head->headers->SetHeader("X-Geo-Level-1", "test-geo-level");
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = base::ByteSize(serialized_seed.size());
  service.test_url_loader_factory()->AddResponse(
      service.interception_url(), std::move(head), serialized_seed, status);

  service.set_last_request_was_retry(false);
  service.set_insecure_url(service.interception_url());
  EXPECT_TRUE(service.CallMaybeRetryOverHTTP());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service.seed_stored());
  EXPECT_TRUE(service.stored_country().empty());
  EXPECT_TRUE(service.stored_geo_level().empty());
}

TEST_F(VariationsServiceTest, Observer) {
  VariationsService service(
      std::make_unique<TestVariationsServiceClient>(),
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager());

  struct TestCase {
    int normal_count;
    int best_effort_count;
    int critical_count;
    int expected_best_effort_notifications;
    int expected_crtical_notifications;
  } cases[] = {
      {0, 0, 0, 0, 0},  {1, 0, 0, 0, 0}, {10, 0, 0, 0, 0}, {0, 1, 0, 1, 0},
      {0, 10, 0, 1, 0}, {0, 0, 1, 0, 1}, {0, 0, 10, 0, 1}, {0, 1, 1, 0, 1},
      {1, 1, 1, 0, 1},  {1, 1, 0, 1, 0}, {1, 0, 1, 0, 1},
  };

  for (const TestCase& test_case : cases) {
    TestVariationsServiceObserver observer;
    service.AddObserver(&observer);

    SeedSimulationResult result;
    result.normal_group_change_count = test_case.normal_count;
    result.kill_best_effort_group_change_count = test_case.best_effort_count;
    result.kill_critical_group_change_count = test_case.critical_count;
    service.NotifyObservers(result);

    EXPECT_EQ(test_case.expected_best_effort_notifications,
              observer.best_effort_changes_notified());
    EXPECT_EQ(test_case.expected_crtical_notifications,
              observer.crticial_changes_notified());

    service.RemoveObserver(&observer);
  }
}

TEST_F(VariationsServiceTest, GetStoredPermanentCountry) {
  struct {
    // The command line overridden country, empty if the
    // kVariationsOverrideCountry switch isn't passed in
    const std::string override_country;
    // The old overridden country, empty string if the pref isn't set initially.
    const std::string permanent_overridden_country_before;
    // Comma separated list, NULL if the pref isn't set initially.
    const std::string permanent_consistency_country_before;
    const std::string expected_country;
  } test_cases[] = {
      {"", "", "<VERSION>,us", "us"},
      {"", "us", "<VERSION>,us", "us"},
      {"", "ca", "<VERSION>,us", "ca"},
      {"", "ca", "", "ca"},
      {"gb", "", "<VERSION>,us", "gb"},
      {"gb", "us", "<VERSION>,us", "gb"},
      {"gb", "ca", "<VERSION>,us", "gb"},
      {"gb", "ca", "", "gb"},
  };

  for (const auto& test : test_cases) {
    TestVariationsService service(
        std::make_unique<web_resource::TestRequestAllowedNotifier>(
            &prefs_, network_tracker_),
        &prefs_, GetMetricsStateManager(), true);

    if (!test.override_country.empty()) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kVariationsOverrideCountry, test.override_country);
    }

    if (test.permanent_overridden_country_before.empty()) {
      prefs_.ClearPref(prefs::kVariationsPermanentOverriddenCountry);
    } else {
      prefs_.SetString(prefs::kVariationsPermanentOverriddenCountry,
                       test.permanent_overridden_country_before);
    }

    if (test.permanent_consistency_country_before.empty()) {
      prefs_.ClearPref(prefs::kVariationsPermanentConsistencyCountry);
    } else {
      std::string version_number(version_info::GetVersionNumber());
      base::ListValue list_value;
      for (const std::string& component :
           base::SplitString(test.permanent_consistency_country_before, ",",
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
        if (component == "<VERSION>") {
          // Replace version placeholder
          list_value.Append(version_number);
        } else {
          list_value.Append(component);
        }
      }
      prefs_.SetList(prefs::kVariationsPermanentConsistencyCountry,
                     std::move(list_value));
    }

    VariationsSeed seed(CreateTestSeed());
    // GetClientFilterableStateForVersion needs to be called before
    // service.GetStoredPermanentCountry can be used in tests.
    service.GetClientFilterableStateForVersion();

    EXPECT_EQ(test.expected_country, service.GetStoredPermanentCountry())
        << test.override_country << ", "
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
      {kPrefUs, "ca", kPrefCa, true},  {kPrefUs, "CA", kPrefCa, true},
      {kPrefUs, "us", kPrefUs, false}, {kPrefUs, "", "", true},
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

struct VariationsServiceSafeModeFetchTestCase {
  metrics::StartupVisibility visibility;
  int expected_streak;
};

class VariationsServiceSafeModeFetchTest
    : public VariationsServiceTest,
      public ::testing::WithParamInterface<
          VariationsServiceSafeModeFetchTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    VariationsServiceSafeModeFetchTest,
    ::testing::Values(
        VariationsServiceSafeModeFetchTestCase{
            .visibility = metrics::StartupVisibility::kUnknown,
            .expected_streak = 2},
        VariationsServiceSafeModeFetchTestCase{
            .visibility = metrics::StartupVisibility::kForeground,
            .expected_streak = 2},
        VariationsServiceSafeModeFetchTestCase{
            .visibility = metrics::StartupVisibility::kBackground,
            .expected_streak = 1}));

TEST_P(VariationsServiceSafeModeFetchTest, RecordFetchStarted) {
  const VariationsServiceSafeModeFetchTestCase& test_case = GetParam();
  prefs_.SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 1);
  VariationsService::EnableFetchForTesting();

  // Create a variations service with the given visibility and start the fetch.
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(test_case.visibility), true);
  service.set_intercepts_fetch(false);
  service.DoActualFetch();

  EXPECT_EQ(test_case.expected_streak,
            prefs_.GetInteger(prefs::kVariationsFailedToFetchSeedStreak));
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

  std::string headers("HTTP/1.1 200 OK\n\n");
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  head->headers->SetHeader("X-Seed-Signature", kBase64SeedSignature);
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = base::ByteSize(response.size());
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
  service.GetClientFilterableStateForVersion();
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

TEST_F(VariationsServiceTest, RetryOverHTTPWithBackgroundEncryption) {
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);
  service.set_intercepts_fetch(true);
  service.set_last_request_was_retry(false);
  service.set_insecure_url(GURL("http://example.test"));
  service.set_latest_serial_number("test_serial_number");

  base::RunLoop run_loop;
  service.set_fetch_intercepted_callback(run_loop.QuitClosure());

  // The retry should be initiated, but the actual fetch is delayed
  // because encryption happens on a background thread.
  EXPECT_TRUE(service.CallMaybeRetryOverHTTP());
  EXPECT_FALSE(service.fetch_attempted());

  // Run the loop until the background task and reply callback execute.
  run_loop.Run();

  EXPECT_TRUE(service.fetch_attempted());
  EXPECT_NE(service.last_header_serial_number(), "test_serial_number");
  std::string decoded;
  EXPECT_TRUE(
      base::Base64Decode(service.last_header_serial_number(), &decoded));
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

TEST_F(VariationsServiceTest, SeedStoredWhenRedirected) {
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
  EXPECT_TRUE(service.seed_stored());
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

  std::string headers("HTTP/1.1 200 OK\n\n");
  auto head = network::mojom::URLResponseHead::New();
  auto http_response_headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  head->headers = http_response_headers;
  EXPECT_EQ(net::HTTP_OK, http_response_headers->response_code());
  http_response_headers->SetHeader("X-Seed-Signature", kBase64SeedSignature);
  // Set ERR_FAILED status code despite the 200 response code.
  network::URLLoaderCompletionStatus status(net::ERR_FAILED);
  status.decoded_body_length = base::ByteSize(response.size());
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

TEST_F(VariationsServiceTest, VariationsServiceStartsRequestOnNetworkChange) {
  // Verifies VariationsService does a request when network status changes from
  // none to connected. This is a regression test for https://crbug.com/826930.
  VariationsService::EnableFetchForTesting();
  network_tracker_->SetConnectionType(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE);
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
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);
  base::RunLoop().RunUntilIdle();

  const int final_request_count = service.request_count();
  // The request will be made once Chrome gets online.
  EXPECT_EQ(initial_request_count + 1, final_request_count);
}

VariationsSeed CreateTestRuntimeMutableSeed(
    const std::string& study_name,
    const std::string& experiment_name,
    const std::vector<std::string>& enable_features,
    const std::vector<std::string>& disable_features,
    const std::string& default_experiment_name = "",
    int probability_weight = 100) {
  VariationsSeed seed;
  Study* study = seed.add_study();
  study->set_name(study_name);
  study->set_default_experiment_name(default_experiment_name.empty()
                                         ? experiment_name
                                         : default_experiment_name);
  study->set_runtime_mutable(true);
  study->set_activation_type(Study::ACTIVATE_ON_STARTUP);
  study->set_consistency(Study::PERMANENT);
  Study_Experiment* experiment = study->add_experiment();
  experiment->set_name(experiment_name);
  experiment->set_probability_weight(probability_weight);

  Study_Experiment_FeatureAssociation* feature_association =
      experiment->mutable_feature_association();
  for (const std::string& feature : enable_features) {
    feature_association->add_enable_feature(feature);
  }
  for (const std::string& feature : disable_features) {
    feature_association->add_disable_feature(feature);
  }

  return seed;
}

// Verifies that SimulateAndApplyRuntimeMutableChanges is called when a seed is
// stored successfully.
TEST_F(VariationsServiceTest,
       SimulateAndApplyRuntimeMutableChanges_CalledOnSeedStore) {
  VariationsService::EnableFetchForTesting();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kVariationsRuntimeMutability);

  // A successful seed store should trigger a runtime mutation simulation.
  {
    TestVariationsService service(
        std::make_unique<web_resource::TestRequestAllowedNotifier>(
            &prefs_, network_tracker_),
        &prefs_, GetMetricsStateManager(), true);

    EXPECT_FALSE(service.seed_stored());
    EXPECT_FALSE(service.runtime_simulation_called());

    service.test_url_loader_factory()->AddResponse(
        service.interception_url().spec(), SerializeSeed(CreateTestSeed()));
    service.set_intercepts_fetch(false);
    service.DoActualFetch();

    EXPECT_TRUE(service.seed_stored());
    EXPECT_TRUE(service.runtime_simulation_called());
  }

  // But not a failed store.
  {
    TestVariationsService service(
        std::make_unique<web_resource::TestRequestAllowedNotifier>(
            &prefs_, network_tracker_),
        &prefs_, GetMetricsStateManager(), true);

    EXPECT_FALSE(service.seed_stored());
    EXPECT_FALSE(service.runtime_simulation_called());

    service.set_seed_stores_succeed(false);
    service.test_url_loader_factory()->AddResponse(
        service.interception_url().spec(), SerializeSeed(CreateTestSeed()));
    service.set_intercepts_fetch(false);
    service.DoActualFetch();

    EXPECT_FALSE(service.runtime_simulation_called());
  }
}

// Verifies that simulation is not applied to non-runtime mutable configs.
TEST_F(VariationsServiceTest,
       ApplyRuntimeMutableChanges_NonRuntimeMutableConfig) {
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);

  base::test::ScopedFeatureList scoped_feature_list;
  auto feature_list = std::make_unique<base::FeatureList>();
  feature_list->EnableRuntimeMutability(
      kTestRuntimeFeatureA,
      base::FeatureList::OnRuntimeMutableFeatureStateChangedCallback());
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  {
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "MyStudy", "Group1", {},
        {kTestRuntimeFeatureA.name, kTestRegularFeature.name});
    seed.mutable_study(0)->set_runtime_mutable(false);
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectTotalCount(kApplyRuntimeMutableChangesResultMetric,
                                      0);
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_FALSE(base::RuntimeFieldTrialOverrides::GetInstance()
                     ->GetRuntimeOverride("MyStudy")
                     .has_value());
  }
}

// Verifies that null simulation results are not applied.
TEST_F(VariationsServiceTest, ApplyRuntimeMutableChanges_NotNull) {
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);

  base::test::ScopedFeatureList scoped_feature_list;
  auto feature_list = std::make_unique<base::FeatureList>();
  feature_list->EnableRuntimeMutability(
      kTestRuntimeFeatureA,
      base::FeatureList::OnRuntimeMutableFeatureStateChangedCallback());
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  {
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "MyStudy", "Group1", {}, {kTestRuntimeFeatureA.name},
        /*default_experiment_name=*/"", /*probability_weight=*/0);
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kSimulatedGroupIsNull, 1);
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_FALSE(base::RuntimeFieldTrialOverrides::GetInstance()
                     ->GetRuntimeOverride("MyStudy")
                     .has_value());
  }
}

// Verifies that only killswitches can be applied.
TEST_F(VariationsServiceTest, ApplyRuntimeMutableChanges_StrictKillswitch) {
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);

  base::test::ScopedFeatureList scoped_feature_list;
  auto feature_list = std::make_unique<base::FeatureList>();
  feature_list->EnableRuntimeMutability(
      kTestRuntimeFeatureA,
      base::FeatureList::OnRuntimeMutableFeatureStateChangedCallback());
  feature_list->EnableRuntimeMutability(
      kTestRuntimeFeatureB,
      base::FeatureList::OnRuntimeMutableFeatureStateChangedCallback());
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  {
    // Case 1: Only specifies features to enable.
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "MyStudy", "Group1", {kTestRuntimeFeatureA.name}, {});
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kNotStrictKillswitch, 1);
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_FALSE(base::RuntimeFieldTrialOverrides::GetInstance()
                     ->GetRuntimeOverride("MyStudy")
                     .has_value());
  }

  {
    // Case 2: Specifies a mix of features to enable and disable.
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "MyStudy", "Group1", {kTestRuntimeFeatureA.name},
        {kTestRuntimeFeatureB.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kNotStrictKillswitch, 1);
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureB));
    EXPECT_FALSE(base::RuntimeFieldTrialOverrides::GetInstance()
                     ->GetRuntimeOverride("MyStudy")
                     .has_value());
  }

  {
    // Case 3: Specifies no features.
    base::HistogramTester histogram_tester;
    VariationsSeed seed =
        CreateTestRuntimeMutableSeed("MyStudy", "Group1", {}, {});
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kNotStrictKillswitch, 1);
    EXPECT_FALSE(base::RuntimeFieldTrialOverrides::GetInstance()
                     ->GetRuntimeOverride("MyStudy")
                     .has_value());
  }

  {
    // Case 4: Specifies a feature to disable. This should work.
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "MyStudy", "Group1", {}, {kTestRuntimeFeatureA.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kSuccess, 1);
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    auto override =
        base::RuntimeFieldTrialOverrides::GetInstance()->GetRuntimeOverride(
            "MyStudy");
    ASSERT_TRUE(override.has_value());
    EXPECT_EQ(override->group_name, "Group1");
    // This is not overriding any specific trial since the feature was simply
    // ENABLED_BY_DEFAULT and not controlled by any field trial.
    EXPECT_FALSE(override->overridden_trial);
  }
}

// Verifies that only starts_active killswitches can be applied.
TEST_F(VariationsServiceTest, ApplyRuntimeMutableChanges_NotStartsActive) {
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);

  base::test::ScopedFeatureList scoped_feature_list;
  auto feature_list = std::make_unique<base::FeatureList>();
  feature_list->EnableRuntimeMutability(
      kTestRuntimeFeatureA,
      base::FeatureList::OnRuntimeMutableFeatureStateChangedCallback());
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  {
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "MyStudy", "Group1", {}, {kTestRuntimeFeatureA.name});
    seed.mutable_study(0)->set_activation_type(Study::ACTIVATE_ON_QUERY);
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kNotStartsActive, 1);
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_FALSE(base::RuntimeFieldTrialOverrides::GetInstance()
                     ->GetRuntimeOverride("MyStudy")
                     .has_value());
  }
}

// Verifies that only session consistency studies cannot be applied.
TEST_F(VariationsServiceTest,
       ApplyRuntimeMutableChanges_NotPermanentConsistency) {
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);

  base::test::ScopedFeatureList scoped_feature_list;
  auto feature_list = std::make_unique<base::FeatureList>();
  feature_list->EnableRuntimeMutability(
      kTestRuntimeFeatureA,
      base::FeatureList::OnRuntimeMutableFeatureStateChangedCallback());
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  {
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "MyStudy", "Group1", {}, {kTestRuntimeFeatureA.name});
    seed.mutable_study(0)->set_consistency(Study::SESSION);
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kNotPermanentConsistency, 1);
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_FALSE(base::RuntimeFieldTrialOverrides::GetInstance()
                     ->GetRuntimeOverride("MyStudy")
                     .has_value());
  }
}

// Verifies the following scenario:
// - Initially, kTestRuntimeFeatureA is disabled by Killswitch/Disabled50.
// - A killswitch config "Killswitch/Disabled50" is received to killswitch the
//   feature -- it should not apply because that config was already applied.
// - A killswitch config "Killswitch/Disabled100" is received to killswitch the
//   feature -- it should apply successfully.
// - A killswitch config "Killswitch/Disabled100" is received to killswitch the
//   feature -- it should not apply because that config was already applied.
// - A killswitch config "Killswitch/Disabled50" is received to killswitch the
//   feature -- it should apply successfully (despite the original trial being
//   also "Killswitch/Disabled50").
TEST_F(VariationsServiceTest, ApplyRuntimeMutableChanges_AlreadyApplied) {
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);

  base::test::ScopedFeatureList scoped_feature_list;
  auto feature_list = std::make_unique<base::FeatureList>();
  feature_list->EnableRuntimeMutability(
      kTestRuntimeFeatureA,
      base::FeatureList::OnRuntimeMutableFeatureStateChangedCallback());
  base::FieldTrial* trial =
      base::FieldTrialList::CreateFieldTrial("Killswitch", "Disabled50");
  feature_list->RegisterFieldTrialOverride(
      kTestRuntimeFeatureA.name, base::FeatureList::OVERRIDE_DISABLE_FEATURE,
      trial);
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  {
    // Try applying Killswitch/Disabled50 -- should not apply because it was
    // already applied.
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "Killswitch", "Disabled50", {}, {kTestRuntimeFeatureA.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kAlreadyApplied, 1);
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_FALSE(base::RuntimeFieldTrialOverrides::GetInstance()
                     ->GetRuntimeOverride("Killswitch")
                     .has_value());
  }

  {
    // Try applying Killswitch/Disabled100 -- should apply successfully (even
    // though the feature is already disabled).
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "Killswitch", "Disabled100", {}, {kTestRuntimeFeatureA.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kSuccess, 1);
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    auto override =
        base::RuntimeFieldTrialOverrides::GetInstance()->GetRuntimeOverride(
            "Killswitch");
    ASSERT_TRUE(override.has_value());
    EXPECT_EQ(override->group_name, "Disabled100");
    EXPECT_EQ(override->overridden_trial, trial);
  }

  {
    // Try applying Killswitch/Disabled100 again -- should not apply because it
    // was already applied.
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "Killswitch", "Disabled100", {}, {kTestRuntimeFeatureA.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kAlreadyApplied, 1);
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    // Override should be unchanged.
    auto override =
        base::RuntimeFieldTrialOverrides::GetInstance()->GetRuntimeOverride(
            "Killswitch");
    ASSERT_TRUE(override.has_value());
    EXPECT_EQ(override->group_name, "Disabled100");
    EXPECT_EQ(override->overridden_trial, trial);
  }

  {
    // Try applying Killswitch/Disabled50 -- should apply successfully (even
    // though the feature is already disabled). Note also that the original
    // trial was also Killswitch/Disabled50, but it was overridden, so it is not
    // considered "currently applied" anymore.
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "Killswitch", "Disabled50", {}, {kTestRuntimeFeatureA.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kSuccess, 1);
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    auto override =
        base::RuntimeFieldTrialOverrides::GetInstance()->GetRuntimeOverride(
            "Killswitch");
    ASSERT_TRUE(override.has_value());
    EXPECT_EQ(override->group_name, "Disabled50");
    EXPECT_EQ(override->overridden_trial, trial);
  }
}

// Verifies that runtime experiments with Google web experiment IDs are not
// applied.
TEST_F(VariationsServiceTest,
       ApplyRuntimeMutableChanges_RuntimeExperimentHasGoogleWebId) {
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);

  base::test::ScopedFeatureList scoped_feature_list;
  auto feature_list = std::make_unique<base::FeatureList>();
  feature_list->EnableRuntimeMutability(
      kTestRuntimeFeatureA,
      base::FeatureList::OnRuntimeMutableFeatureStateChangedCallback());
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  {
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "MyStudy", "Group1", {}, {kTestRuntimeFeatureA.name});
    seed.mutable_study(0)->mutable_experiment(0)->set_google_web_experiment_id(
        12345);
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kRuntimeExperimentHasGoogleWebId, 1);
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_FALSE(base::RuntimeFieldTrialOverrides::GetInstance()
                     ->GetRuntimeOverride("MyStudy")
                     .has_value());
  }

  {
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "MyStudy", "Group1", {}, {kTestRuntimeFeatureA.name});
    seed.mutable_study(0)
        ->mutable_experiment(0)
        ->set_google_web_trigger_experiment_id(12345);
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kRuntimeExperimentHasGoogleWebId, 1);
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_FALSE(base::RuntimeFieldTrialOverrides::GetInstance()
                     ->GetRuntimeOverride("MyStudy")
                     .has_value());
  }
}

// Verifies that overriding a trial with Google web experiment IDs is not
// allowed (even if the variation ID was set on an unselected group in that
// trial).
TEST_F(VariationsServiceTest,
       ApplyRuntimeMutableChanges_OverriddenTrialHasGoogleWebId) {
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);

  base::test::ScopedFeatureList scoped_feature_list;
  auto feature_list = std::make_unique<base::FeatureList>();
  feature_list->EnableRuntimeMutability(
      kTestRuntimeFeatureA,
      base::FeatureList::OnRuntimeMutableFeatureStateChangedCallback());
  base::FieldTrial* trial =
      base::FieldTrialList::CreateFieldTrial("MyTrial", "Group1");
  feature_list->RegisterFieldTrialOverride(
      kTestRuntimeFeatureA.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      trial);
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  // Associate a Google web VariationID with an unselected group in "MyTrial".
  AssociateGoogleVariationIDForTesting(GOOGLE_WEB_PROPERTIES_ANY_CONTEXT,
                                       "MyTrial", "UnselectedGroup", 12345);

  {
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "MyStudy", "Group1", {}, {kTestRuntimeFeatureA.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kOverriddenTrialHasGoogleWebId, 1);
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_FALSE(base::RuntimeFieldTrialOverrides::GetInstance()
                     ->GetRuntimeOverride("MyStudy")
                     .has_value());
  }
}

// Verifies that non-runtime mutable features should not work.
TEST_F(VariationsServiceTest,
       ApplyRuntimeMutableChanges_NonRuntimeMutableFeature) {
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);

  base::test::ScopedFeatureList scoped_feature_list;
  auto feature_list = std::make_unique<base::FeatureList>();
  feature_list->EnableRuntimeMutability(
      kTestRuntimeFeatureA,
      base::FeatureList::OnRuntimeMutableFeatureStateChangedCallback());
  feature_list->EnableRuntimeMutability(
      kTestRuntimeFeatureB,
      base::FeatureList::OnRuntimeMutableFeatureStateChangedCallback());
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  {
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "MyStudy", "Group1", {},
        {kTestRuntimeFeatureA.name, kTestRegularFeature.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kNonRuntimeMutableFeature, 1);
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRegularFeature));
    EXPECT_FALSE(base::RuntimeFieldTrialOverrides::GetInstance()
                     ->GetRuntimeOverride("MyStudy")
                     .has_value());
  }
}

// Verifies that if a feature was overridden by a command line flag, runtime
// mutable configs won't be applied.
TEST_F(VariationsServiceTest,
       ApplyRuntimeMutableChanges_FeatureOverriddenFromCommandLine) {
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);

  base::test::ScopedFeatureList scoped_feature_list;
  auto feature_list = std::make_unique<base::FeatureList>();
  feature_list->EnableRuntimeMutability(
      kTestRuntimeFeatureA,
      base::FeatureList::OnRuntimeMutableFeatureStateChangedCallback());
  feature_list->EnableRuntimeMutability(
      kTestRuntimeFeatureB,
      base::FeatureList::OnRuntimeMutableFeatureStateChangedCallback());
  feature_list->InitFromCommandLine(kTestRuntimeFeatureB.name, "");
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  {
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "MyStudy", "Group1", {},
        {kTestRuntimeFeatureA.name, kTestRuntimeFeatureB.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kFeatureOverriddenFromCommandLine, 1);
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureB));
    EXPECT_FALSE(base::RuntimeFieldTrialOverrides::GetInstance()
                     ->GetRuntimeOverride("MyStudy")
                     .has_value());
  }
}

// Verifies the following scenario:
// 1. kTestRuntimeFeatureA is enabled by Trial1, kTestRuntimeFeatureB is enabled
//    by Trial2.
// 2. A killswitch config is received to killswitch both features -- it should
//    not apply because they are controlled by different trials.
// 3. A killswitch config is received to killswitch only kTestRuntimeFeatureA --
//    it should apply successfully.
// 4. A killswitch config is received to killswitch both features -- it should
//    not apply because they are controlled by different trials.
// 5. A killswitch config is received to killswitch only kTestRuntimeFeatureB --
//    it should apply successfully.
// 6. A killswitch config is received to killswitch both features -- it should
//    not apply because they are controlled by different trials.
TEST_F(VariationsServiceTest,
       ApplyRuntimeMutableChanges_FeaturesControlledByDifferentTrials) {
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);

  base::test::ScopedFeatureList scoped_feature_list;
  auto feature_list = std::make_unique<base::FeatureList>();
  feature_list->EnableRuntimeMutability(
      kTestRuntimeFeatureA,
      base::FeatureList::OnRuntimeMutableFeatureStateChangedCallback());
  feature_list->EnableRuntimeMutability(
      kTestRuntimeFeatureB,
      base::FeatureList::OnRuntimeMutableFeatureStateChangedCallback());
  base::FieldTrial* trial1 =
      base::FieldTrialList::CreateFieldTrial("Trial1", "Group1");
  base::FieldTrial* trial2 =
      base::FieldTrialList::CreateFieldTrial("Trial2", "Group1");
  feature_list->RegisterFieldTrialOverride(
      kTestRuntimeFeatureA.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      trial1);
  feature_list->RegisterFieldTrialOverride(
      kTestRuntimeFeatureB.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      trial2);
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  {
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "KillswitchAAndB", "Disabled", {},
        {kTestRuntimeFeatureA.name, kTestRuntimeFeatureB.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);

    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kFeaturesNotControlledBySameTrial, 1);
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureB));
    EXPECT_FALSE(base::RuntimeFieldTrialOverrides::GetInstance()
                     ->GetRuntimeOverride("KillswitchAAndB")
                     .has_value());
  }

  {
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "KillswitchA", "Disabled", {}, {kTestRuntimeFeatureA.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);

    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kSuccess, 1);
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureB));
    auto override =
        base::RuntimeFieldTrialOverrides::GetInstance()->GetRuntimeOverride(
            "KillswitchA");
    ASSERT_TRUE(override.has_value());
    EXPECT_EQ(override->group_name, "Disabled");
    EXPECT_EQ(override->overridden_trial, trial1);
  }

  {
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "KillswitchAAndB", "Disabled", {},
        {kTestRuntimeFeatureA.name, kTestRuntimeFeatureB.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);

    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kFeaturesNotControlledBySameTrial, 1);
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureB));
    EXPECT_FALSE(base::RuntimeFieldTrialOverrides::GetInstance()
                     ->GetRuntimeOverride("KillswitchAAndB")
                     .has_value());
  }

  {
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "KillswitchB", "Disabled", {}, {kTestRuntimeFeatureB.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);

    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kSuccess, 1);
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureB));
    auto override =
        base::RuntimeFieldTrialOverrides::GetInstance()->GetRuntimeOverride(
            "KillswitchB");
    ASSERT_TRUE(override.has_value());
    EXPECT_EQ(override->group_name, "Disabled");
    EXPECT_EQ(override->overridden_trial, trial2);
  }

  {
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "KillswitchAAndB", "Disabled", {},
        {kTestRuntimeFeatureA.name, kTestRuntimeFeatureB.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);

    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kFeaturesNotControlledBySameTrial, 1);
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureB));
    EXPECT_FALSE(base::RuntimeFieldTrialOverrides::GetInstance()
                     ->GetRuntimeOverride("KillswitchAAndB")
                     .has_value());
  }
}

// Verifies the following scenario:
// 1. kTestRuntimeFeatureA is enabled by Trial1, kTestRuntimeFeatureB is not
//    enabled by any trial (but it is ENABLED_BY_DEFAULT).
// 2. A killswitch config is received to killswitch both features -- it should
//    not apply because they are controlled by different trials.
// 3. A killswitch config is received to killswitch only kTestRuntimeFeatureA --
//    it should apply successfully.
// 4. A killswitch config is received to killswitch both features -- it should
//    not apply because they are controlled by different trials.
TEST_F(VariationsServiceTest,
       ApplyRuntimeMutableChanges_OneFeatureControlledByTrial) {
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);

  base::test::ScopedFeatureList scoped_feature_list;
  auto feature_list = std::make_unique<base::FeatureList>();
  feature_list->EnableRuntimeMutability(
      kTestRuntimeFeatureA,
      base::FeatureList::OnRuntimeMutableFeatureStateChangedCallback());
  feature_list->EnableRuntimeMutability(
      kTestRuntimeFeatureB,
      base::FeatureList::OnRuntimeMutableFeatureStateChangedCallback());
  base::FieldTrial* trial =
      base::FieldTrialList::CreateFieldTrial("Trial1", "Group1");
  feature_list->RegisterFieldTrialOverride(
      kTestRuntimeFeatureA.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      trial);
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  {
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "KillswitchAAndB", "Disabled", {},
        {kTestRuntimeFeatureA.name, kTestRuntimeFeatureB.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);

    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kFeaturesNotControlledBySameTrial, 1);
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureB));
    EXPECT_FALSE(base::RuntimeFieldTrialOverrides::GetInstance()
                     ->GetRuntimeOverride("KillswitchAAndB")
                     .has_value());
  }

  {
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "KillswitchA", "Disabled", {}, {kTestRuntimeFeatureA.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);

    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kSuccess, 1);
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureB));
    auto override =
        base::RuntimeFieldTrialOverrides::GetInstance()->GetRuntimeOverride(
            "KillswitchA");
    ASSERT_TRUE(override.has_value());
    EXPECT_EQ(override->group_name, "Disabled");
    EXPECT_EQ(override->overridden_trial, trial);
  }

  {
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "KillswitchAAndB", "Disabled", {},
        {kTestRuntimeFeatureA.name, kTestRuntimeFeatureB.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);

    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kFeaturesNotControlledBySameTrial, 1);
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureB));
    EXPECT_FALSE(base::RuntimeFieldTrialOverrides::GetInstance()
                     ->GetRuntimeOverride("KillswitchAAndB")
                     .has_value());
  }
}

// Verifies that if a killswitch config killswitches FeatureA, but the trial
// controlling FeatureA also controls other features, then the killswitch is not
// applied.
TEST_F(VariationsServiceTest,
       ApplyRuntimeMutableChanges_ControllingTrialHasOtherFeatures) {
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);

  base::test::ScopedFeatureList scoped_feature_list;
  auto feature_list = std::make_unique<base::FeatureList>();
  feature_list->EnableRuntimeMutability(
      kTestRuntimeFeatureA,
      base::FeatureList::OnRuntimeMutableFeatureStateChangedCallback());
  feature_list->EnableRuntimeMutability(
      kTestRuntimeFeatureB,
      base::FeatureList::OnRuntimeMutableFeatureStateChangedCallback());
  base::FieldTrial* trial =
      base::FieldTrialList::CreateFieldTrial("Trial1", "Group1");
  feature_list->RegisterFieldTrialOverride(
      kTestRuntimeFeatureA.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      trial);
  feature_list->RegisterFieldTrialOverride(
      kTestRuntimeFeatureB.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      trial);
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  {
    // Killswitch only A -- should not apply since the trial for A ("Trial1")
    // also controls B.
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "KillswitchA", "Disabled", {}, {kTestRuntimeFeatureA.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kControllingTrialHasOtherFeatures, 1);
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureB));
    EXPECT_FALSE(base::RuntimeFieldTrialOverrides::GetInstance()
                     ->GetRuntimeOverride("KillswitchA")
                     .has_value());
  }

  {
    // Killswitch both A and B -- should apply.
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "KillswitchAAndB", "Disabled", {},
        {kTestRuntimeFeatureB.name, kTestRuntimeFeatureA.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kSuccess, 1);
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureB));
    auto override =
        base::RuntimeFieldTrialOverrides::GetInstance()->GetRuntimeOverride(
            "KillswitchAAndB");
    ASSERT_TRUE(override.has_value());
    EXPECT_EQ(override->group_name, "Disabled");
    EXPECT_EQ(override->overridden_trial, trial);
  }

  {
    // Try kill switching only A again -- should not apply since the override
    // trial for A ("KillswitchAAndB") also controls B.
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "KillswitchA", "Disable", {}, {kTestRuntimeFeatureA.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kControllingTrialHasOtherFeatures, 1);
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureB));
    EXPECT_FALSE(base::RuntimeFieldTrialOverrides::GetInstance()
                     ->GetRuntimeOverride("KillswitchA")
                     .has_value());
    // Previous override is still active.
    auto override =
        base::RuntimeFieldTrialOverrides::GetInstance()->GetRuntimeOverride(
            "KillswitchAAndB");
    ASSERT_TRUE(override.has_value());
    EXPECT_EQ(override->group_name, "Disabled");
    EXPECT_EQ(override->overridden_trial, trial);
  }
}

// Verifies that checking associated features for a controlling trial does not
// crash when FeatureList overrides contains features with null field trials
// (e.g., features overridden from command line or via extra feature overrides).
// This was a previously buggy behaviour.
TEST_F(VariationsServiceTest,
       ApplyRuntimeMutableChanges_NullFieldTrialInOverrides) {
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);
  base::test::ScopedFeatureList scoped_feature_list;
  auto feature_list = std::make_unique<base::FeatureList>();
  // Register an extra feature override without an associated FieldTrial
  // (`field_trial` pointer inside `overrides_` will be null).
  feature_list->RegisterExtraFeatureOverrides(
      {{kTestRegularFeature, base::FeatureList::OVERRIDE_DISABLE_FEATURE}});
  // Enable runtime mutability for `kTestRuntimeFeatureA` controlled by
  // "Trial1".
  feature_list->EnableRuntimeMutability(
      kTestRuntimeFeatureA,
      base::FeatureList::OnRuntimeMutableFeatureStateChangedCallback());
  base::FieldTrial* trial =
      base::FieldTrialList::CreateFieldTrial("Trial1", "Group1");
  feature_list->RegisterFieldTrialOverride(
      kTestRuntimeFeatureA.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      trial);
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  {
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "KillswitchA", "Disabled", {}, {kTestRuntimeFeatureA.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kSuccess, 1);
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    auto override_info =
        base::RuntimeFieldTrialOverrides::GetInstance()->GetRuntimeOverride(
            "KillswitchA");
    ASSERT_TRUE(override_info.has_value());
    EXPECT_EQ(override_info->group_name, "Disabled");
    EXPECT_EQ(override_info->overridden_trial, trial);
  }
}

// Verifies that if a killswitch config disables features that are all not
// associated with any trial, then the killswitch can be applied. However, from
// then on, only configs that disable the same set of features will be applied.
TEST_F(VariationsServiceTest, ApplyRuntimeMutableChanges_FeaturesWithNoTrials) {
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);

  base::test::ScopedFeatureList scoped_feature_list;
  auto feature_list = std::make_unique<base::FeatureList>();
  feature_list->EnableRuntimeMutability(
      kTestRuntimeFeatureA,
      base::FeatureList::OnRuntimeMutableFeatureStateChangedCallback());
  feature_list->EnableRuntimeMutability(
      kTestRuntimeFeatureB,
      base::FeatureList::OnRuntimeMutableFeatureStateChangedCallback());
  feature_list->EnableRuntimeMutability(
      kTestRuntimeFeatureC,
      base::FeatureList::OnRuntimeMutableFeatureStateChangedCallback());
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  {
    // Killswitch A and B -- should apply since they are both not controlled by
    // any trial.
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "KillswitchAAndB", "Disabled50", {},
        {kTestRuntimeFeatureA.name, kTestRuntimeFeatureB.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kSuccess, 1);
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureB));
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureC));
    auto override =
        base::RuntimeFieldTrialOverrides::GetInstance()->GetRuntimeOverride(
            "KillswitchAAndB");
    ASSERT_TRUE(override.has_value());
    EXPECT_EQ(override->group_name, "Disabled50");
    EXPECT_EQ(override->overridden_trial, nullptr);
  }

  {
    // Killswitch A, B, and C. Had this been the first killswitch config
    // received, it would have been applied. But since the previous config
    // (killswitch A and B) was already applied, this killswitch is rejected
    // as not all features are controlled by the same trial (A and B are
    // controlled by KillswitchAAndB, but C is not).
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "KillswitchABC", "Disabled", {},
        {kTestRuntimeFeatureA.name, kTestRuntimeFeatureB.name,
         kTestRuntimeFeatureC.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kFeaturesNotControlledBySameTrial, 1);
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureB));
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureC));
    EXPECT_FALSE(base::RuntimeFieldTrialOverrides::GetInstance()
                     ->GetRuntimeOverride("KillswitchABC")
                     .has_value());
    // Previous override is still active.
    auto override =
        base::RuntimeFieldTrialOverrides::GetInstance()->GetRuntimeOverride(
            "KillswitchAAndB");
    ASSERT_TRUE(override.has_value());
    EXPECT_EQ(override->group_name, "Disabled50");
    EXPECT_EQ(override->overridden_trial, nullptr);
  }

  {
    // Killswitch only A. Again, had this been the first killswitch config
    // received, it would have been applied. But since the previous config
    // (killswitch A and B) was already applied, this killswitch is rejected
    // as the controlling override trial (KillswitchAAndB) also controls
    // FeatureB.
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "KillswitchA", "DisableA", {}, {kTestRuntimeFeatureA.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kControllingTrialHasOtherFeatures, 1);
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureB));
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureC));
    EXPECT_FALSE(base::RuntimeFieldTrialOverrides::GetInstance()
                     ->GetRuntimeOverride("KillswitchA")
                     .has_value());
    // Previous override is still active.
    auto override =
        base::RuntimeFieldTrialOverrides::GetInstance()->GetRuntimeOverride(
            "KillswitchAAndB");
    ASSERT_TRUE(override.has_value());
    EXPECT_EQ(override->group_name, "Disabled50");
    EXPECT_EQ(override->overridden_trial, nullptr);
  }

  {
    // A new killswitch config is received that only disables features A and B.
    // This should be applied successfully as it's the same set of features.
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "KillswitchAAndB", "Disabled100", {},
        {kTestRuntimeFeatureA.name, kTestRuntimeFeatureB.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kSuccess, 1);
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureB));
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureC));
    auto override =
        base::RuntimeFieldTrialOverrides::GetInstance()->GetRuntimeOverride(
            "KillswitchAAndB");
    ASSERT_TRUE(override.has_value());
    EXPECT_EQ(override->group_name, "Disabled100");
    EXPECT_EQ(override->overridden_trial, nullptr);
  }
}

// Verifies that a killswitch config that would result in a trial name collision
// (either with an existing FieldTrial or a runtime mutable trial) is not
// applied. However, if that colliding trial is about to be overridden by the
// killswitch, then the killswitch can be applied.
TEST_F(VariationsServiceTest, ApplyRuntimeMutableChanges_TrialNameCollision) {
  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);

  base::test::ScopedFeatureList scoped_feature_list;
  auto feature_list = std::make_unique<base::FeatureList>();
  feature_list->EnableRuntimeMutability(
      kTestRuntimeFeatureA,
      base::FeatureList::OnRuntimeMutableFeatureStateChangedCallback());
  feature_list->EnableRuntimeMutability(
      kTestRuntimeFeatureB,
      base::FeatureList::OnRuntimeMutableFeatureStateChangedCallback());
  base::FieldTrial* trial1 =
      base::FieldTrialList::CreateFieldTrial("Trial1", "Group1");
  feature_list->RegisterFieldTrialOverride(
      kTestRuntimeFeatureA.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      trial1);
  base::FieldTrial* trial2 =
      base::FieldTrialList::CreateFieldTrial("Trial2", "Group2");
  feature_list->RegisterFieldTrialOverride(
      kTestRuntimeFeatureB.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      trial2);
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  {
    // Killswitch FeatureA with a runtime override name "Trial2". This should
    // fail since there already exists a separate "Trial2" (which controls
    // FeatureB).
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "Trial2", "Group3", {}, {kTestRuntimeFeatureA.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kTrialNameCollision, 1);
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureB));
    EXPECT_FALSE(base::RuntimeFieldTrialOverrides::GetInstance()
                     ->GetRuntimeOverride("Trial2")
                     .has_value());
  }

  {
    // Killswitch FeatureA with a runtime override name "Trial1". Although there
    // already exists a "Trial1" (which controls FeatureA), the killswitch is
    // still applied as it is overriding that exact trial and hence does not
    // introduce a name collision.
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "Trial1", "Group3", {}, {kTestRuntimeFeatureA.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kSuccess, 1);
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureB));
    auto override =
        base::RuntimeFieldTrialOverrides::GetInstance()->GetRuntimeOverride(
            "Trial1");
    ASSERT_TRUE(override.has_value());
    EXPECT_EQ(override->group_name, "Group3");
    EXPECT_EQ(override->overridden_trial, trial1);
  }

  {
    // Killswitch FeatureA with a runtime override name "Killswitch". This
    // should apply as there are no trials at all with the name "Killswitch".
    // It should also replace the "Trial1" override.
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "Killswitch", "Disabled50", {}, {kTestRuntimeFeatureA.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kSuccess, 1);
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureB));
    EXPECT_FALSE(base::RuntimeFieldTrialOverrides::GetInstance()
                     ->GetRuntimeOverride("Trial1")
                     .has_value());
    auto override =
        base::RuntimeFieldTrialOverrides::GetInstance()->GetRuntimeOverride(
            "Killswitch");
    ASSERT_TRUE(override.has_value());
    EXPECT_EQ(override->group_name, "Disabled50");
    EXPECT_EQ(override->overridden_trial, trial1);
  }

  {
    // Killswitch FeatureB with a runtime override name "Killswitch". This
    // should not apply as there already exists a runtime override with the
    // name "Killswitch" (which controls FeatureA).
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "Killswitch", "DisableB", {}, {kTestRuntimeFeatureB.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kTrialNameCollision, 1);
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureB));
    // The previously existing override should be unchanged.
    auto override =
        base::RuntimeFieldTrialOverrides::GetInstance()->GetRuntimeOverride(
            "Killswitch");
    ASSERT_TRUE(override.has_value());
    EXPECT_EQ(override->group_name, "Disabled50");
    EXPECT_EQ(override->overridden_trial, trial1);
  }

  {
    // Killswitch FeatureA with a runtime override name "Killswitch". Although
    // there already exists a "Killswitch" (which controls FeatureA), the
    // killswitch is still applied as it is overriding that exact override and
    // hence does not introduce a name collision.
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "Killswitch", "Disabled100", {}, {kTestRuntimeFeatureA.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kSuccess, 1);
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureB));
    auto override =
        base::RuntimeFieldTrialOverrides::GetInstance()->GetRuntimeOverride(
            "Killswitch");
    ASSERT_TRUE(override.has_value());
    EXPECT_EQ(override->group_name, "Disabled100");
    EXPECT_EQ(override->overridden_trial, trial1);
  }

  {
    // Finally, killswitch FeatureA with a runtime override name "Trial1", which
    // is the original name of the trial that disabled FeatureA. For the same
    // reasons as earlier, this should be applied successfully. It should also
    // replace the "Killswitch" override.
    base::HistogramTester histogram_tester;
    VariationsSeed seed = CreateTestRuntimeMutableSeed(
        "Trial1", "Disabled", {}, {kTestRuntimeFeatureA.name});
    service.SimulateAndApplyRuntimeMutableChanges(seed);
    histogram_tester.ExpectUniqueSample(
        kApplyRuntimeMutableChangesResultMetric,
        ApplyRuntimeMutableChangesResult::kSuccess, 1);
    EXPECT_FALSE(base::FeatureList::IsEnabled(kTestRuntimeFeatureA));
    EXPECT_TRUE(base::FeatureList::IsEnabled(kTestRuntimeFeatureB));
    EXPECT_FALSE(base::RuntimeFieldTrialOverrides::GetInstance()
                     ->GetRuntimeOverride("Killswitch")
                     .has_value());
    auto override =
        base::RuntimeFieldTrialOverrides::GetInstance()->GetRuntimeOverride(
            "Trial1");
    ASSERT_TRUE(override.has_value());
    EXPECT_EQ(override->group_name, "Disabled");
    EXPECT_EQ(override->overridden_trial, trial1);
  }
}

// TODO(isherman): Add an integration test for saving and loading a safe seed,
// once the loading functionality is implemented on the seed store.

TEST_F(VariationsServiceTest, VariationsServiceSeedFetchingPauseResume) {
  VariationsService::EnableFetchForTesting();

  // Start with online connection so fetch can happen.
  network_tracker_->SetConnectionType(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);

  TestVariationsService service(
      std::make_unique<web_resource::TestRequestAllowedNotifier>(
          &prefs_, network_tracker_),
      &prefs_, GetMetricsStateManager(), true);
  // Keep intercepts_fetch = true (default).
  service.CancelCurrentRequestForTesting();

  // Pause fetching.
  service.SetSeedFetchingPaused(
      metrics::RuntimeMutableFeaturesHandlerBase::CreatePassKeyForTesting(),
      true);
  EXPECT_TRUE(service.IsSeedFetchingPaused());

  // Start repeated fetch (simulating startup).
  service.StartRepeatedVariationsSeedFetchForTesting();

  // Verify no request was made because it is paused.
  EXPECT_FALSE(service.fetch_attempted());

  // Resume fetching.
  service.SetSeedFetchingPaused(
      metrics::RuntimeMutableFeaturesHandlerBase::CreatePassKeyForTesting(),
      false);
  EXPECT_FALSE(service.IsSeedFetchingPaused());

  // Verify that resume immediately triggered a fetch.
  EXPECT_TRUE(service.fetch_attempted());
}

}  // namespace variations
