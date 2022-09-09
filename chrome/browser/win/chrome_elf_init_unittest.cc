// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/chrome_elf_init.h"

#include <memory>
#include <string>

#include "base/metrics/field_trial.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/chrome_elf/blocklist_constants.h"
#include "chrome/common/chrome_version.h"
#include "chrome/install_static/install_util.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ChromeBlocklistTrialTest : public testing::Test {
 public:
  ChromeBlocklistTrialTest(const ChromeBlocklistTrialTest&) = delete;
  ChromeBlocklistTrialTest& operator=(const ChromeBlocklistTrialTest&) = delete;

 protected:
  ChromeBlocklistTrialTest() {}
  ~ChromeBlocklistTrialTest() override {}

  void SetUp() override {
    testing::Test::SetUp();

    ASSERT_NO_FATAL_FAILURE(
        override_manager_.OverrideRegistry(HKEY_CURRENT_USER));

    blocklist_registry_key_ = std::make_unique<base::win::RegKey>(
        HKEY_CURRENT_USER,
        install_static::GetRegistryPath()
            .append(blocklist::kRegistryBeaconKeyName)
            .c_str(),
        KEY_QUERY_VALUE | KEY_SET_VALUE);
  }

  DWORD GetBlocklistState() {
    DWORD blocklist_state = blocklist::BLOCKLIST_STATE_MAX;
    blocklist_registry_key_->ReadValueDW(blocklist::kBeaconState,
                                         &blocklist_state);

    return blocklist_state;
  }

  std::wstring GetBlocklistVersion() {
    std::wstring blocklist_version;
    blocklist_registry_key_->ReadValue(blocklist::kBeaconVersion,
                                       &blocklist_version);

    return blocklist_version;
  }

  std::unique_ptr<base::win::RegKey> blocklist_registry_key_;
  registry_util::RegistryOverrideManager override_manager_;
  content::BrowserTaskEnvironment task_environment_;
};

// Ensure that the default trial sets up the blocklist beacons.
TEST_F(ChromeBlocklistTrialTest, DefaultRun) {
  // Set some dummy values as beacons.
  blocklist_registry_key_->WriteValue(blocklist::kBeaconState,
                                      blocklist::BLOCKLIST_DISABLED);
  blocklist_registry_key_->WriteValue(blocklist::kBeaconVersion, L"Data");

  // This setup code should result in the default group, which should have
  // the blocklist set up.
  InitializeChromeElf();

  // Ensure the beacon values are now correct, indicating the
  // blocklist beacon was setup.
  ASSERT_EQ(static_cast<DWORD>(blocklist::BLOCKLIST_ENABLED),
            GetBlocklistState());
  std::wstring version(base::UTF8ToWide(version_info::GetVersionNumber()));
  ASSERT_EQ(version, GetBlocklistVersion());
}

// Ensure that the blocklist is disabled for any users in the
// "BlocklistDisabled" finch group.
TEST_F(ChromeBlocklistTrialTest, BlocklistDisabledRun) {
  // Set the beacons to enabled values.
  blocklist_registry_key_->WriteValue(blocklist::kBeaconState,
                                      blocklist::BLOCKLIST_ENABLED);
  blocklist_registry_key_->WriteValue(blocklist::kBeaconVersion, L"Data");

  scoped_refptr<base::FieldTrial> trial(
    base::FieldTrialList::CreateFieldTrial(
      kBrowserBlocklistTrialName, kBrowserBlocklistTrialDisabledGroupName));

  // This setup code should now delete any existing blocklist beacons.
  InitializeChromeElf();

  // Ensure invalid values are returned to indicate that the beacon
  // values are indeed gone.
  ASSERT_EQ(static_cast<DWORD>(blocklist::BLOCKLIST_STATE_MAX),
            GetBlocklistState());
  ASSERT_EQ(std::wstring(), GetBlocklistVersion());
}

TEST_F(ChromeBlocklistTrialTest, VerifyFirstRun) {
  BrowserBlocklistBeaconSetup();

  // Verify the state is properly set after the first run.
  ASSERT_EQ(static_cast<DWORD>(blocklist::BLOCKLIST_ENABLED),
            GetBlocklistState());

  std::wstring version(base::UTF8ToWide(version_info::GetVersionNumber()));
  ASSERT_EQ(version, GetBlocklistVersion());
}

TEST_F(ChromeBlocklistTrialTest, BlocklistFailed) {
  // Ensure when the blocklist set up failed we set the state to disabled for
  // future runs.
  blocklist_registry_key_->WriteValue(blocklist::kBeaconVersion,
                                      TEXT(CHROME_VERSION_STRING));
  blocklist_registry_key_->WriteValue(blocklist::kBeaconState,
                                      blocklist::BLOCKLIST_SETUP_FAILED);

  BrowserBlocklistBeaconSetup();

  ASSERT_EQ(static_cast<DWORD>(blocklist::BLOCKLIST_DISABLED),
            GetBlocklistState());
}

TEST_F(ChromeBlocklistTrialTest, VersionChanged) {
  // Mark the blocklist as disabled for an older version, it should
  // get enabled for this new version.  Also record a non-zero number of
  // setup failures, which should be reset to zero.
  blocklist_registry_key_->WriteValue(blocklist::kBeaconVersion,
                                      L"old_version");
  blocklist_registry_key_->WriteValue(blocklist::kBeaconState,
                                      blocklist::BLOCKLIST_DISABLED);
  blocklist_registry_key_->WriteValue(blocklist::kBeaconAttemptCount,
                                      blocklist::kBeaconMaxAttempts);

  BrowserBlocklistBeaconSetup();

  // The beacon should now be marked as enabled for the current version.
  ASSERT_EQ(static_cast<DWORD>(blocklist::BLOCKLIST_ENABLED),
            GetBlocklistState());

  std::wstring expected_version(
      base::UTF8ToWide(version_info::GetVersionNumber()));
  ASSERT_EQ(expected_version, GetBlocklistVersion());

  // The counter should be reset.
  DWORD attempt_count = blocklist::kBeaconMaxAttempts;
  blocklist_registry_key_->ReadValueDW(blocklist::kBeaconAttemptCount,
                                       &attempt_count);
  ASSERT_EQ(static_cast<DWORD>(0), attempt_count);
}

}  // namespace
