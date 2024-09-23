// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_isolation/site_isolation_policy.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/system/sys_info.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/site_isolation/features.h"
#include "components/site_isolation/pref_names.h"
#include "components/site_isolation/preloaded_isolated_origins.h"
#include "components/user_prefs/user_prefs.h"
#include "components/variations/variations_switches.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_response_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace site_isolation {
namespace {

using IsolatedOriginSource =
    content::ChildProcessSecurityPolicy::IsolatedOriginSource;

// Some command-line switches override field trials - the tests need to be
// skipped in this case.
bool ShouldSkipBecauseOfConflictingCommandLineSwitches() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess))
    return true;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableSiteIsolation))
    return true;

  return false;
}

}  // namespace

// Base class for site isolation tests which handles setting a
// ContentBrowserClient with logic for enabling/disabling site isolation.
class BaseSiteIsolationTest : public testing::Test {
 public:
  BaseSiteIsolationTest() {
    original_client_ = content::SetBrowserClientForTesting(&browser_client_);
    SetEnableStrictSiteIsolation(
        original_client_->ShouldEnableStrictSiteIsolation());
  }

  ~BaseSiteIsolationTest() override {
    content::SetBrowserClientForTesting(original_client_);
  }

  void SetUp() override {
    SiteIsolationPolicy::SetDisallowMemoryThresholdCachingForTesting(true);
  }

  void TearDown() override {
    SiteIsolationPolicy::SetDisallowMemoryThresholdCachingForTesting(false);
  }

 protected:
  void SetEnableStrictSiteIsolation(bool enable) {
    browser_client_.strict_isolation_enabled_ = enable;
  }

 private:
  class SiteIsolationContentBrowserClient
      : public content::ContentBrowserClient {
   public:
    bool ShouldEnableStrictSiteIsolation() override {
      return strict_isolation_enabled_;
    }

    bool ShouldDisableSiteIsolation(
        content::SiteIsolationMode site_isolation_mode) override {
      return SiteIsolationPolicy::
          ShouldDisableSiteIsolationDueToMemoryThreshold(site_isolation_mode);
    }

    std::vector<url::Origin> GetOriginsRequiringDedicatedProcess() override {
      return GetBrowserSpecificBuiltInIsolatedOrigins();
    }

    bool strict_isolation_enabled_ = false;
  };

  SiteIsolationContentBrowserClient browser_client_;
  raw_ptr<content::ContentBrowserClient> original_client_ = nullptr;
};

