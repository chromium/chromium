// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_settings_impl.h"

#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include "base/json/values_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "base/values.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/privacy_sandbox/privacy_sandbox_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

namespace privacy_sandbox {
namespace {

using ::privacy_sandbox_test_util::MultipleStateKeys;
using ::privacy_sandbox_test_util::SiteDataExceptions;
using ::privacy_sandbox_test_util::TestCase;
using ::privacy_sandbox_test_util::TestInput;
using ::privacy_sandbox_test_util::TestOutput;
using ::privacy_sandbox_test_util::TestState;

using enum privacy_sandbox_test_util::StateKey;
using enum privacy_sandbox_test_util::InputKey;
using enum privacy_sandbox_test_util::OutputKey;

}  // namespace

class PrivacySandboxSettingsTest : public testing::Test {
 public:
  PrivacySandboxSettingsTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        scoped_attestations_(PrivacySandboxAttestations::CreateForTesting()) {
    // Mark all Privacy Sandbox APIs as attested since the test cases are
    // testing behaviors not related to attestations.
    PrivacySandboxAttestations::GetInstance()
        ->SetAllPrivacySandboxAttestedForTesting(true);
    content_settings::CookieSettings::RegisterProfilePrefs(prefs()->registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs()->registry());
    RegisterProfilePrefs(prefs()->registry());
    host_content_settings_map_ = new HostContentSettingsMap(
        &prefs_, false /* is_off_the_record */, false /* store_last_modified */,
        false /* restore_session */, false /* should_record_metrics */);
    cookie_settings_ = new content_settings::CookieSettings(
        host_content_settings_map_.get(), &prefs_, false,
        content_settings::CookieSettings::NoFedCmSharingPermissionsCallback(),
        "chrome-extension");
  }
  ~PrivacySandboxSettingsTest() override {
    cookie_settings()->ShutdownOnUIThread();
    host_content_settings_map()->ShutdownOnUIThread();
  }

  void SetUp() override {
    InitializePrefsBeforeStart();
    InitializeFeaturesBeforeStart();

    privacy_sandbox_settings_ = std::make_unique<PrivacySandboxSettingsImpl>(
        host_content_settings_map(), cookie_settings_, prefs());
  }

  virtual void InitializePrefsBeforeStart() {}

  virtual void InitializeFeaturesBeforeStart() {}

  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }
  HostContentSettingsMap* host_content_settings_map() {
    return host_content_settings_map_.get();
  }
  content_settings::CookieSettings* cookie_settings() {
    return cookie_settings_.get();
  }
  PrivacySandboxSettings* privacy_sandbox_settings() {
    return privacy_sandbox_settings_.get();
  }
  PrivacySandboxSettingsImpl* privacy_sandbox_settings_impl() {
    return privacy_sandbox_settings_.get();
  }
  content::BrowserTaskEnvironment* task_environment() {
    return &browser_task_environment_;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedFeatureList disabled_topics_feature_list_;

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  ScopedPrivacySandboxAttestations scoped_attestations_;

  std::unique_ptr<PrivacySandboxSettingsImpl> privacy_sandbox_settings_;
};

TEST_F(PrivacySandboxSettingsTest, OnRelatedWebsiteSetsEnabledChanged) {
  // OnRelatedWebsiteSetsEnabledChanged() should only call observers when the
  // pref changes.
  privacy_sandbox_test_util::MockPrivacySandboxObserver observer;
  privacy_sandbox_settings()->AddObserver(&observer);
  EXPECT_CALL(observer, OnRelatedWebsiteSetsEnabledChanged(/*enabled=*/true));

  prefs()->SetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled, true);
  testing::Mock::VerifyAndClearExpectations(&observer);
}

// Tests class for the PrivacySandboxSettings4 / M1 launch.
class PrivacySandboxSettingsM1Test : public PrivacySandboxSettingsTest {
 protected:
  void RunTestCase(const TestState& test_state,
                   const TestInput& test_input,
                   const TestOutput& test_output) {
    ASSERT_FALSE(test_case_run_)
        << "Each test fixture should run a single test, to ensure the test "
           "profile is in a known state.";
    test_case_run_ = true;
    auto user_provider = std::make_unique<content_settings::MockProvider>();
    auto* user_provider_raw = user_provider.get();
    auto managed_provider = std::make_unique<content_settings::MockProvider>();
    auto* managed_provider_raw = managed_provider.get();

    content_settings::TestUtils::OverrideProvider(
        host_content_settings_map(), std::move(user_provider),
        content_settings::ProviderType::kPrefProvider);
    content_settings::TestUtils::OverrideProvider(
        host_content_settings_map(), std::move(managed_provider),
        content_settings::ProviderType::kPolicyProvider);

    privacy_sandbox_test_util::RunTestCase(
        task_environment(), prefs(), host_content_settings_map(),
        privacy_sandbox_settings(), nullptr, user_provider_raw,
        managed_provider_raw, TestCase(test_state, test_input, test_output));
  }

 protected:
  // Pseudo-constants for the convenience of tests that need to check values of
  // type bool*. We can't actually make these const, as then dereferencing them
  // would give the wrong type (i.e. const bool*). Since we are using
  // std::variant for the test outputs, the types must exactly match.
  bool kTrue_ = true;
  bool kFalse_ = false;

  bool actual_out_shared_storage_block_is_site_setting_specific_ = false;
  bool actual_out_select_url_block_is_site_setting_specific_ = false;

 private:
  bool test_case_run_ = false;
};

}  // namespace privacy_sandbox
