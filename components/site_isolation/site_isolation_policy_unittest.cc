// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_isolation/site_isolation_policy.h"

#include "base/base_switches.h"
#include "base/system/sys_info.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_field_trial_list_resetter.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace site_isolation {
namespace {

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

    bool ShouldDisableSiteIsolation() override {
      return SiteIsolationPolicy::
          ShouldDisableSiteIsolationDueToMemoryThreshold();
    }

    std::vector<url::Origin> GetOriginsRequiringDedicatedProcess() override {
      return GetBrowserSpecificBuiltInIsolatedOrigins();
    }

    bool strict_isolation_enabled_ = false;
  };

  SiteIsolationContentBrowserClient browser_client_;
  content::ContentBrowserClient* original_client_ = nullptr;
};

class SiteIsolationPolicyTest : public BaseSiteIsolationTest {
 public:
  SiteIsolationPolicyTest() {
    prefs_.registry()->RegisterListPref(prefs::kUserTriggeredIsolatedOrigins);
    user_prefs::UserPrefs::Set(&browser_context_, &prefs_);
  }

 protected:
  content::BrowserContext* browser_context() { return &browser_context_; }

  PrefService* prefs() { return &prefs_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  TestingPrefServiceSimple prefs_;

  DISALLOW_COPY_AND_ASSIGN(SiteIsolationPolicyTest);
};

// Helper class that enables site isolation for password sites.
class PasswordSiteIsolationPolicyTest : public SiteIsolationPolicyTest {
 public:
  PasswordSiteIsolationPolicyTest() = default;

 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        features::kSiteIsolationForPasswordSites);
    SetEnableStrictSiteIsolation(false);
    SiteIsolationPolicyTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(PasswordSiteIsolationPolicyTest);
};

// Verifies that SiteIsolationPolicy::ApplyPersistedIsolatedOrigins applies
// stored isolated origins correctly when using site isolation for password
// sites.
TEST_F(PasswordSiteIsolationPolicyTest, ApplyPersistedIsolatedOrigins) {
  EXPECT_TRUE(SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled());

  // Add foo.com and bar.com to stored isolated origins.
  {
    ListPrefUpdate update(prefs(), prefs::kUserTriggeredIsolatedOrigins);
    base::ListValue* list = update.Get();
    list->Append("http://foo.com");
    list->Append("https://bar.com");
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

 protected:
  void SetUp() override {
    feature_list_.InitAndDisableFeature(
        features::kSiteIsolationForPasswordSites);
    SetEnableStrictSiteIsolation(false);
    SiteIsolationPolicyTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(NoPasswordSiteIsolationPolicyTest);
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
    ListPrefUpdate update(prefs(), prefs::kUserTriggeredIsolatedOrigins);
    base::ListValue* list = update.Get();
    list->Append("http://foo.com");
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
    switch (GetParam().threshold) {
      case SitePerProcessMemoryThreshold::kNone:
        break;
      case SitePerProcessMemoryThreshold::k128MB:
        threshold_feature_.InitAndEnableFeatureWithParameters(
            features::kSitePerProcessOnlyForHighMemoryClients,
            {{features::kSitePerProcessOnlyForHighMemoryClientsParamName,
              "128"}});
        break;
      case SitePerProcessMemoryThreshold::k768MB:
        threshold_feature_.InitAndEnableFeatureWithParameters(
            features::kSitePerProcessOnlyForHighMemoryClients,
            {{features::kSitePerProcessOnlyForHighMemoryClientsParamName,
              "768"}});
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
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  // These are the origins we expect to be returned by
  // content::ChildProcessSecurityPolicy::GetIsolatedOrigins() even if
  // ContentBrowserClient::ShouldDisableSiteIsolation() returns true.
  std::vector<url::Origin> expected_embedder_origins_;

#if defined(OS_ANDROID)
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

  DISALLOW_COPY_AND_ASSIGN(SitePerProcessMemoryThresholdBrowserTest);
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
#if defined(OS_ANDROID)
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
#if !defined(OS_ANDROID)
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
#if defined(OS_ANDROID)
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
  explicit PasswordSiteIsolationFieldTrialTest(bool should_enable)
      : field_trial_list_(std::make_unique<base::MockEntropyProvider>()) {
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
  }

 protected:
  base::test::ScopedFieldTrialListResetter trial_list_resetter_;
  base::test::ScopedFeatureList feature_list_;
  base::FieldTrialList field_trial_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordSiteIsolationFieldTrialTest);
};

class EnabledPasswordSiteIsolationFieldTrialTest
    : public PasswordSiteIsolationFieldTrialTest {
 public:
  EnabledPasswordSiteIsolationFieldTrialTest()
      : PasswordSiteIsolationFieldTrialTest(true /* should_enable */) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(EnabledPasswordSiteIsolationFieldTrialTest);
};

class DisabledPasswordSiteIsolationFieldTrialTest
    : public PasswordSiteIsolationFieldTrialTest {
 public:
  DisabledPasswordSiteIsolationFieldTrialTest()
      : PasswordSiteIsolationFieldTrialTest(false /* should_enable */) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DisabledPasswordSiteIsolationFieldTrialTest);
};

TEST_F(EnabledPasswordSiteIsolationFieldTrialTest, BelowThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  // If no memory threshold is defined, password site isolation should be
  // enabled on desktop.  It should be disabled on Android, because Android
  // defaults to a 1900MB memory threshold, which is above the 512MB physical
  // memory that this test simulates.
#if defined(OS_ANDROID)
  EXPECT_FALSE(SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled());
#else
  EXPECT_TRUE(SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled());
#endif

  // Define a memory threshold at 768MB.  Since this is above the 512MB of
  // physical memory that this test simulates, password site isolation should
  // now be disabled.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSitePerProcessOnlyForHighMemoryClients,
      {{features::kSitePerProcessOnlyForHighMemoryClientsParamName, "768"}});

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
#if defined(OS_ANDROID)
  EXPECT_FALSE(SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled());
#else
  EXPECT_TRUE(SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled());
#endif

  // Define a memory threshold at 128MB.  Since this is below the 512MB of
  // physical memory that this test simulates, password site isolation should
  // still be enabled.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSitePerProcessOnlyForHighMemoryClients,
      {{features::kSitePerProcessOnlyForHighMemoryClientsParamName, "128"}});

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
      features::kSitePerProcessOnlyForHighMemoryClients,
      {{features::kSitePerProcessOnlyForHighMemoryClientsParamName, "768"}});

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
      features::kSitePerProcessOnlyForHighMemoryClients,
      {{features::kSitePerProcessOnlyForHighMemoryClientsParamName, "128"}});

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
  explicit StrictOriginIsolationFieldTrialTest(bool should_enable)
      : field_trial_list_(std::make_unique<base::MockEntropyProvider>()) {
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
  }

 protected:
  base::test::ScopedFieldTrialListResetter trial_list_resetter_;
  base::test::ScopedFeatureList feature_list_;
  base::FieldTrialList field_trial_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(StrictOriginIsolationFieldTrialTest);
};