// Tests with OriginKeyedProcessesByDefault enabled.
class OriginKeyedProcessesByDefaultSiteIsolationPolicyTest
    : public BaseSiteIsolationTest {
 public:
  OriginKeyedProcessesByDefaultSiteIsolationPolicyTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{::features::kOriginKeyedProcessesByDefault},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Make sure AreOriginKeyedProcessesEnabledByDefault() only returns true when
// StrictSiteIsolation is enabled.
TEST_F(OriginKeyedProcessesByDefaultSiteIsolationPolicyTest,
       RequiresStrictSiteIsolation) {
  SetEnableStrictSiteIsolation(false);
  // Even though we've disabled ShouldEnableStrictSiteIsolation via the test
  // ContentBrowserClient, if this test runs on a bot where --site-per-process
  // is specified on the command line, UseDedicatedProcessesForAllSites() will
  // still be true, which will enable AreOriginKeyedProcessesEnabledByDefault().
  EXPECT_EQ(
      content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites(),
      content::SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault());
  SetEnableStrictSiteIsolation(true);
  // When this runs on Android, the return value from
  // SiteIsolationContentBrowserClient::ShouldDisableSiteIsolation() may still
  // override our attempt to SetEnableStrictSiteIsolation(true).
  EXPECT_EQ(
      content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites(),
      content::SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault());
}

class SiteIsolationPolicyTest : public BaseSiteIsolationTest {
 public:
  explicit SiteIsolationPolicyTest(
      content::BrowserTaskEnvironment::TimeSource time_source =
          content::BrowserTaskEnvironment::TimeSource::DEFAULT)
      : task_environment_(time_source) {
    prefs_.registry()->RegisterListPref(prefs::kUserTriggeredIsolatedOrigins);
    prefs_.registry()->RegisterDictionaryPref(
        prefs::kWebTriggeredIsolatedOrigins);
    user_prefs::UserPrefs::Set(&browser_context_, &prefs_);
  }

  SiteIsolationPolicyTest(const SiteIsolationPolicyTest&) = delete;
  SiteIsolationPolicyTest& operator=(const SiteIsolationPolicyTest&) = delete;

 protected:
  content::BrowserContext* browser_context() { return &browser_context_; }

  PrefService* prefs() { return &prefs_; }

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  TestingPrefServiceSimple prefs_;
};

class WebTriggeredIsolatedOriginsPolicyTest : public SiteIsolationPolicyTest {
 public:
  WebTriggeredIsolatedOriginsPolicyTest()
      : SiteIsolationPolicyTest(
            content::BrowserTaskEnvironment::TimeSource::MOCK_TIME) {}

  WebTriggeredIsolatedOriginsPolicyTest(
      const WebTriggeredIsolatedOriginsPolicyTest&) = delete;
  WebTriggeredIsolatedOriginsPolicyTest& operator=(
      const WebTriggeredIsolatedOriginsPolicyTest&) = delete;

  void PersistOrigin(const std::string& origin) {
    SiteIsolationPolicy::PersistIsolatedOrigin(
        browser_context(), url::Origin::Create(GURL(origin)),
        IsolatedOriginSource::WEB_TRIGGERED);
    task_environment()->FastForwardBy(base::Milliseconds(1));
  }

  std::vector<std::string> GetStoredOrigins() {
    std::vector<std::string> origins;
    const auto& dict = user_prefs::UserPrefs::Get(browser_context())
                           ->GetDict(prefs::kWebTriggeredIsolatedOrigins);
    for (auto pair : dict)
      origins.push_back(pair.first);
    return origins;
  }

 protected:
  void SetUp() override {
    // Set up the COOP isolation feature with persistence enabled and a maximum
    // of 3 stored sites.
    base::test::FeatureRefAndParams coop_feature = {
        ::features::kSiteIsolationForCrossOriginOpenerPolicy,
        {{::features::kSiteIsolationForCrossOriginOpenerPolicyMaxSitesParam
              .name,
          base::NumberToString(3)},
         {::features::kSiteIsolationForCrossOriginOpenerPolicyShouldPersistParam
              .name,
          "true"}}};

    // Some machines running this test may be below the default memory
    // threshold.  To ensure that COOP isolation is also enabled on those
    // machines, set a very low 128MB threshold.
    base::test::FeatureRefAndParams memory_threshold_feature = {
        site_isolation::features::kSiteIsolationMemoryThresholds,
        {{site_isolation::features::
              kPartialSiteIsolationMemoryThresholdParamName,
          "128"}}};

    feature_list_.InitWithFeaturesAndParameters(
        /* enabled_features = */ {coop_feature, memory_threshold_feature},
        /* disabled_features = */ {});

    // Disable strict site isolation to observe effects of COOP isolation.
    SetEnableStrictSiteIsolation(false);
    SiteIsolationPolicyTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verify that persisting web-triggered isolated origins properly saves the
// origins to prefs and respects the maximum number of entries (3 in this
// test).
TEST_F(WebTriggeredIsolatedOriginsPolicyTest, PersistIsolatedOrigin) {
  PersistOrigin("https://foo1.com");
  PersistOrigin("https://foo2.com");
  PersistOrigin("https://foo3.com");

  EXPECT_THAT(GetStoredOrigins(),
              testing::UnorderedElementsAre(
                  "https://foo1.com", "https://foo2.com", "https://foo3.com"));

  // Adding foo4.com should evict the oldest entry (foo1.com).
  PersistOrigin("https://foo4.com");
  EXPECT_THAT(GetStoredOrigins(),
              testing::UnorderedElementsAre(
                  "https://foo2.com", "https://foo3.com", "https://foo4.com"));

  // Adding foo5.com and foo6.com should evict the next two oldest entries.
  PersistOrigin("https://foo5.com");
  PersistOrigin("https://foo6.com");
  EXPECT_THAT(GetStoredOrigins(),
              testing::UnorderedElementsAre(
                  "https://foo4.com", "https://foo5.com", "https://foo6.com"));

  // Updating the timestamp on foo5.com should keep the current three entries.
  PersistOrigin("https://foo5.com");
  EXPECT_THAT(GetStoredOrigins(),
              testing::UnorderedElementsAre(
                  "https://foo4.com", "https://foo5.com", "https://foo6.com"));

  // Adding two new entries should now evict foo4.com and foo6.com, since
  // foo5.com has a more recent timestamp.
  PersistOrigin("https://foo7.com");
  PersistOrigin("https://foo8.com");
  EXPECT_THAT(GetStoredOrigins(),
              testing::UnorderedElementsAre(
                  "https://foo5.com", "https://foo7.com", "https://foo8.com"));
}

// Verify that when origins stored in prefs contain more than the current
// maximum number of entries, we clean up older entries when adding a new one
// to go back under the size limit.
TEST_F(WebTriggeredIsolatedOriginsPolicyTest, UpdatedMaxSize) {
  // Populate the pref manually with more entries than the 3 allowed by the
  // field trial param.
  ScopedDictPrefUpdate update(
      user_prefs::UserPrefs::Get(browser_context()),
      site_isolation::prefs::kWebTriggeredIsolatedOrigins);
  base::Value::Dict& dict = update.Get();
  dict.Set("https://foo1.com", base::TimeToValue(base::Time::Now()));
  dict.Set("https://foo2.com", base::TimeToValue(base::Time::Now()));
  dict.Set("https://foo3.com", base::TimeToValue(base::Time::Now()));
  dict.Set("https://foo4.com", base::TimeToValue(base::Time::Now()));
  dict.Set("https://foo5.com", base::TimeToValue(base::Time::Now()));
  EXPECT_THAT(GetStoredOrigins(),
              testing::UnorderedElementsAre(
                  "https://foo1.com", "https://foo2.com", "https://foo3.com",
                  "https://foo4.com", "https://foo5.com"));

  // Now, attempt to save a new origin.  This should evict the three oldest
  // entries to make room for the new origin.
  PersistOrigin("https://foo6.com");
  EXPECT_THAT(GetStoredOrigins(),
              testing::UnorderedElementsAre(
                  "https://foo4.com", "https://foo5.com", "https://foo6.com"));
}

// Verify that when origins stored in prefs expire, we don't apply them when
// loading persisted isolated origins, and we remove them from prefs.
TEST_F(WebTriggeredIsolatedOriginsPolicyTest, Expiration) {
  // Running this test with a command-line --site-per-process flag (which might
  // be the case on some bots) conflicts with the feature configuration in this
  // test.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess)) {
    return;
  }

  EXPECT_TRUE(content::SiteIsolationPolicy::IsSiteIsolationForCOOPEnabled());
  EXPECT_TRUE(content::SiteIsolationPolicy::ShouldPersistIsolatedCOOPSites());

  // Persist two origins which will eventually expire.
  PersistOrigin("https://foo1.com");
  PersistOrigin("https://foo2.com");
  EXPECT_THAT(GetStoredOrigins(), testing::UnorderedElementsAre(
                                      "https://foo1.com", "https://foo2.com"));

  // Fast-forward time so we exceed the default expiration timeout.
  base::TimeDelta default_timeout =
      ::features::kSiteIsolationForCrossOriginOpenerPolicyExpirationTimeoutParam
          .default_value;
  task_environment()->FastForwardBy(default_timeout + base::Days(1));

  // foo1.com and foo2.com should still be in prefs. (Expired entries are only
  // removed when we try to load them from prefs.)
  EXPECT_THAT(GetStoredOrigins(), testing::UnorderedElementsAre(
                                      "https://foo1.com", "https://foo2.com"));

  // Persist another origin which should remain below expiration threshold.
  PersistOrigin("https://foo3.com");
  EXPECT_THAT(GetStoredOrigins(),
              testing::UnorderedElementsAre(
                  "https://foo1.com", "https://foo2.com", "https://foo3.com"));

  // Loading persisted isolated origins should only load foo3.com.  Also,
  // it should remove foo1.com and foo2.com from prefs.
  SiteIsolationPolicy::ApplyPersistedIsolatedOrigins(browser_context());

  auto* cpsp = content::ChildProcessSecurityPolicy::GetInstance();
  std::vector<url::Origin> isolated_origins = cpsp->GetIsolatedOrigins(
      IsolatedOriginSource::WEB_TRIGGERED, browser_context());
  EXPECT_THAT(isolated_origins,
              testing::UnorderedElementsAre(
                  url::Origin::Create(GURL("https://foo3.com"))));
  EXPECT_THAT(GetStoredOrigins(),
              testing::UnorderedElementsAre("https://foo3.com"));
}

// Helper class that enables site isolation for password sites.
class PasswordSiteIsolationPolicyTest : public SiteIsolationPolicyTest {
 public:
  PasswordSiteIsolationPolicyTest() = default;

  PasswordSiteIsolationPolicyTest(const PasswordSiteIsolationPolicyTest&) =
      delete;
  PasswordSiteIsolationPolicyTest& operator=(
      const PasswordSiteIsolationPolicyTest&) = delete;

 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        features::kSiteIsolationForPasswordSites);
    SetEnableStrictSiteIsolation(false);
    SiteIsolationPolicyTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies that SiteIsolationPolicy::ApplyPersistedIsolatedOrigins applies
// stored isolated origins correctly when using site isolation for password
// sites.
TEST_F(PasswordSiteIsolationPolicyTest, ApplyPersistedIsolatedOrigins) {
  EXPECT_TRUE(SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled());

  // Add foo.com and bar.com to stored isolated origins.
  {
    ScopedListPrefUpdate update(prefs(), prefs::kUserTriggeredIsolatedOrigins);
    base::Value::List& list = update.Get();
    list.Append("http://foo.com");
    list.Append("https://bar.com");
  }

  // New SiteInstances for foo.com and bar.com shouldn't require a dedicated
  // process to start with.  An exception is if this test runs with a
  // command-line --site-per-process flag (which might be the case on some
  // bots).  This will override the feature configuration in this test and make
  // all sites isolated.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess)) {
    scoped_refptr<content::SiteInstance> foo_instance =
        content::SiteInstance::CreateForURL(browser_context(),
                                            GURL("http://foo.com/1"));
    EXPECT_FALSE(foo_instance->RequiresDedicatedProcess());

    scoped_refptr<content::SiteInstance> bar_instance =
        content::SiteInstance::CreateForURL(browser_context(),
                                            GURL("https://baz.bar.com/2"));
    EXPECT_FALSE(bar_instance->RequiresDedicatedProcess());
  }

  // Apply isolated origins and ensure that they take effect for SiteInstances
  // in new BrowsingInstances.
  base::HistogramTester histograms;
  SiteIsolationPolicy::ApplyPersistedIsolatedOrigins(browser_context());
  histograms.ExpectUniqueSample(
      "SiteIsolation.SavedUserTriggeredIsolatedOrigins.Size", 2, 1);
  {
    scoped_refptr<content::SiteInstance> foo_instance =
        content::SiteInstance::CreateForURL(browser_context(),
                                            GURL("http://foo.com/1"));
    EXPECT_TRUE(foo_instance->RequiresDedicatedProcess());

    scoped_refptr<content::SiteInstance> bar_instance =
        content::SiteInstance::CreateForURL(browser_context(),
                                            GURL("https://baz.bar.com/2"));
    EXPECT_TRUE(bar_instance->RequiresDedicatedProcess());
  }
}

// Helper class that disables strict site isolation as well as site isolation
// for password sites.
class NoPasswordSiteIsolationPolicyTest : public SiteIsolationPolicyTest {
 public:
  NoPasswordSiteIsolationPolicyTest() = default;

  NoPasswordSiteIsolationPolicyTest(const NoPasswordSiteIsolationPolicyTest&) =
      delete;
  NoPasswordSiteIsolationPolicyTest& operator=(
      const NoPasswordSiteIsolationPolicyTest&) = delete;

 protected:
  void SetUp() override {
    feature_list_.InitAndDisableFeature(
        features::kSiteIsolationForPasswordSites);
    SetEnableStrictSiteIsolation(false);
    SiteIsolationPolicyTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies that SiteIsolationPolicy::ApplyPersistedIsolatedOrigins ignores
// stored isolated origins when site isolation for password sites is off.
TEST_F(NoPasswordSiteIsolationPolicyTest,
       PersistedIsolatedOriginsIgnoredWithoutPasswordIsolation) {
  // Running this test with a command-line --site-per-process flag (which might
  // be the case on some bots) doesn't make sense, as that will make all sites
  // isolated, overriding the feature configuration in this test.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess))
    return;

  EXPECT_FALSE(SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled());

  // Add foo.com to stored isolated origins.
  {
    ScopedListPrefUpdate update(prefs(), prefs::kUserTriggeredIsolatedOrigins);
    update->Append("http://foo.com");
  }

  // Applying saved isolated origins should have no effect, since site
  // isolation for password sites is off.
  SiteIsolationPolicy::ApplyPersistedIsolatedOrigins(browser_context());
  scoped_refptr<content::SiteInstance> foo_instance =
      content::SiteInstance::CreateForURL(browser_context(),
                                          GURL("http://foo.com/"));
  EXPECT_FALSE(foo_instance->RequiresDedicatedProcess());
}

enum class SitePerProcessMemoryThreshold {
  kNone,
  k128MB,
  k768MB,
};

enum class SitePerProcessMode {
  kDisabled,
  kEnabled,
  kIsolatedOrigin,
};

struct SitePerProcessMemoryThresholdBrowserTestParams {
  SitePerProcessMemoryThreshold threshold;
  SitePerProcessMode mode;
};

const url::Origin& GetTrialOrigin() {
  static base::NoDestructor<url::Origin> origin{
      url::Origin::Create(GURL("http://foo.com/"))};
  return *origin;
}

// Helper class to run tests on a simulated 512MB low-end device.
class SitePerProcessMemoryThresholdBrowserTest
    : public BaseSiteIsolationTest,
      public ::testing::WithParamInterface<
          SitePerProcessMemoryThresholdBrowserTestParams> {
 public:
  SitePerProcessMemoryThresholdBrowserTest() {
    // When a memory threshold is specified, set it for both strict site
    // isolation and partial site isolation modes, since these tests care about
    // both. For example, UseDedicatedProcessesForAllSites() depends on the
    // former, while isolated origins specified via field trials use the
    // latter.
    switch (GetParam().threshold) {
      case SitePerProcessMemoryThreshold::kNone:
        break;
      case SitePerProcessMemoryThreshold::k128MB:
        threshold_feature_.InitAndEnableFeatureWithParameters(
            features::kSiteIsolationMemoryThresholds,
            {{features::kStrictSiteIsolationMemoryThresholdParamName, "128"},
             {features::kPartialSiteIsolationMemoryThresholdParamName, "128"}});
        break;
      case SitePerProcessMemoryThreshold::k768MB:
        threshold_feature_.InitAndEnableFeatureWithParameters(
            features::kSiteIsolationMemoryThresholds,
            {{features::kStrictSiteIsolationMemoryThresholdParamName, "768"},
             {features::kPartialSiteIsolationMemoryThresholdParamName, "768"}});
        break;
    }

    switch (GetParam().mode) {
      case SitePerProcessMode::kDisabled:
        SetEnableStrictSiteIsolation(false);
        break;
      case SitePerProcessMode::kEnabled:
        SetEnableStrictSiteIsolation(true);
        break;
      case SitePerProcessMode::kIsolatedOrigin:
        mode_feature_.InitAndEnableFeatureWithParameters(
            ::features::kIsolateOrigins,
            {{::features::kIsolateOriginsFieldTrialParamName,
              GetTrialOrigin().Serialize()}});
        break;
    }
  }

  SitePerProcessMemoryThresholdBrowserTest(
      const SitePerProcessMemoryThresholdBrowserTest&) = delete;
  SitePerProcessMemoryThresholdBrowserTest& operator=(
      const SitePerProcessMemoryThresholdBrowserTest&) = delete;

  void SetUp() override {
    // This way the test always sees the same amount of physical memory
    // (kLowMemoryDeviceThresholdMB = 512MB), regardless of how much memory is
    // available in the testing environment.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableLowEndDeviceMode);
    EXPECT_EQ(512, base::SysInfo::AmountOfPhysicalMemoryMB());

    // On Android official builds, we expect to isolate an additional set of
    // built-in origins.
    expected_embedder_origins_ = GetBrowserSpecificBuiltInIsolatedOrigins();
    BaseSiteIsolationTest::SetUp();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  // These are the origins we expect to be returned by
  // content::ChildProcessSecurityPolicy::GetIsolatedOrigins() even if
  // ContentBrowserClient::ShouldDisableSiteIsolation() returns true.
  std::vector<url::Origin> expected_embedder_origins_;

#if BUILDFLAG(IS_ANDROID)
  // On Android we don't expect any trial origins because the 512MB
  // physical memory used for testing is below the Android specific
  // hardcoded 1024MB memory limit that disables site isolation.
  const std::size_t kExpectedTrialOrigins = 0;
#else
  // All other platforms expect the single trial origin to be returned because
  // they don't have the memory limit that disables site isolation.
  const std::size_t kExpectedTrialOrigins = 1;
#endif

 private:
  base::test::ScopedFeatureList threshold_feature_;
  base::test::ScopedFeatureList mode_feature_;
};

using SitePerProcessMemoryThresholdBrowserTestNoIsolation =
    SitePerProcessMemoryThresholdBrowserTest;
TEST_P(SitePerProcessMemoryThresholdBrowserTestNoIsolation, NoIsolation) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  // Isolation should be disabled given the set of parameters used to
  // instantiate these tests.
  EXPECT_FALSE(
      content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
}

using SitePerProcessMemoryThresholdBrowserTestIsolation =
    SitePerProcessMemoryThresholdBrowserTest;
TEST_P(SitePerProcessMemoryThresholdBrowserTestIsolation, Isolation) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  // Isolation should be enabled given the set of parameters used to
  // instantiate these tests.
  EXPECT_TRUE(content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
}

INSTANTIATE_TEST_SUITE_P(
    NoIsolation,
    SitePerProcessMemoryThresholdBrowserTestNoIsolation,
    testing::Values(
#if BUILDFLAG(IS_ANDROID)
        // Expect no isolation on Android because 512MB physical memory
        // triggered by kEnableLowEndDeviceMode in SetUp() is below the 1024MB
        // Android specific memory limit which disables site isolation for all
        // sites.
        SitePerProcessMemoryThresholdBrowserTestParams{
            SitePerProcessMemoryThreshold::kNone, SitePerProcessMode::kEnabled},
#endif
        SitePerProcessMemoryThresholdBrowserTestParams{
            SitePerProcessMemoryThreshold::k768MB,
            SitePerProcessMode::kEnabled},
        SitePerProcessMemoryThresholdBrowserTestParams{
            SitePerProcessMemoryThreshold::kNone,
            SitePerProcessMode::kDisabled},
        SitePerProcessMemoryThresholdBrowserTestParams{
            SitePerProcessMemoryThreshold::k128MB,
            SitePerProcessMode::kDisabled},
        SitePerProcessMemoryThresholdBrowserTestParams{
            SitePerProcessMemoryThreshold::k768MB,
            SitePerProcessMode::kDisabled}));

INSTANTIATE_TEST_SUITE_P(Isolation,
                         SitePerProcessMemoryThresholdBrowserTestIsolation,
                         testing::Values(
#if !BUILDFLAG(IS_ANDROID)
                             // See the note above regarding why this
                             // expectation is different on Android.
                             SitePerProcessMemoryThresholdBrowserTestParams{
                                 SitePerProcessMemoryThreshold::kNone,
                                 SitePerProcessMode::kEnabled},
#endif
                             SitePerProcessMemoryThresholdBrowserTestParams{
                                 SitePerProcessMemoryThreshold::k128MB,
                                 SitePerProcessMode::kEnabled}));

using SitePerProcessMemoryThresholdBrowserTestNoIsolatedOrigin =
    SitePerProcessMemoryThresholdBrowserTest;
TEST_P(SitePerProcessMemoryThresholdBrowserTestNoIsolatedOrigin,
       TrialNoIsolatedOrigin) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  content::SiteIsolationPolicy::ApplyGlobalIsolatedOrigins();

  auto* cpsp = content::ChildProcessSecurityPolicy::GetInstance();
  std::vector<url::Origin> isolated_origins = cpsp->GetIsolatedOrigins();
  EXPECT_EQ(expected_embedder_origins_.size(), isolated_origins.size());

  // Verify that the expected embedder origins are present even though site
  // isolation has been disabled and the trial origins should not be present.
  EXPECT_THAT(expected_embedder_origins_,
              ::testing::IsSubsetOf(isolated_origins));

  // Verify that the trial origin is not present.
  EXPECT_THAT(isolated_origins,
              ::testing::Not(::testing::Contains(GetTrialOrigin())));
}

using SitePerProcessMemoryThresholdBrowserTestIsolatedOrigin =
    SitePerProcessMemoryThresholdBrowserTest;
TEST_P(SitePerProcessMemoryThresholdBrowserTestIsolatedOrigin,
       TrialIsolatedOrigin) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  content::SiteIsolationPolicy::ApplyGlobalIsolatedOrigins();

  auto* cpsp = content::ChildProcessSecurityPolicy::GetInstance();
  std::vector<url::Origin> isolated_origins = cpsp->GetIsolatedOrigins();
  EXPECT_EQ(1u + expected_embedder_origins_.size(), isolated_origins.size());
  EXPECT_THAT(expected_embedder_origins_,
              ::testing::IsSubsetOf(isolated_origins));

  // Verify that the trial origin is present.
  EXPECT_THAT(isolated_origins, ::testing::Contains(GetTrialOrigin()));
}

INSTANTIATE_TEST_SUITE_P(
    TrialNoIsolatedOrigin,
    SitePerProcessMemoryThresholdBrowserTestNoIsolatedOrigin,
    testing::Values(
#if BUILDFLAG(IS_ANDROID)
        // When the memory threshold is not explicitly specified, Android uses
        // a 1900MB global memory threshold.  The 512MB simulated device memory
        // is below 1900MB, so the test origin should not be isolated.
        SitePerProcessMemoryThresholdBrowserTestParams{
            SitePerProcessMemoryThreshold::kNone,
            SitePerProcessMode::kIsolatedOrigin},
#endif
        // The 512MB simulated device memory is under the explicit 768MB memory
        // threshold below, so the test origin should not be isolated.
        SitePerProcessMemoryThresholdBrowserTestParams{
            SitePerProcessMemoryThreshold::k768MB,
            SitePerProcessMode::kIsolatedOrigin}));

INSTANTIATE_TEST_SUITE_P(
    TrialIsolatedOrigin,
    SitePerProcessMemoryThresholdBrowserTestIsolatedOrigin,
    // The 512MB simulated device memory is above the explicit 128MB memory
    // threshold below, so the test origin should be isolated both on desktop
    // and Android.
    testing::Values(SitePerProcessMemoryThresholdBrowserTestParams{
        SitePerProcessMemoryThreshold::k128MB,
        SitePerProcessMode::kIsolatedOrigin}));

// Helper class to run tests with password-triggered site isolation initialized
// via a regular field trial and *not* via a command-line override.  It
// creates a new field trial (with 100% probability of being in the group), and
// initializes the test class's ScopedFeatureList using it.  Two derived
// classes below control are used to initialize the feature to either enabled
// or disabled state.
class PasswordSiteIsolationFieldTrialTest : public BaseSiteIsolationTest {
 public:
  explicit PasswordSiteIsolationFieldTrialTest(bool should_enable) {
    empty_feature_scope_.InitWithEmptyFeatureAndFieldTrialLists();

    const std::string kTrialName = "PasswordSiteIsolation";
    const std::string kGroupName = "FooGroup";  // unused
    scoped_refptr<base::FieldTrial> trial =
        base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);

    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->RegisterFieldTrialOverride(
        features::kSiteIsolationForPasswordSites.name,
        should_enable
            ? base::FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE
            : base::FeatureList::OverrideState::OVERRIDE_DISABLE_FEATURE,
        trial.get());

    feature_list_.InitWithFeatureList(std::move(feature_list));
  }

  PasswordSiteIsolationFieldTrialTest(
      const PasswordSiteIsolationFieldTrialTest&) = delete;
  PasswordSiteIsolationFieldTrialTest& operator=(
      const PasswordSiteIsolationFieldTrialTest&) = delete;

  void SetUp() override {
    // This test creates and tests its own field trial group, so it needs to
    // disable the field trial testing config, which might define an
    // incompatible trial name/group.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        variations::switches::kDisableFieldTrialTestingConfig);

    // This way the test always sees the same amount of physical memory
    // (kLowMemoryDeviceThresholdMB = 512MB), regardless of how much memory is
    // available in the testing environment.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableLowEndDeviceMode);
    EXPECT_EQ(512, base::SysInfo::AmountOfPhysicalMemoryMB());
    BaseSiteIsolationTest::SetUp();
  }

 protected:
  // |empty_feature_scope_| is used to prepare an environment with empty
  // features and field trial lists.
  base::test::ScopedFeatureList empty_feature_scope_;
  // |feature_list_| is used to enable and disable features for
  // PasswordSiteIsolationFieldTrialTest.
  base::test::ScopedFeatureList feature_list_;
};

class EnabledPasswordSiteIsolationFieldTrialTest
    : public PasswordSiteIsolationFieldTrialTest {
 public:
  EnabledPasswordSiteIsolationFieldTrialTest()
      : PasswordSiteIsolationFieldTrialTest(true /* should_enable */) {}

  EnabledPasswordSiteIsolationFieldTrialTest(
      const EnabledPasswordSiteIsolationFieldTrialTest&) = delete;
  EnabledPasswordSiteIsolationFieldTrialTest& operator=(
      const EnabledPasswordSiteIsolationFieldTrialTest&) = delete;
};

class DisabledPasswordSiteIsolationFieldTrialTest
    : public PasswordSiteIsolationFieldTrialTest {
 public:
  DisabledPasswordSiteIsolationFieldTrialTest()
      : PasswordSiteIsolationFieldTrialTest(false /* should_enable */) {}

  DisabledPasswordSiteIsolationFieldTrialTest(
      const DisabledPasswordSiteIsolationFieldTrialTest&) = delete;
  DisabledPasswordSiteIsolationFieldTrialTest& operator=(
      const DisabledPasswordSiteIsolationFieldTrialTest&) = delete;
};

TEST_F(EnabledPasswordSiteIsolationFieldTrialTest, BelowThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  // If no memory threshold is defined, password site isolation should be
  // enabled on desktop.  It should be disabled on Android, because Android
  // defaults to a 1900MB memory threshold, which is above the 512MB physical
  // memory that this test simulates.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled());
#else
  EXPECT_TRUE(SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled());
#endif

  // Define a memory threshold at 768MB.  Since this is above the 512MB of
  // physical memory that this test simulates, password site isolation should
  // now be disabled.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSiteIsolationMemoryThresholds,
      {{features::kPartialSiteIsolationMemoryThresholdParamName, "768"}});

  EXPECT_FALSE(SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled());

  // Simulate enabling password site isolation from command line.  (Note that
  // InitAndEnableFeature uses ScopedFeatureList::InitFromCommandLine
  // internally, and that triggering the feature via chrome://flags follows the
  // same override path as well.)
  base::test::ScopedFeatureList password_site_isolation_feature;
  password_site_isolation_feature.InitAndEnableFeature(
      features::kSiteIsolationForPasswordSites);

  // This should override the memory threshold and enable password site
  // isolation.
  EXPECT_TRUE(SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled());
}

