// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_scripts_fetcher_impl.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace {

constexpr char kOriginWithScript1[] = "https://example.com";
constexpr char kOriginWithScript2[] = "https://mobile.example.com";
constexpr char kOriginWithScript3[] = "https://test.com";
constexpr char kOriginWithoutScript[] = "https://no-script.com";
constexpr char kVersion[] = "87";

constexpr char kTestResponseContent[] =
    R"json(
      {
        "example.com": {
          "domains": [
            "https://example.com",
            "https://mobile.example.com"
          ],
          "min_version": "86"
        },
        "test.com": {
          "domains": ["https://test.com"],
          "min_version": "87"
        }
      }
    )json";

url::Origin GetOriginWithScript1() {
  return url::Origin::Create(GURL(kOriginWithScript1));
}

url::Origin GetOriginWithScript2() {
  return url::Origin::Create(GURL(kOriginWithScript2));
}

url::Origin GetOriginWithScript3() {
  return url::Origin::Create(GURL(kOriginWithScript3));
}

url::Origin GetOriginWithoutScript() {
  return url::Origin::Create(GURL(kOriginWithoutScript));
}

base::Version GetVersion() {
  return base::Version(kVersion);
}

}  // namespace

namespace password_manager {
class PasswordScriptsFetcherImplTest : public ::testing::Test {
 public:
  void Reinitialize(const base::Version& version) {
    recorded_responses_.clear();
    test_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>();
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            test_url_loader_factory_.get());
    fetcher_ = std::make_unique<PasswordScriptsFetcherImpl>(
        version, test_shared_loader_factory_);
  }

  void SetUp() override {
    // Recreate all classes as they are stateful.
    Reinitialize(GetVersion());
  }

  void TearDown() override {
    EXPECT_EQ(0, GetNumberOfPendingRequests());
    EXPECT_EQ(0u, pending_fetch_finished_callbacks_);
  }

  void SimulateResponse() { SimulateResponseWithContent(kTestResponseContent); }

  void SimulateResponseWithContent(const std::string& content) {
    EXPECT_TRUE(test_url_loader_factory_->SimulateResponseForPendingRequest(
        kDefaultChangePasswordScriptsListUrl, content));
  }

  void SimulateFailedResponse() {
    EXPECT_TRUE(test_url_loader_factory_->SimulateResponseForPendingRequest(
        kDefaultChangePasswordScriptsListUrl, kTestResponseContent,
        net::HttpStatusCode::HTTP_BAD_REQUEST));
  }

  void StartBulkCheck() {
    pending_fetch_finished_callbacks_++;
    fetcher()->RefreshScriptsIfNecessary(
        base::BindOnce(&PasswordScriptsFetcherImplTest::RecordFetchFinished,
                       base::Unretained(this)));
    RequestSingleScriptAvailability(GetOriginWithScript1());
    RequestSingleScriptAvailability(GetOriginWithScript2());
    RequestSingleScriptAvailability(GetOriginWithScript3());
    RequestSingleScriptAvailability(GetOriginWithoutScript());
  }

  void RequestSingleScriptAvailability(const url::Origin& origin) {
    fetcher_->FetchScriptAvailability(origin, GenerateResponseCallback(origin));
  }

  int GetNumberOfPendingRequests() {
    return test_url_loader_factory_->NumPending();
  }

  base::flat_map<url::Origin, bool>& recorded_responses() {
    return recorded_responses_;
  }

  PasswordScriptsFetcherImpl* fetcher() { return fetcher_.get(); }

 private:
  void RecordResponse(url::Origin origin, bool has_script) {
    const auto& it = recorded_responses_.find(origin);
    if (it != recorded_responses_.end()) {
      EXPECT_EQ(recorded_responses_[origin], has_script)
          << "Responses for " << origin << " differ";
    } else {
      recorded_responses_[origin] = has_script;
    }
  }

