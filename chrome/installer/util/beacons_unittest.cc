// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/beacons.h"

#include <memory>

#include "base/test/test_reg_util_win.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "build/branding_buildflags.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Bool;
using ::testing::Combine;
using ::testing::Values;
using BeaconType = installer_util::Beacon::BeaconType;
using BeaconScope = installer_util::Beacon::BeaconScope;

namespace installer_util {

// A test fixture that exercises a beacon.
class BeaconTest : public ::testing::TestWithParam<
                       ::testing::tuple<BeaconType, BeaconScope, bool>> {
 protected:
  static const wchar_t kBeaconName[];

  BeaconTest()
      : beacon_type_(::testing::get<0>(GetParam())),
        beacon_scope_(::testing::get<1>(GetParam())),
        system_install_(::testing::get<2>(GetParam())),
        scoped_install_details_(system_install_),
        beacon_(kBeaconName, beacon_type_, beacon_scope_) {}

  void SetUp() override {
    // Override the registry so that tests can freely push state to it.
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
  }

  BeaconType beacon_type() const { return beacon_type_; }
  BeaconScope beacon_scope() const { return beacon_scope_; }
  bool system_install() const { return system_install_; }
  Beacon* beacon() { return &beacon_; }

 private:
  BeaconType beacon_type_;
  BeaconScope beacon_scope_;
  bool system_install_;
  install_static::ScopedInstallDetails scoped_install_details_;
  Beacon beacon_;
  registry_util::RegistryOverrideManager registry_override_manager_;
};

// static
const wchar_t BeaconTest::kBeaconName[] = L"TestBeacon";

// Nothing in the regsitry, so the beacon should not exist.
TEST_P(BeaconTest, GetNonExistent) {
  ASSERT_TRUE(beacon()->Get().is_null());
}

// Updating and then getting the beacon should return a value, and that it is
// within range.
TEST_P(BeaconTest, UpdateAndGet) {
  base::Time before(base::Time::Now());
  beacon()->Update();
  base::Time after(base::Time::Now());
  base::Time beacon_time(beacon()->Get());
  ASSERT_FALSE(beacon_time.is_null());
  ASSERT_LE(before, beacon_time);
  ASSERT_GE(after, beacon_time);
}

// Tests that updating a first beacon only updates it the first time, but doing
// so for a last beacon always updates.
TEST_P(BeaconTest, UpdateTwice) {
  beacon()->Update();
  base::Time beacon_time(beacon()->Get());

  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());

  beacon()->Update();
  if (beacon_type() == BeaconType::FIRST) {
    ASSERT_EQ(beacon_time, beacon()->Get());
  } else {
    ASSERT_NE(beacon_time, beacon()->Get());
  }
}