TEST_F(EnabledPasswordSiteIsolationFieldTrialTest, AboveThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  // If no memory threshold is defined, password site isolation should be
  // enabled on desktop.  It should be disabled on Android, because Android
  // defaults to a 1900MB memory threshold, which is above the 512MB physical
  // memory that this test simulates.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled());
#else
  EXPECT_TRUE(SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled());
#endif

  // Define a memory threshold at 128MB.  Since this is below the 512MB of
  // physical memory that this test simulates, password site isolation should
  // still be enabled.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSiteIsolationMemoryThresholds,
      {{features::kPartialSiteIsolationMemoryThresholdParamName, "128"}});

  EXPECT_TRUE(SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled());

  // Simulate disabling password site isolation from command line.  (Note that
  // InitAndEnableFeature uses ScopedFeatureList::InitFromCommandLine
  // internally, and that triggering the feature via chrome://flags follows the
  // same override path as well.)  This should take precedence over the regular
  // field trial behavior.
  base::test::ScopedFeatureList password_site_isolation_feature;
  password_site_isolation_feature.InitAndDisableFeature(
      features::kSiteIsolationForPasswordSites);
  EXPECT_FALSE(SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled());
}

// This test verifies that when password-triggered site isolation is disabled
// via field trials but force-enabled via command line, it takes effect even
// when below the memory threshold.  See https://crbug.com/1009828.
TEST_F(DisabledPasswordSiteIsolationFieldTrialTest,
       CommandLineOverride_BelowThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  // Password site isolation should be disabled at this point.
  EXPECT_FALSE(SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled());

  // Simulate enabling password site isolation from command line.  (Note that
  // InitAndEnableFeature uses ScopedFeatureList::InitFromCommandLine
  // internally, and that triggering the feature via chrome://flags follows the
  // same override path as well.)
  base::test::ScopedFeatureList password_site_isolation_feature;
  password_site_isolation_feature.InitAndEnableFeature(
      features::kSiteIsolationForPasswordSites);

  // If no memory threshold is defined, password site isolation should be
  // enabled.
  EXPECT_TRUE(SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled());

  // Define a memory threshold at 768MB.  This is above the 512MB of physical
  // memory that this test simulates, but password site isolation should still
  // be enabled, because the test has simulated the user manually overriding
  // this feature via command line.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSiteIsolationMemoryThresholds,
      {{features::kPartialSiteIsolationMemoryThresholdParamName, "768"}});

  EXPECT_TRUE(SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled());
}