  void RecordFetchFinished() { pending_fetch_finished_callbacks_--; }

  PasswordScriptsFetcher::ResponseCallback GenerateResponseCallback(
      url::Origin origin) {
    return base::BindOnce(&PasswordScriptsFetcherImplTest::RecordResponse,
                          base::Unretained(this), origin);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::flat_map<url::Origin, bool> recorded_responses_;
  size_t pending_fetch_finished_callbacks_ = 0;
  std::unique_ptr<PasswordScriptsFetcherImpl> fetcher_;
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
};

TEST_F(PasswordScriptsFetcherImplTest, PrewarmCache) {
  std::unique_ptr<base::HistogramTester> histogram_tester =
      std::make_unique<base::HistogramTester>();
  fetcher()->PrewarmCache();
  EXPECT_EQ(1, GetNumberOfPendingRequests());
  SimulateResponse();
  EXPECT_EQ(0, GetNumberOfPendingRequests());

  // The cache is not stale yet. So, no new request is expected.
  fetcher()->PrewarmCache();
  EXPECT_EQ(0, GetNumberOfPendingRequests());

  StartBulkCheck();
  EXPECT_THAT(recorded_responses(),
              UnorderedElementsAre(Pair(GetOriginWithScript1(), true),
                                   Pair(GetOriginWithScript2(), true),
                                   Pair(GetOriginWithScript3(), true),
                                   Pair(GetOriginWithoutScript(), false)));
  EXPECT_EQ(0, GetNumberOfPendingRequests());
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.PasswordScriptsFetcher.CacheState",
      PasswordScriptsFetcher::CacheState::kReady, 1u);

  // Make cache stale and re-fetch the map.
  histogram_tester = std::make_unique<base::HistogramTester>();
  fetcher()->make_cache_stale_for_testing();
  recorded_responses().clear();

  StartBulkCheck();
  EXPECT_EQ(1, GetNumberOfPendingRequests());
  // OriginWithScript2 (test.com) is not available anymore.
  SimulateResponseWithContent(
      R"({
        "example.com":
          {
            "domains": ["https://example.com"],
            "min_version": "86"
          }
        })");
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(recorded_responses(),
              UnorderedElementsAre(Pair(GetOriginWithScript1(), true),
                                   Pair(GetOriginWithScript2(), false),
                                   Pair(GetOriginWithScript3(), false),
                                   Pair(GetOriginWithoutScript(), false)));
  EXPECT_EQ(0, GetNumberOfPendingRequests());
  histogram_tester->ExpectUniqueSample(
      "PasswordManager.PasswordScriptsFetcher.CacheState",
      PasswordScriptsFetcher::CacheState::kStale, 1u);
}

TEST_F(PasswordScriptsFetcherImplTest, SlowResponse) {
  base::HistogramTester histogram_tester;
  fetcher()->PrewarmCache();

  // Bulk check started before server's response.
  StartBulkCheck();
  SimulateResponse();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordScriptsFetcher.CacheState",
      PasswordScriptsFetcher::CacheState::kWaiting, 1u);
}

TEST_F(PasswordScriptsFetcherImplTest, NoHttpSupport) {
  fetcher()->PrewarmCache();
  EXPECT_EQ(1, GetNumberOfPendingRequests());
  SimulateResponseWithContent(
      R"({
        "test.com":
          {
            "domains": ["https://test.com", "http://wrong.scheme.test.com"],
            "min_version": "86"
          }
        })");
  base::RunLoop().RunUntilIdle();

  const url::Origin kOriginWithHttpScheme =
      url::Origin::Create(GURL("http://wrong.scheme.test.com"));
  const url::Origin kOriginWithHttpsScheme =
      url::Origin::Create(GURL("https://test.com"));

  RequestSingleScriptAvailability(kOriginWithHttpsScheme);
  RequestSingleScriptAvailability(kOriginWithHttpScheme);
  EXPECT_THAT(recorded_responses(),
              UnorderedElementsAre(Pair(kOriginWithHttpsScheme, true),
                                   Pair(kOriginWithHttpScheme, false)));
  EXPECT_EQ(0, GetNumberOfPendingRequests());
}

