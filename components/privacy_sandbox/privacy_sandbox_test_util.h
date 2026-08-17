// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_TEST_UTIL_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_TEST_UTIL_H_

#include <set>
#include <string>
#include <variant>

#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/origin.h"

namespace sync_preferences {
class TestingPrefServiceSyncable;
}

class HostContentSettingsMap;

namespace privacy_sandbox_test_util {

class PrivacySandboxServiceTestInterface {
 public:
  virtual void ForceChromeBuildForTests(bool force_chrome_build) const = 0;
};

// Allow tests to access private variables and functions from
// `PrivacySandboxSettingsImpl`.
class PrivacySandboxSettingsTestPeer {
 public:
  explicit PrivacySandboxSettingsTestPeer(
      privacy_sandbox::PrivacySandboxSettingsImpl* pss_impl)
      : pss_impl_(pss_impl) {}
  ~PrivacySandboxSettingsTestPeer() = default;

  using Status = privacy_sandbox::PrivacySandboxSettingsImpl::Status;

 private:
  raw_ptr<privacy_sandbox::PrivacySandboxSettingsImpl> pss_impl_;
};

class MockPrivacySandboxObserver
    : public privacy_sandbox::PrivacySandboxSettings::Observer {
 public:
  MockPrivacySandboxObserver();
  ~MockPrivacySandboxObserver();
  MOCK_METHOD1(OnRelatedWebsiteSetsEnabledChanged, void(bool));
};

// A declarative test case is a collection of key value pairs, which each define
// some property of the test, such as the state of the profile, the input, or
// expected output.
// Defines the state of the profile prior to testing.
enum class StateKey {
  kM1TopicsEnabledUserPrefValue = 1,
  kCookieControlsModeUserPrefValue = 2,
  kSiteDataUserDefault = 3,
  kSiteDataUserExceptions = 4,
  kM1FledgeEnabledUserPrefValue = 5,
  kM1AdMeasurementEnabledUserPrefValue = 6,
  kIsIncognito = 7,
  kIsRestrictedAccount = 8,
  kHasCurrentTopics = 9,
  kAdvanceClockBy = 11,
  kM1TopicsDisabledByPolicy = 21,
  kM1FledgeDisabledByPolicy = 22,
  kM1AdMesaurementDisabledByPolicy = 23,
  kAttestationsMap = 26,
  kBlockFledgeJoiningForEtldplus1 = 27,
};

// Defines the input to the functions under test.
enum class InputKey {
  kTopFrameOrigin = 1,
  kTopicsURL = 2,
  kFledgeAuctionPartyOrigin = 3,
  kAdMeasurementReportingOrigin = 4,
  kAccessingOrigin = 7,
  kForceChromeBuild = 9,
  // kPromptAction is Obsolete.
  // TODO(crbug.com/474716334): Remove this enum.
  kPromptAction = 10,
  kEventReportingDestinationOrigin = 11,
  kOutSharedStorageDebugMessage = 12,
  kOutSharedStorageSelectURLDebugMessage = 13,
  kOutSharedStorageBlockIsSiteSettingSpecific = 14,
  kOutSharedStorageSelectURLBlockIsSiteSettingSpecific = 15,
};

// Defines the expected output of the functions under test, when the profile is
// setup as per defined state, and they are provided the defined inputs.
enum class OutputKey {
  kIsSharedStorageAllowed = 6,
  kIsSharedStorageSelectURLAllowed = 7,
  kIsSharedStorageAllowedMetric = 14,
  kIsSharedStorageSelectURLAllowedMetric = 15,
  // kPromptType and kM1PromptSuppressedReason are Obsolete.
  // TODO(crbug.com/474716334): Remove obsolete enums.
  kPromptType = 21,
  kM1TopicsEnabled = 26,
  kM1FledgeEnabled = 27,
  kM1AdMeasurementEnabled = 28,
  kIsSharedStorageAllowedDebugMessage = 48,
  kIsSharedStorageSelectURLAllowedDebugMessage = 49,
  kIsSharedStorageBlockSiteSettingSpecific = 50,
  kIsSharedStorageSelectURLBlockSiteSettingSpecific = 51,
};

// To allow multiple input keys to map to the same value, without having to
// redeclare every such relationship, additional types are defined here. The
// result is that `TestKey` can represent 1:1 and many:1 key value
// relationships.
template <typename T>
using MultipleKeys = std::set<T>;

using MultipleStateKeys = MultipleKeys<StateKey>;
using MultipleInputKeys = MultipleKeys<InputKey>;
using MultipleOutputKeys = MultipleKeys<OutputKey>;

template <typename T>
using TestKey = std::variant<T, MultipleKeys<T>>;

using SiteDataException = std::pair<std::string, ContentSetting>;
using SiteDataExceptions = std::vector<SiteDataException>;

// Although each part of the test case (state, input, output) uses different
// key types, the set of value types associated with those keys is shared, and
// represented by this variant. When accessing keys, the test util will expect
// a particular value type, and will error otherwise.
using TestCaseItemValue =
    std::variant<bool,
                 bool*,
                 std::string,
                 std::string*,
                 url::Origin,
                 GURL,
                 content_settings::CookieControlsMode,
                 SiteDataExceptions,
                 ContentSetting,
                 int,
                 base::Time,
                 base::TimeDelta,
                 std::vector<int>,
                 std::optional<privacy_sandbox::PrivacySandboxAttestationsMap>>;

using TestState = std::map<TestKey<StateKey>, TestCaseItemValue>;
using TestInput = std::map<TestKey<InputKey>, TestCaseItemValue>;
using TestOutput = std::map<TestKey<OutputKey>, TestCaseItemValue>;

using TestCase = std::tuple<TestState, TestInput, TestOutput>;

// Define an additional content setting value to simulate an unmanaged default
// content setting.
const ContentSetting kNoSetting = static_cast<ContentSetting>(-1);

struct CookieContentSettingException {
  std::string primary_pattern;
  std::string secondary_pattern;
  ContentSetting content_setting;
};

// Setup and run the provided test case.
void RunTestCase(
    content::BrowserTaskEnvironment* task_environment,
    sync_preferences::TestingPrefServiceSyncable* testing_pref_service,
    HostContentSettingsMap* host_content_settings_map,
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    PrivacySandboxServiceTestInterface* privacy_sandbox_service,
    content_settings::MockProvider* user_content_setting_provider,
    content_settings::MockProvider* managed_content_setting_provider,
    const TestCase& test_case);

// Applies the state defined by `key`, `value` to the provided profile
// components. This is only exposed for access via the TestUtil unittest.
// Use `RunTestCase()` exclusively elsewhere.
void ApplyTestState(
    StateKey key,
    const TestCaseItemValue& value,
    content::BrowserTaskEnvironment* task_environment,
    sync_preferences::TestingPrefServiceSyncable* testing_pref_service,
    HostContentSettingsMap* map,
    PrivacySandboxServiceTestInterface* privacy_sandbox_service,
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    content_settings::MockProvider* user_content_setting_provider,
    content_settings::MockProvider* managed_content_setting_provider);

// Some input is not directly passed to the function under test, and so must
// be run in advance of checking output. When input is provided directly to
// and output function, it is handled in `CheckOutput()`. This is only exposed
// for access via the TestUtil unit test. Use `RunTestCase()` exclusively
// elsewhere.
void ProvideInput(const std::pair<InputKey, TestCaseItemValue>& input,
                  PrivacySandboxServiceTestInterface* privacy_sandbox_service);

// Checks that the output of functions defined in `output`, when provided with
// appropriate entries from `input` is as expected. This is only exposed for
// access via the TestUtil unit test. Use `RunTestCase()` exclusively elsewhere.
void CheckOutput(
    const std::map<InputKey, TestCaseItemValue>& input,
    const std::pair<OutputKey, TestCaseItemValue>& output,
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    PrivacySandboxServiceTestInterface* privacy_sandbox_service,
    sync_preferences::TestingPrefServiceSyncable* testing_pref_service);

}  // namespace privacy_sandbox_test_util

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_TEST_UTIL_H_