// Similar to the test above, but with device memory being above memory
// threshold.
TEST_F(DisabledPasswordSiteIsolationFieldTrialTest,
       CommandLineOverride_AboveThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  EXPECT_FALSE(SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled());

  base::test::ScopedFeatureList password_site_isolation_feature;
  password_site_isolation_feature.InitAndEnableFeature(
      features::kSiteIsolationForPasswordSites);

  // If no memory threshold is defined, password site isolation should be
  // enabled.
  EXPECT_TRUE(SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled());

  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSiteIsolationMemoryThresholds,
      {{features::kPartialSiteIsolationMemoryThresholdParamName, "128"}});

  EXPECT_TRUE(SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled());
}

// Helper class to run tests with strict origin isolation initialized via
// a regular field trial and *not* via a command-line override.  It creates a
// new field trial (with 100% probability of being in the group), and
// initializes the test class's ScopedFeatureList using it.  Two derived
// classes below control are used to initialize the feature to either enabled
// or disabled state.
class StrictOriginIsolationFieldTrialTest : public BaseSiteIsolationTest {
 public:
  explicit StrictOriginIsolationFieldTrialTest(bool should_enable) {
    empty_feature_scope_.InitWithEmptyFeatureAndFieldTrialLists();

    const std::string kTrialName = "StrictOriginIsolation";
    const std::string kGroupName = "FooGroup";  // unused
    scoped_refptr<base::FieldTrial> trial =
        base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);

    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->RegisterFieldTrialOverride(
        ::features::kStrictOriginIsolation.name,
        should_enable
            ? base::FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE
            : base::FeatureList::OverrideState::OVERRIDE_DISABLE_FEATURE,
        trial.get());