// Tests that the beacon is written into the proper location in the registry.
TEST_P(BeaconTest, Location) {
  beacon()->Update();
  const install_static::InstallDetails& install_details =
      install_static::InstallDetails::Get();
  HKEY right_root = system_install() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  HKEY wrong_root = system_install() ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
  std::wstring right_key;
  std::wstring wrong_key;
  std::wstring value_name;

  if (beacon_scope() == BeaconScope::PER_INSTALL || !system_install()) {
    value_name = kBeaconName;
    right_key = install_details.GetClientStateKeyPath();
    wrong_key = install_details.GetClientStateMediumKeyPath();
  } else {
    ASSERT_TRUE(base::win::GetUserSidString(&value_name));
    right_key =
        install_details.GetClientStateMediumKeyPath() + L"\\" + kBeaconName;
    wrong_key = install_details.GetClientStateKeyPath();
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Keys should not exist in the wrong root or in the right root but wrong key.
  ASSERT_FALSE(
      base::win::RegKey(wrong_root, right_key.c_str(), KEY_READ).Valid())
      << right_key;
  ASSERT_FALSE(
      base::win::RegKey(wrong_root, wrong_key.c_str(), KEY_READ).Valid())
      << wrong_key;
  ASSERT_FALSE(
      base::win::RegKey(right_root, wrong_key.c_str(), KEY_READ).Valid())
      << wrong_key;
#else
  // The tests above are skipped for Chromium builds because they fail for two
  // reasons:
  // - ClientState and ClientStateMedium are both Software\Chromium.
  // - the registry override manager does its virtualization into
  //   Software\Chromium, so it always exists.

  // Silence unused variable warnings.
  std::ignore = wrong_root;
#endif

  // The right key should exist.
  base::win::RegKey key(right_root, right_key.c_str(), KEY_READ);
  ASSERT_TRUE(key.Valid()) << right_key;
  // And should have the value.
  ASSERT_TRUE(key.HasValue(value_name.c_str())) << value_name;
}

// Run the tests for all combinations of beacon type, scope, and install level.
INSTANTIATE_TEST_SUITE_P(BeaconTest,
                         BeaconTest,
                         Combine(Values(BeaconType::FIRST, BeaconType::LAST),
                                 Values(BeaconScope::PER_USER,
                                        BeaconScope::PER_INSTALL),
                                 Bool()));

class DefaultBrowserBeaconTest
    : public ::testing::TestWithParam<
          std::tuple<install_static::InstallConstantIndex, const char*>> {
 protected:
  using Super = ::testing::TestWithParam<
      std::tuple<install_static::InstallConstantIndex, const char*>>;

  void SetUp() override {
    Super::SetUp();

    auto [mode_index, level] = GetParam();

    system_install_ = (std::string(level) != "user");

    // Configure InstallDetails for the test.
    scoped_install_details_ =
        std::make_unique<install_static::ScopedInstallDetails>(system_install_,
                                                               mode_index);
    // Override the registry so that tests can freely push state to it.
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
  }

  bool system_install_ = false;

 private:
  std::unique_ptr<install_static::ScopedInstallDetails> scoped_install_details_;
  registry_util::RegistryOverrideManager registry_override_manager_;
};

// Tests that the default browser beacons work as expected.
TEST_P(DefaultBrowserBeaconTest, All) {
  std::unique_ptr<Beacon> last_was_default(MakeLastWasDefaultBeacon());
  std::unique_ptr<Beacon> first_not_default(MakeFirstNotDefaultBeacon());

  ASSERT_TRUE(last_was_default->Get().is_null());
  ASSERT_TRUE(first_not_default->Get().is_null());

  // Chrome is not default.
  UpdateDefaultBrowserBeaconWithState(ShellUtil::NOT_DEFAULT);
  ASSERT_TRUE(last_was_default->Get().is_null());
  ASSERT_FALSE(first_not_default->Get().is_null());

  // Then it is.
  UpdateDefaultBrowserBeaconWithState(ShellUtil::IS_DEFAULT);
  ASSERT_FALSE(last_was_default->Get().is_null());
  ASSERT_TRUE(first_not_default->Get().is_null());

  // It still is.
  UpdateDefaultBrowserBeaconWithState(ShellUtil::IS_DEFAULT);
  ASSERT_FALSE(last_was_default->Get().is_null());
  ASSERT_TRUE(first_not_default->Get().is_null());

  // Now it's not again.
  UpdateDefaultBrowserBeaconWithState(ShellUtil::NOT_DEFAULT);
  ASSERT_FALSE(last_was_default->Get().is_null());
  ASSERT_FALSE(first_not_default->Get().is_null());

  // And it still isn't.
  UpdateDefaultBrowserBeaconWithState(ShellUtil::NOT_DEFAULT);
  ASSERT_FALSE(last_was_default->Get().is_null());
  ASSERT_FALSE(first_not_default->Get().is_null());
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Stable supports user and system levels.
INSTANTIATE_TEST_SUITE_P(
    Stable,
    DefaultBrowserBeaconTest,
    testing::Combine(testing::Values(install_static::STABLE_INDEX),
                     testing::Values("user", "system")));
// Beta supports user and system levels.
INSTANTIATE_TEST_SUITE_P(
    Beta,
    DefaultBrowserBeaconTest,
    testing::Combine(testing::Values(install_static::BETA_INDEX),
                     testing::Values("user", "system")));
// Dev supports user and system levels.
INSTANTIATE_TEST_SUITE_P(
    Dev,
    DefaultBrowserBeaconTest,
    testing::Combine(testing::Values(install_static::DEV_INDEX),
                     testing::Values("user", "system")));
// Canary is only at user level.
INSTANTIATE_TEST_SUITE_P(
    Canary,
    DefaultBrowserBeaconTest,
    testing::Combine(testing::Values(install_static::CANARY_INDEX),
                     testing::Values("user")));
#elif BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
// Chrome for Testing is only at user level.
INSTANTIATE_TEST_SUITE_P(
    ChromeForTesting,
    DefaultBrowserBeaconTest,
    testing::Combine(
        testing::Values(install_static::GOOGLE_CHROME_FOR_TESTING_INDEX),
        testing::Values("user")));
#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Chromium supports user and system levels.
INSTANTIATE_TEST_SUITE_P(
    Chromium,
    DefaultBrowserBeaconTest,
    testing::Combine(testing::Values(install_static::CHROMIUM_INDEX),
                     testing::Values("user", "system")));
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace installer_util