class EnabledStrictOriginIsolationFieldTrialTest
    : public StrictOriginIsolationFieldTrialTest {
 public:
  EnabledStrictOriginIsolationFieldTrialTest()
      : StrictOriginIsolationFieldTrialTest(true /* should_enable */) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(EnabledStrictOriginIsolationFieldTrialTest);
};

class DisabledStrictOriginIsolationFieldTrialTest
    : public StrictOriginIsolationFieldTrialTest {
 public:
  DisabledStrictOriginIsolationFieldTrialTest()
      : StrictOriginIsolationFieldTrialTest(false /* should_enable */) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DisabledStrictOriginIsolationFieldTrialTest);
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
#if defined(OS_ANDROID)
  EXPECT_FALSE(content::SiteIsolationPolicy::IsStrictOriginIsolationEnabled());
#else
  EXPECT_TRUE(content::SiteIsolationPolicy::IsStrictOriginIsolationEnabled());
#endif

  // Define a memory threshold at 128MB.  Since this is below the 512MB of
  // physical memory that this test simulates, strict origin isolation should
  // still be enabled.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSitePerProcessOnlyForHighMemoryClients,
      {{features::kSitePerProcessOnlyForHighMemoryClientsParamName, "128"}});
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
      features::kSitePerProcessOnlyForHighMemoryClients,
      {{features::kSitePerProcessOnlyForHighMemoryClientsParamName, "768"}});

  EXPECT_TRUE(content::SiteIsolationPolicy::IsStrictOriginIsolationEnabled());
}

// The following tests verify that the list of Android's built-in isolated
// origins takes effect. This list is only used in official builds, and only
// when above the memory threshold.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(OS_ANDROID)
class BuiltInIsolatedOriginsTest : public SiteIsolationPolicyTest {
 public:
  BuiltInIsolatedOriginsTest() = default;

 protected:
  void SetUp() override {
    // Simulate a 512MB device.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableLowEndDeviceMode);
    EXPECT_EQ(512, base::SysInfo::AmountOfPhysicalMemoryMB());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BuiltInIsolatedOriginsTest);
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
      features::kSitePerProcessOnlyForHighMemoryClients,
      {{features::kSitePerProcessOnlyForHighMemoryClientsParamName, "128"}});

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
      features::kSitePerProcessOnlyForHighMemoryClients,
      {{features::kSitePerProcessOnlyForHighMemoryClientsParamName, "768"}});

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
      features::kSitePerProcessOnlyForHighMemoryClients,
      {{features::kSitePerProcessOnlyForHighMemoryClientsParamName, "128"}});

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

  DISALLOW_COPY_AND_ASSIGN(OptInOriginIsolationPolicyTest);
};

// Check that opt-in origin isolation is not applied when below the memory
// threshold (and when full site isolation is not used).
TEST_F(OptInOriginIsolationPolicyTest, BelowThreshold) {
  if (ShouldSkipBecauseOfConflictingCommandLineSwitches())
    return;

  // Define a memory threshold at 768MB.  This is above the 512MB of physical
  // memory that this test simulates, so opt-in origin isolation should be
  // disabled.
  base::test::ScopedFeatureList memory_feature;
  memory_feature.InitAndEnableFeatureWithParameters(
      features::kSitePerProcessOnlyForHighMemoryClients,
      {{features::kSitePerProcessOnlyForHighMemoryClientsParamName, "768"}});

  EXPECT_FALSE(content::SiteIsolationPolicy::IsOptInOriginIsolationEnabled());

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
      features::kSitePerProcessOnlyForHighMemoryClients,
      {{features::kSitePerProcessOnlyForHighMemoryClientsParamName, "128"}});

  EXPECT_TRUE(content::SiteIsolationPolicy::IsOptInOriginIsolationEnabled());

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