    feature_list_.InitWithFeatureList(std::move(feature_list));
  }

  StrictOriginIsolationFieldTrialTest(
      const StrictOriginIsolationFieldTrialTest&) = delete;
  StrictOriginIsolationFieldTrialTest& operator=(
      const StrictOriginIsolationFieldTrialTest&) = delete;

  void SetUp() override {
    // This test creates and tests its own field trial group, so it needs to
    // disable the field trial testing config, which might define an
    // incompatible trial name/group.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        variations::switches::kDisableFieldTrialTestingConfig);

    // This way the test always sees the same amount of physical memory
    // (kLowMemoryDeviceThresholdMB = 512MB), regardless of how much memory is
    // available in the testing environment.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableLowEndDeviceMode);
    EXPECT_EQ(512, base::SysInfo::AmountOfPhysicalMemoryMB());
    BaseSiteIsolationTest::SetUp();
  }

 protected:
  // |empty_feature_scope_| is used to prepare an environment with empty
  // features and field trial lists.
  base::test::ScopedFeatureList empty_feature_scope_;
  // |feature_list_| is used to enable and disable features for
  // StrictOriginIsolationFieldTrialTest.
  base::test::ScopedFeatureList feature_list_;
};

class EnabledStrictOriginIsolationFieldTrialTest
    : public StrictOriginIsolationFieldTrialTest {
 public:
  EnabledStrictOriginIsolationFieldTrialTest()
      : StrictOriginIsolationFieldTrialTest(true /* should_enable */) {}

  EnabledStrictOriginIsolationFieldTrialTest(
      const EnabledStrictOriginIsolationFieldTrialTest&) = delete;
  EnabledStrictOriginIsolationFieldTrialTest& operator=(
      const EnabledStrictOriginIsolationFieldTrialTest&) = delete;
};