TEST_F(PasswordScriptsFetcherImplTest, NoPrewarmCache) {
  base::HistogramTester histogram_tester;
  StartBulkCheck();  // Without preceding |PrewarmCache|.
  EXPECT_EQ(1, GetNumberOfPendingRequests());
  SimulateResponse();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(recorded_responses(),
              UnorderedElementsAre(Pair(GetOriginWithScript1(), true),
                                   Pair(GetOriginWithScript2(), true),
                                   Pair(GetOriginWithScript3(), true),
                                   Pair(GetOriginWithoutScript(), false)));
  EXPECT_EQ(0, GetNumberOfPendingRequests());

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordScriptsFetcher.CacheState",
      PasswordScriptsFetcher::CacheState::kNeverSet, 1u);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordScriptsFetcher.ParsingResult",
      PasswordScriptsFetcherImpl::ParsingResult::kOk, 1u);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.PasswordScriptsFetcher.ResponseTime", 1u);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordScriptsFetcher.HttpResponseAndNetErrorCode",
      net::HttpStatusCode::HTTP_OK, 1u);
}

TEST_F(PasswordScriptsFetcherImplTest, InvalidResponseBody) {
  const struct TestCase {
    const char* const response;
    PasswordScriptsFetcherImpl::ParsingResult histogram_value;
  } kTestCases[]{
      {"", PasswordScriptsFetcherImpl::ParsingResult::kInvalidJson},
      {"{{{", PasswordScriptsFetcherImpl::ParsingResult::kInvalidJson},
      {R"(["1", "2"])",
       PasswordScriptsFetcherImpl::ParsingResult::kInvalidJson},
      {R"({"no-domains-attribute.com" : {}})",
       PasswordScriptsFetcherImpl::ParsingResult::kInvalidJson},
      {R"({"not-url.com" : {"domains": ["scheme-forgotten.com"], "min_version": "2"}})",
       PasswordScriptsFetcherImpl::ParsingResult::kInvalidUrl},
      {R"({"not-url.com" : {"domains": ["https://no-min-version.com"]}})",
       PasswordScriptsFetcherImpl::ParsingResult::kInvalidJson}};
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "test_case=" << test_case.response);
    base::HistogramTester histogram_tester;

    fetcher()->make_cache_stale_for_testing();
    recorded_responses().clear();

    StartBulkCheck();
    SimulateResponseWithContent(test_case.response);
    base::RunLoop().RunUntilIdle();

    EXPECT_THAT(recorded_responses(),
                UnorderedElementsAre(Pair(GetOriginWithScript1(), false),
                                     Pair(GetOriginWithScript2(), false),
                                     Pair(GetOriginWithScript3(), false),
                                     Pair(GetOriginWithoutScript(), false)));
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.PasswordScriptsFetcher.ParsingResult",
        test_case.histogram_value, 1u);
  }
}

TEST_F(PasswordScriptsFetcherImplTest, ServerError) {
  base::HistogramTester histogram_tester;
  StartBulkCheck();
  SimulateFailedResponse();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(recorded_responses(),
              UnorderedElementsAre(Pair(GetOriginWithScript1(), false),
                                   Pair(GetOriginWithScript2(), false),
                                   Pair(GetOriginWithScript3(), false),
                                   Pair(GetOriginWithoutScript(), false)));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordScriptsFetcher.ParsingResult",
      PasswordScriptsFetcherImpl::ParsingResult::kNoResponse, 1u);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.PasswordScriptsFetcher.ResponseTime", 1u);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordScriptsFetcher.HttpResponseAndNetErrorCode",
      net::HttpStatusCode::HTTP_BAD_REQUEST, 1u);
}