class DisabledStrictOriginIsolationFieldTrialTest
    : public StrictOriginIsolationFieldTrialTest {
 public:
  DisabledStrictOriginIsolationFieldTrialTest()
      : StrictOriginIsolationFieldTrialTest(false /* should_enable */) {}

  DisabledStrictOriginIsolationFieldTrialTest(
      const DisabledStrictOriginIsolationFieldTrialTest&) = delete;
  DisabledStrictOriginIsolationFieldTrialTest& operator=(
      const DisabledStrictOriginIsolationFieldTrialTest&) = delete;
};

// Check that when strict origin isolation is enabled via a field trial, and
// the device is above the memory threshold, disabling it via the command line
// takes precedence.
TEST_F(EnabledStrictOriginIsolationFieldTrialTest,
       DisabledViaCommandLineOverride) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  // If no memory threshold is defined, strict origin isolation should be
  // enabled on desktop.  It should be disabled on Android, because Android
  // defaults to a 1900MB memory threshold, which is above the 512MB physical
  // memory that this test simulates.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(content::SiteIsolationPolicy::IsStrictOriginIsolationEnabled());
#else
  EXPECT_TRUE(content::SiteIsolationPolicy::IsStrictOriginIsolationEnabled());
#endif

  // Define a memory threshold at 128MB.  Since this is below the 512MB of
  // physical memory that this test simulates, strict origin isolation should
  // still be enabled.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSiteIsolationMemoryThresholds,
      {{features::kStrictSiteIsolationMemoryThresholdParamName, "128"}});
  EXPECT_TRUE(content::SiteIsolationPolicy::IsStrictOriginIsolationEnabled());

  // Simulate disabling strict origin isolation from command line.  (Note that
  // InitAndEnableFeature uses ScopedFeatureList::InitFromCommandLine
  // internally, and that disabling the feature via chrome://flags follows the
  // same override path as well.)
  base::test::ScopedFeatureList strict_origin_isolation_feature;
  strict_origin_isolation_feature.InitAndDisableFeature(
      ::features::kStrictOriginIsolation);
  EXPECT_FALSE(content::SiteIsolationPolicy::IsStrictOriginIsolationEnabled());
}

// This test verifies that when strict origin isolation is disabled
// via field trials but force-enabled via command line, it takes effect even
// when below the memory threshold.  See https://crbug.com/1009828.
TEST_F(DisabledStrictOriginIsolationFieldTrialTest,
       EnabledViaCommandLineOverride_BelowThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  // Strict origin isolation should be disabled at this point.
  EXPECT_FALSE(content::SiteIsolationPolicy::IsStrictOriginIsolationEnabled());

  // Simulate enabling strict origin isolation from command line.  (Note that
  // InitAndEnableFeature uses ScopedFeatureList::InitFromCommandLine
  // internally, and that triggering the feature via chrome://flags follows the
  // same override path as well.)
  base::test::ScopedFeatureList strict_origin_isolation_feature;
  strict_origin_isolation_feature.InitAndEnableFeature(
      ::features::kStrictOriginIsolation);

  // If no memory threshold is defined, strict origin isolation should be
  // enabled.
  EXPECT_TRUE(content::SiteIsolationPolicy::IsStrictOriginIsolationEnabled());

  // Define a memory threshold at 768MB.  This is above the 512MB of physical
  // memory that this test simulates, but strict origin isolation should still
  // be enabled, because the test has simulated the user manually overriding
  // this feature via command line.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSiteIsolationMemoryThresholds,
      {{features::kStrictSiteIsolationMemoryThresholdParamName, "768"}});

  EXPECT_TRUE(content::SiteIsolationPolicy::IsStrictOriginIsolationEnabled());
}

// The following tests verify that the list of Android's built-in isolated
// origins takes effect. This list is only used in official builds, and only
// when above the memory threshold.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && BUILDFLAG(IS_ANDROID)
class BuiltInIsolatedOriginsTest : public SiteIsolationPolicyTest {
 public:
  BuiltInIsolatedOriginsTest() = default;

  BuiltInIsolatedOriginsTest(const BuiltInIsolatedOriginsTest&) = delete;
  BuiltInIsolatedOriginsTest& operator=(const BuiltInIsolatedOriginsTest&) =
      delete;

 protected:
  void SetUp() override {
    // Simulate a 512MB device.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableLowEndDeviceMode);
    EXPECT_EQ(512, base::SysInfo::AmountOfPhysicalMemoryMB());
    SiteIsolationPolicyTest::SetUp();
  }
};

// Check that the list of preloaded isolated origins is properly applied when
// device RAM is above the site isolation memory threshold.
TEST_F(BuiltInIsolatedOriginsTest, DefaultThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  // Define a memory threshold at 128MB.  This is below the 512MB of physical
  // memory that this test simulates, so preloaded isolated origins should take
  // effect.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSiteIsolationMemoryThresholds,
      {{features::kPartialSiteIsolationMemoryThresholdParamName, "128"}});

  // Ensure that isolated origins that are normally loaded on browser
  // startup are applied.
  content::SiteIsolationPolicy::ApplyGlobalIsolatedOrigins();

  EXPECT_TRUE(
      content::SiteIsolationPolicy::ArePreloadedIsolatedOriginsEnabled());

  auto* cpsp = content::ChildProcessSecurityPolicy::GetInstance();
  std::vector<url::Origin> isolated_origins = cpsp->GetIsolatedOrigins(
      content::ChildProcessSecurityPolicy::IsolatedOriginSource::BUILT_IN);

  // The list of built-in origins is fairly large; we don't want to hardcode
  // the size here as it might change, so just check that there are at least 10
  // origins.
  EXPECT_GT(isolated_origins.size(), 10u);

  // Check that a couple of well-known origins are on the list.
  EXPECT_THAT(
      isolated_origins,
      ::testing::Contains(url::Origin::Create(GURL("https://google.com/"))));
  EXPECT_THAT(
      isolated_origins,
      ::testing::Contains(url::Origin::Create(GURL("https://amazon.com/"))));
  EXPECT_THAT(
      isolated_origins,
      ::testing::Contains(url::Origin::Create(GURL("https://facebook.com/"))));

  cpsp->ClearIsolatedOriginsForTesting();
}

TEST_F(BuiltInIsolatedOriginsTest, BelowThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  // Define a memory threshold at 768MB.  This is above the 512MB of physical
  // memory that this test simulates, so preloaded isolated origins shouldn't
  // take effect.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSiteIsolationMemoryThresholds,
      {{features::kPartialSiteIsolationMemoryThresholdParamName, "768"}});

  // Ensure that isolated origins that are normally loaded on browser
  // startup are applied.
  content::SiteIsolationPolicy::ApplyGlobalIsolatedOrigins();

  EXPECT_FALSE(
      content::SiteIsolationPolicy::ArePreloadedIsolatedOriginsEnabled());

  auto* cpsp = content::ChildProcessSecurityPolicy::GetInstance();
  std::vector<url::Origin> isolated_origins = cpsp->GetIsolatedOrigins(
      content::ChildProcessSecurityPolicy::IsolatedOriginSource::BUILT_IN);

  // There shouldn't be any built-in origins on Android. (Note that desktop has
  // some built-in origins that are applied regardless of memory threshold.)
  EXPECT_EQ(isolated_origins.size(), 0u);

  cpsp->ClearIsolatedOriginsForTesting();
}