TEST_F(PasswordScriptsFetcherImplTest, IsScriptAvailable) {
  // |IsScriptAvailable| does not trigger any network requests and returns the
  // default value (false).
  EXPECT_FALSE(fetcher()->IsScriptAvailable(GetOriginWithScript1()));
  EXPECT_FALSE(fetcher()->IsScriptAvailable(GetOriginWithScript2()));
  EXPECT_FALSE(fetcher()->IsScriptAvailable(GetOriginWithScript3()));
  EXPECT_FALSE(fetcher()->IsScriptAvailable(GetOriginWithoutScript()));
  EXPECT_EQ(0, GetNumberOfPendingRequests());

  StartBulkCheck();
  EXPECT_EQ(1, GetNumberOfPendingRequests());
  // Bulk check restart doesn't trigger new network request if the cache's state
  // is |kWaiting|.
  StartBulkCheck();
  EXPECT_EQ(1, GetNumberOfPendingRequests());
  SimulateResponse();
  base::RunLoop().RunUntilIdle();
  // Cache is ready.
  EXPECT_TRUE(fetcher()->IsScriptAvailable(GetOriginWithScript1()));
  EXPECT_TRUE(fetcher()->IsScriptAvailable(GetOriginWithScript2()));
  EXPECT_TRUE(fetcher()->IsScriptAvailable(GetOriginWithScript3()));
  EXPECT_FALSE(fetcher()->IsScriptAvailable(GetOriginWithoutScript()));

  // |IsScriptAvailable| does not trigger refetching and returns stale values.
  fetcher()->make_cache_stale_for_testing();
  EXPECT_TRUE(fetcher()->IsScriptAvailable(GetOriginWithScript1()));
  EXPECT_TRUE(fetcher()->IsScriptAvailable(GetOriginWithScript2()));
  EXPECT_TRUE(fetcher()->IsScriptAvailable(GetOriginWithScript3()));
  EXPECT_FALSE(fetcher()->IsScriptAvailable(GetOriginWithoutScript()));
  EXPECT_EQ(0, GetNumberOfPendingRequests());
}

TEST_F(PasswordScriptsFetcherImplTest, EnablePasswordDomainCapabilitiesFlag) {
  // |kEnablePasswordDomainCapabilities| flag is disabled, |IsScriptAvailable|
  // returns the default value (false).
  EXPECT_FALSE(fetcher()->IsScriptAvailable(GetOriginWithScript1()));
  EXPECT_FALSE(fetcher()->IsScriptAvailable(GetOriginWithScript2()));
  EXPECT_FALSE(fetcher()->IsScriptAvailable(GetOriginWithScript3()));
  EXPECT_FALSE(fetcher()->IsScriptAvailable(GetOriginWithoutScript()));

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      password_manager::features::kForceEnablePasswordDomainCapabilities);

  // |kEnablePasswordDomainCapabilities| is enabled, all scripts should have
  // capabilities enabled.
  EXPECT_TRUE(fetcher()->IsScriptAvailable(GetOriginWithScript1()));
  EXPECT_TRUE(fetcher()->IsScriptAvailable(GetOriginWithScript2()));
  EXPECT_TRUE(fetcher()->IsScriptAvailable(GetOriginWithScript3()));
  EXPECT_TRUE(fetcher()->IsScriptAvailable(GetOriginWithoutScript()));

  EXPECT_EQ(0, GetNumberOfPendingRequests());
  // Request capabilities from the server.
  StartBulkCheck();
  EXPECT_EQ(1, GetNumberOfPendingRequests());
  SimulateResponse();
  base::RunLoop().RunUntilIdle();
  // Cache is ready.
  // All scripts should have capabilities regardless of the server response.
  EXPECT_TRUE(fetcher()->IsScriptAvailable(GetOriginWithScript1()));
  EXPECT_TRUE(fetcher()->IsScriptAvailable(GetOriginWithScript2()));
  EXPECT_TRUE(fetcher()->IsScriptAvailable(GetOriginWithScript3()));
  EXPECT_TRUE(fetcher()->IsScriptAvailable(GetOriginWithoutScript()));
}

TEST_F(PasswordScriptsFetcherImplTest, AnotherScriptsListUrl) {
  // Create a special fetcher with a non-default URL.
  const char kNonDefaultScriptsListUrl[] =
      "https://gstatic.com/experimental_list.json";
  network::TestURLLoaderFactory test_url_loader_factory;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory);
  PasswordScriptsFetcherImpl fetcher(GetVersion(), test_shared_loader_factory,
                                     kNonDefaultScriptsListUrl);

  fetcher.PrewarmCache();
  const char kNonDefaultResponseContent[] =
      R"json(
      {
        "experiment.com": {
          "domains": ["https://experiment.com"],
          "min_version" : "86"
        }
      }
    )json";
  test_url_loader_factory.SimulateResponseForPendingRequest(
      kNonDefaultScriptsListUrl, kNonDefaultResponseContent);

  const char kExperimentalDomain[] = "https://experiment.com";
  // Use |IsScriptAvailable(origin)| instead of |FetchScriptAvailability(origin,
  // callback)| to simplify the test.
  EXPECT_TRUE(fetcher.IsScriptAvailable(
      url::Origin::Create(GURL(kExperimentalDomain))));
  EXPECT_FALSE(fetcher.IsScriptAvailable(GetOriginWithScript3()));
}

TEST_F(PasswordScriptsFetcherImplTest, DifferentVersions) {
  StartBulkCheck();
  EXPECT_EQ(1, GetNumberOfPendingRequests());
  SimulateResponse();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(fetcher()->IsScriptAvailable(GetOriginWithScript3()));

  const char kOlderVersion[] = "86";
  Reinitialize(base::Version(kOlderVersion));
  StartBulkCheck();
  EXPECT_EQ(1, GetNumberOfPendingRequests());
  SimulateResponse();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(fetcher()->IsScriptAvailable(GetOriginWithScript3()));
}

TEST_F(PasswordScriptsFetcherImplTest, DebugInformationForInternals) {
  base::Value::Dict debug_info = fetcher()->GetDebugInformationForInternals();

  const std::string* engine = debug_info.FindString("engine");
  EXPECT_TRUE(engine);
  EXPECT_EQ("gstatic lookup", *engine);

  const std::string* script_list_url = debug_info.FindString("script_list_url");
  EXPECT_TRUE(script_list_url);
  EXPECT_EQ(
      "https://www.gstatic.com/chrome/duplex/change_password_scripts.json",
      *script_list_url);
}

TEST_F(PasswordScriptsFetcherImplTest, CheckCacheEntries) {
  fetcher()->PrewarmCache();
  EXPECT_EQ(1, GetNumberOfPendingRequests());

  // Cache should still be empty, too.
  base::Value::List cache_entries = fetcher()->GetCacheEntries();
  EXPECT_EQ(cache_entries.size(), 0u);

  SimulateResponse();
  EXPECT_EQ(0, GetNumberOfPendingRequests());

  // Cache should now contain three entries.
  cache_entries = fetcher()->GetCacheEntries();
  EXPECT_EQ(cache_entries.size(), 3u);

  std::vector<std::string> urls;
  // Only `kOriginWithoutScript` is not expected to have a script.
  for (auto it = cache_entries.begin(); it != cache_entries.end(); ++it) {
    const std::string* url = it->GetDict().FindString("url");
    ASSERT_TRUE(url);
    urls.push_back(*url);
  }

  // There should be entries for all sites with a script.
  EXPECT_THAT(urls, UnorderedElementsAre(kOriginWithScript1, kOriginWithScript2,
                                         kOriginWithScript3));
}

}  // namespace password_manager