// Check that the list of preloaded isolated origins is not applied when full
// site isolation is used, since in that case the list is redundant.
TEST_F(BuiltInIsolatedOriginsTest, NotAppliedWithFullSiteIsolation) {
  // Force full site-per-process mode.
  content::IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  // Define a memory threshold at 128MB.  This is below the 512MB of physical
  // memory that this test simulates, so preloaded isolated origins shouldn't
  // be disabled by the memory threshold.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSiteIsolationMemoryThresholds,
      {{features::kPartialSiteIsolationMemoryThresholdParamName, "128"}});

  // Ensure that isolated origins that are normally loaded on browser
  // startup are applied.
  content::SiteIsolationPolicy::ApplyGlobalIsolatedOrigins();

  // Because full site-per-process is used, the preloaded isolated origins are
  // redundant and should not be applied.
  EXPECT_FALSE(
      content::SiteIsolationPolicy::ArePreloadedIsolatedOriginsEnabled());

  auto* cpsp = content::ChildProcessSecurityPolicy::GetInstance();
  std::vector<url::Origin> isolated_origins = cpsp->GetIsolatedOrigins(
      content::ChildProcessSecurityPolicy::IsolatedOriginSource::BUILT_IN);
  EXPECT_EQ(isolated_origins.size(), 0u);
}
#endif

// Helper class for tests that use header-based opt-in origin isolation and
// simulate a 512MB device, while turning off strict site isolation.  This is
// used for checking how opt-in origin isolation behaves with site isolation
// memory thresholds.
class OptInOriginIsolationPolicyTest : public BaseSiteIsolationTest {
 public:
  OptInOriginIsolationPolicyTest() = default;

  OptInOriginIsolationPolicyTest(const OptInOriginIsolationPolicyTest&) =
      delete;
  OptInOriginIsolationPolicyTest& operator=(
      const OptInOriginIsolationPolicyTest&) = delete;

 protected:
  void SetUp() override {
    // Simulate a 512MB device.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableLowEndDeviceMode);
    EXPECT_EQ(512, base::SysInfo::AmountOfPhysicalMemoryMB());
    // Turn off strict site isolation.  This simulates what would happen on
    // Android.
    SetEnableStrictSiteIsolation(false);
    // Enable Origin-Agent-Cluster header.
    feature_list_.InitAndEnableFeature(::features::kOriginIsolationHeader);
    BaseSiteIsolationTest::SetUp();
  }

  content::BrowserContext* browser_context() { return &browser_context_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;

  base::test::ScopedFeatureList feature_list_;
};

// Check that opt-in origin isolation is not applied when below the memory
// threshold (and when full site isolation is not used).
TEST_F(OptInOriginIsolationPolicyTest, BelowThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  // Define a memory threshold at 768MB.  This is above the 512MB of physical
  // memory that this test simulates, so process isolation for
  // Origin-Agent-Cluster (OAC) should be disabled. But other aspects of OAC
  // should still take effect.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSiteIsolationMemoryThresholds,
      {{features::kPartialSiteIsolationMemoryThresholdParamName, "768"}});

  EXPECT_FALSE(content::SiteIsolationPolicy::
                   IsProcessIsolationForOriginAgentClusterEnabled());
  EXPECT_TRUE(content::SiteIsolationPolicy::IsOriginAgentClusterEnabled());

  // Simulate a navigation to a URL that serves an Origin-Agent-Cluster header.
  // Since we're outside of content/, it's difficult to verify that internal
  // ChildProcessSecurityPolicy state wasn't changed by opt-in origin
  // isolation.  Instead, verify that the resulting SiteInstance doesn't
  // require a dedicated process.  This should be the end result, and it
  // implicitly checks that ChildProcessSecurityPolicy::IsIsolatedOrigin()
  // doesn't return true for this origin.
  const GURL kUrl("https://www.google.com/");
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(browser_context(),
                                                        nullptr);
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateBrowserInitiated(kUrl,
                                                           web_contents.get());
  simulator->Start();
  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_headers->SetHeader("Origin-Agent-Cluster", "?1");
  simulator->SetResponseHeaders(response_headers);
  simulator->Commit();

  content::SiteInstance* site_instance =
      simulator->GetFinalRenderFrameHost()->GetSiteInstance();
  EXPECT_FALSE(site_instance->RequiresDedicatedProcess());
  // Despite not getting process isolation, the origin will still get logical
  // isolation in Blink, and should still be tracked by
  // ChildProcessSecurityPolicy to ensure consistent OAC behavior for this
  // origin within this BrowsingInstance.
  EXPECT_TRUE(IsOriginAgentClusterEnabledForOrigin(site_instance,
                                                   url::Origin::Create(kUrl)));
}

// Counterpart to the test above, but verifies that opt-in origin isolation is
// enabled when above the memory threshold.
TEST_F(OptInOriginIsolationPolicyTest, AboveThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  // Define a memory threshold at 128MB.  This is below the 512MB of physical
  // memory that this test simulates, so opt-in origin isolation should be
  // enabled.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSiteIsolationMemoryThresholds,
      {{features::kPartialSiteIsolationMemoryThresholdParamName, "128"}});

  EXPECT_TRUE(content::SiteIsolationPolicy::
                  IsProcessIsolationForOriginAgentClusterEnabled());
  EXPECT_TRUE(content::SiteIsolationPolicy::IsOriginAgentClusterEnabled());

  // Simulate a navigation to a URL that serves an Origin-Agent-Cluster header.
  // Verify that the resulting SiteInstance requires a dedicated process.  Note
  // that this test disables strict site isolation, so this would happen only
  // if opt-in isolation took place.
  const GURL kUrl("https://www.google.com/");
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(browser_context(),
                                                        nullptr);
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateBrowserInitiated(kUrl,
                                                           web_contents.get());
  simulator->Start();
  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_headers->SetHeader("Origin-Agent-Cluster", "?1");
  simulator->SetResponseHeaders(response_headers);
  simulator->Commit();

  content::SiteInstance* site_instance =
      simulator->GetFinalRenderFrameHost()->GetSiteInstance();
  EXPECT_TRUE(site_instance->RequiresDedicatedProcess());
}

}  // namespace site_isolation
