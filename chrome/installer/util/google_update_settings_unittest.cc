// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/installer/util/google_update_settings.h"

#include <windows.h>

#include <stddef.h>

#include <memory>
#include <string_view>

#include "base/base_paths.h"
#include "base/hash/hash.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "base/win/shlwapi.h"  // For SHDeleteKey.
#include "build/branding_buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/util/additional_parameters.h"
#include "chrome/installer/util/fake_installation_state.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item_list.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;

namespace {

// This test fixture redirects the HKLM and HKCU registry hives for
// the duration of the test to make it independent of the machine
// and user settings.
class GoogleUpdateSettingsTest : public testing::Test {
 protected:
  enum SystemUserInstall {
    SYSTEM_INSTALL,
    USER_INSTALL,
  };

  GoogleUpdateSettingsTest()
      : program_files_override_(base::DIR_PROGRAM_FILES),
        program_files_x86_override_(base::DIR_PROGRAM_FILESX86) {}

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_overrides_.OverrideRegistry(HKEY_LOCAL_MACHINE));
    ASSERT_NO_FATAL_FAILURE(
        registry_overrides_.OverrideRegistry(HKEY_CURRENT_USER));
  }

  // Creates "ap" key with the value given as parameter. Also adds work
  // items to work_item_list given so that they can be rolled back later.
  bool CreateApKey(WorkItemList* work_item_list, const std::wstring& value) {
    HKEY reg_root = HKEY_CURRENT_USER;
    std::wstring reg_key = GetApKeyPath();
    work_item_list->AddCreateRegKeyWorkItem(reg_root, reg_key, KEY_WOW64_32KEY);
    work_item_list->AddSetRegValueWorkItem(reg_root, reg_key, KEY_WOW64_32KEY,
                                           google_update::kRegApField,
                                           value.c_str(), true);
    if (!work_item_list->Do()) {
      work_item_list->Rollback();
      return false;
    }
    return true;
  }

  static std::wstring GetProductGuid() { return install_static::GetAppGuid(); }

  // Returns the key path of "ap" key, e.g.:
  // Google\Update\ClientState\<kTestProductGuid>
  std::wstring GetApKeyPath() {
    return install_static::GetClientStateKeyPath();
  }

  // Utility method to read "ap" key value
  std::wstring ReadApKeyValue() {
    RegKey key;
    std::wstring ap_key_value;
    std::wstring reg_key = GetApKeyPath();
    if (key.Open(HKEY_CURRENT_USER, reg_key.c_str(),
                 KEY_WOW64_32KEY | KEY_QUERY_VALUE) == ERROR_SUCCESS) {
      key.ReadValue(google_update::kRegApField, &ap_key_value);
    }

    return ap_key_value;
  }

  bool SetUpdatePolicyForAppGuid(const std::wstring& app_guid,
                                 GoogleUpdateSettings::UpdatePolicy policy) {
    RegKey policy_key;
    if (policy_key.Create(HKEY_LOCAL_MACHINE,
                          GoogleUpdateSettings::kPoliciesKey,
                          KEY_SET_VALUE) == ERROR_SUCCESS) {
      std::wstring app_update_override(
          GoogleUpdateSettings::kUpdateOverrideValuePrefix);
      app_update_override.append(app_guid);
      return policy_key.WriteValue(app_update_override.c_str(),
                                   static_cast<DWORD>(policy)) == ERROR_SUCCESS;
    }
    return false;
  }

  GoogleUpdateSettings::UpdatePolicy GetUpdatePolicyForAppGuid(
      const std::wstring& app_guid) {
    RegKey policy_key;
    if (policy_key.Create(HKEY_LOCAL_MACHINE,
                          GoogleUpdateSettings::kPoliciesKey,
                          KEY_QUERY_VALUE) == ERROR_SUCCESS) {
      std::wstring app_update_override(
          GoogleUpdateSettings::kUpdateOverrideValuePrefix);
      app_update_override.append(app_guid);

      DWORD value;
      if (policy_key.ReadValueDW(app_update_override.c_str(), &value) ==
          ERROR_SUCCESS) {
        return static_cast<GoogleUpdateSettings::UpdatePolicy>(value);
      }
    }
    return GoogleUpdateSettings::UPDATE_POLICIES_COUNT;
  }

  bool SetGlobalUpdatePolicy(GoogleUpdateSettings::UpdatePolicy policy) {
    RegKey policy_key;
    return policy_key.Create(HKEY_LOCAL_MACHINE,
                             GoogleUpdateSettings::kPoliciesKey,
                             KEY_SET_VALUE) == ERROR_SUCCESS &&
           policy_key.WriteValue(GoogleUpdateSettings::kUpdatePolicyValue,
                                 static_cast<DWORD>(policy)) == ERROR_SUCCESS;
  }

  GoogleUpdateSettings::UpdatePolicy GetGlobalUpdatePolicy() {
    RegKey policy_key;
    DWORD value;
    return (policy_key.Create(HKEY_LOCAL_MACHINE,
                              GoogleUpdateSettings::kPoliciesKey,
                              KEY_QUERY_VALUE) == ERROR_SUCCESS &&
            policy_key.ReadValueDW(GoogleUpdateSettings::kUpdatePolicyValue,
                                   &value) == ERROR_SUCCESS)
               ? static_cast<GoogleUpdateSettings::UpdatePolicy>(value)
               : GoogleUpdateSettings::UPDATE_POLICIES_COUNT;
  }

  bool SetUpdateTimeoutOverride(DWORD time_in_minutes) {
    RegKey policy_key;
    return policy_key.Create(HKEY_LOCAL_MACHINE,
                             GoogleUpdateSettings::kPoliciesKey,
                             KEY_SET_VALUE) == ERROR_SUCCESS &&
           policy_key.WriteValue(
               GoogleUpdateSettings::kCheckPeriodOverrideMinutes,
               time_in_minutes) == ERROR_SUCCESS;
  }

  // Path overrides so that SHGetFolderPath isn't needed after the registry
  // is overridden.
  base::ScopedPathOverride program_files_override_;
  base::ScopedPathOverride program_files_x86_override_;
  registry_util::RegistryOverrideManager registry_overrides_;
};

}  // namespace

// Run through all combinations of diff vs. full install, success and failure
// results, and a fistful of initial "ap" values checking that the expected
// final "ap" value is generated by
// GoogleUpdateSettings::UpdateGoogleUpdateApKey.
TEST_F(GoogleUpdateSettingsTest, UpdateGoogleUpdateApKey) {
  const installer::ArchiveType archive_types[] = {
      installer::UNKNOWN_ARCHIVE_TYPE, installer::FULL_ARCHIVE_TYPE,
      installer::INCREMENTAL_ARCHIVE_TYPE};
  const int results[] = {installer::FIRST_INSTALL_SUCCESS,
                         installer::INSTALL_FAILED};
  const wchar_t* const plain[] = {L"", L"1.1", L"1.1-dev"};
  const wchar_t* const full[] = {L"-full", L"1.1-full", L"1.1-dev-full"};
  static_assert(std::size(full) == std::size(plain), "bad full array size");
  const wchar_t* const* input_arrays[] = {plain, full};
  for (const installer::ArchiveType archive_type : archive_types) {
    SCOPED_TRACE(::testing::Message()
                 << "archive_type="
                 << (archive_type == installer::UNKNOWN_ARCHIVE_TYPE
                         ? "UNKNOWN"
                         : (archive_type == installer::FULL_ARCHIVE_TYPE
                                ? "FULL"
                                : "INCREMENTAL")));
    for (const int result : results) {
      SCOPED_TRACE(::testing::Message()
                   << "result="
                   << (result == installer::FIRST_INSTALL_SUCCESS ? "SUCCESS"
                                                                  : "FAILED"));
      // The archive type will/must always be known on install success.
      if (archive_type == installer::UNKNOWN_ARCHIVE_TYPE &&
          result == installer::FIRST_INSTALL_SUCCESS) {
        continue;
      }
      const wchar_t* const* outputs = nullptr;
      if (result == installer::FIRST_INSTALL_SUCCESS ||
          archive_type == installer::FULL_ARCHIVE_TYPE) {
        outputs = plain;
      } else if (archive_type == installer::INCREMENTAL_ARCHIVE_TYPE) {
        outputs = full;
      }  // else if (archive_type == UNKNOWN) see below

      for (const wchar_t* const* inputs : input_arrays) {
        if (archive_type == installer::UNKNOWN_ARCHIVE_TYPE) {
          // "-full" is untouched if the archive type is unknown.
          if (inputs == full)
            outputs = full;
          else
            outputs = plain;
        }
        for (size_t input_idx = 0; input_idx < std::size(plain); ++input_idx) {
          const wchar_t* input = inputs[input_idx];
          const wchar_t* output = outputs[input_idx];
          SCOPED_TRACE(::testing::Message() << "input=\"" << input << "\"");
          SCOPED_TRACE(::testing::Message() << "output=\"" << output << "\"");

          std::unique_ptr<WorkItemList> work_item_list(
              WorkItem::CreateWorkItemList());

          ASSERT_TRUE(CreateApKey(work_item_list.get(), input));
          installer::AdditionalParameters ap;
          if (std::wstring_view(output) == ap.value()) {
            EXPECT_FALSE(GoogleUpdateSettings::UpdateGoogleUpdateApKey(
                archive_type, result, &ap));
          } else {
            EXPECT_TRUE(GoogleUpdateSettings::UpdateGoogleUpdateApKey(
                archive_type, result, &ap));
          }
          EXPECT_STREQ(output, ap.value());
        }
      }
    }
  }
}

TEST_F(GoogleUpdateSettingsTest, UpdateInstallStatusTest) {
  std::unique_ptr<WorkItemList> work_item_list(WorkItem::CreateWorkItemList());
  // Test incremental install failure
  ASSERT_TRUE(CreateApKey(work_item_list.get(), L""))
      << "Failed to create ap key.";
  GoogleUpdateSettings::UpdateInstallStatus(
      false, installer::INCREMENTAL_ARCHIVE_TYPE, installer::INSTALL_FAILED);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"-full");
  work_item_list->Rollback();

  work_item_list.reset(WorkItem::CreateWorkItemList());
  // Test incremental install success
  ASSERT_TRUE(CreateApKey(work_item_list.get(), L""))
      << "Failed to create ap key.";
  GoogleUpdateSettings::UpdateInstallStatus(false,
                                            installer::INCREMENTAL_ARCHIVE_TYPE,
                                            installer::FIRST_INSTALL_SUCCESS);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"");
  work_item_list->Rollback();

  work_item_list.reset(WorkItem::CreateWorkItemList());
  // Test full install failure
  ASSERT_TRUE(CreateApKey(work_item_list.get(), L"-full"))
      << "Failed to create ap key.";
  GoogleUpdateSettings::UpdateInstallStatus(false, installer::FULL_ARCHIVE_TYPE,
                                            installer::INSTALL_FAILED);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"");
  work_item_list->Rollback();

  work_item_list.reset(WorkItem::CreateWorkItemList());
  // Test full install success
  ASSERT_TRUE(CreateApKey(work_item_list.get(), L"-full"))
      << "Failed to create ap key.";
  GoogleUpdateSettings::UpdateInstallStatus(false, installer::FULL_ARCHIVE_TYPE,
                                            installer::FIRST_INSTALL_SUCCESS);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"");
  work_item_list->Rollback();

  work_item_list.reset(WorkItem::CreateWorkItemList());
  // Test the case of when "ap" key doesnt exist at all
  std::wstring ap_key_value = ReadApKeyValue();
  std::wstring reg_key = GetApKeyPath();
  HKEY reg_root = HKEY_CURRENT_USER;
  bool ap_key_deleted = false;
  RegKey key;
  if (key.Open(HKEY_CURRENT_USER, reg_key.c_str(),
               KEY_WOW64_32KEY | KEY_SET_VALUE) != ERROR_SUCCESS) {
    work_item_list->AddCreateRegKeyWorkItem(reg_root, reg_key, KEY_WOW64_32KEY);
    ASSERT_TRUE(work_item_list->Do()) << "Failed to create ClientState key.";
  } else if (key.DeleteValue(google_update::kRegApField) == ERROR_SUCCESS) {
    ap_key_deleted = true;
  }
  // try differential installer
  GoogleUpdateSettings::UpdateInstallStatus(
      false, installer::INCREMENTAL_ARCHIVE_TYPE, installer::INSTALL_FAILED);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"-full");
  // try full installer now
  GoogleUpdateSettings::UpdateInstallStatus(false, installer::FULL_ARCHIVE_TYPE,
                                            installer::INSTALL_FAILED);
  EXPECT_STREQ(ReadApKeyValue().c_str(), L"");
  // Now cleanup to leave the system in unchanged state.
  // - Diff installer creates an ap key if it didn't exist, so delete this ap
  // key
  // - If we created any reg key path for ap, roll it back
  // - Finally restore the original value of ap key.
  if (key.Open(HKEY_CURRENT_USER, reg_key.c_str(),
               KEY_WOW64_32KEY | KEY_SET_VALUE) == ERROR_SUCCESS) {
    key.DeleteValue(google_update::kRegApField);
  }
  work_item_list->Rollback();
  if (ap_key_deleted) {
    work_item_list.reset(WorkItem::CreateWorkItemList());
    ASSERT_TRUE(CreateApKey(work_item_list.get(), ap_key_value))
        << "Failed to restore ap key.";
  }
}

TEST_F(GoogleUpdateSettingsTest, SetEulaConsent) {
  using installer::FakeInstallationState;

  const bool system_level = true;
  FakeInstallationState machine_state;

  // Chrome is installed.
  machine_state.AddChrome(system_level,
                          new base::Version(chrome::kChromeVersion));

  RegKey key;
  DWORD value;

  // eulaconsent is set on the product.
  EXPECT_TRUE(GoogleUpdateSettings::SetEulaConsent(machine_state, true));
  EXPECT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE,
                     install_static::GetClientStateMediumKeyPath().c_str(),
                     KEY_QUERY_VALUE));
  EXPECT_EQ(ERROR_SUCCESS,
            key.ReadValueDW(google_update::kRegEulaAceptedField, &value));
  EXPECT_EQ(1U, value);
}

// Test that the appropriate default is returned if no update override is
// present.
TEST_F(GoogleUpdateSettingsTest, GetAppUpdatePolicyNoOverride) {
  // There are no policies at all.
  EXPECT_EQ(ERROR_FILE_NOT_FOUND,
            RegKey().Open(HKEY_LOCAL_MACHINE,
                          GoogleUpdateSettings::kPoliciesKey, KEY_QUERY_VALUE));
  bool is_overridden = true;
  EXPECT_EQ(GoogleUpdateSettings::kDefaultUpdatePolicy,
            GoogleUpdateSettings::GetAppUpdatePolicy(GetProductGuid(),
                                                     &is_overridden));
  EXPECT_FALSE(is_overridden);

  // The policy key exists, but there are no values of interest present.
  EXPECT_EQ(ERROR_SUCCESS,
            RegKey().Create(HKEY_LOCAL_MACHINE,
                            GoogleUpdateSettings::kPoliciesKey, KEY_SET_VALUE));
  EXPECT_EQ(ERROR_SUCCESS,
            RegKey().Open(HKEY_LOCAL_MACHINE,
                          GoogleUpdateSettings::kPoliciesKey, KEY_QUERY_VALUE));
  is_overridden = true;
  EXPECT_EQ(GoogleUpdateSettings::kDefaultUpdatePolicy,
            GoogleUpdateSettings::GetAppUpdatePolicy(GetProductGuid(),
                                                     &is_overridden));
  EXPECT_FALSE(is_overridden);
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Test that the default override is returned if no app-specific override is
// present.
TEST_F(GoogleUpdateSettingsTest, GetAppUpdatePolicyDefaultOverride) {
  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, GoogleUpdateSettings::kPoliciesKey,
                   KEY_SET_VALUE)
                .WriteValue(GoogleUpdateSettings::kUpdatePolicyValue,
                            static_cast<DWORD>(0)));
  bool is_overridden = true;
  EXPECT_EQ(GoogleUpdateSettings::UPDATES_DISABLED,
            GoogleUpdateSettings::GetAppUpdatePolicy(GetProductGuid(),
                                                     &is_overridden));
  EXPECT_FALSE(is_overridden);

  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, GoogleUpdateSettings::kPoliciesKey,
                   KEY_SET_VALUE)
                .WriteValue(GoogleUpdateSettings::kUpdatePolicyValue,
                            static_cast<DWORD>(1)));
  is_overridden = true;
  EXPECT_EQ(GoogleUpdateSettings::AUTOMATIC_UPDATES,
            GoogleUpdateSettings::GetAppUpdatePolicy(GetProductGuid(),
                                                     &is_overridden));
  EXPECT_FALSE(is_overridden);

  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, GoogleUpdateSettings::kPoliciesKey,
                   KEY_SET_VALUE)
                .WriteValue(GoogleUpdateSettings::kUpdatePolicyValue,
                            static_cast<DWORD>(2)));
  is_overridden = true;
  EXPECT_EQ(GoogleUpdateSettings::MANUAL_UPDATES_ONLY,
            GoogleUpdateSettings::GetAppUpdatePolicy(GetProductGuid(),
                                                     &is_overridden));
  EXPECT_FALSE(is_overridden);

  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, GoogleUpdateSettings::kPoliciesKey,
                   KEY_SET_VALUE)
                .WriteValue(GoogleUpdateSettings::kUpdatePolicyValue,
                            static_cast<DWORD>(3)));
  is_overridden = true;
  EXPECT_EQ(GoogleUpdateSettings::AUTO_UPDATES_ONLY,
            GoogleUpdateSettings::GetAppUpdatePolicy(GetProductGuid(),
                                                     &is_overridden));
  EXPECT_FALSE(is_overridden);

  // The default policy should be in force for bogus values.
  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, GoogleUpdateSettings::kPoliciesKey,
                   KEY_SET_VALUE)
                .WriteValue(GoogleUpdateSettings::kUpdatePolicyValue,
                            static_cast<DWORD>(4)));
  is_overridden = true;
  EXPECT_EQ(GoogleUpdateSettings::kDefaultUpdatePolicy,
            GoogleUpdateSettings::GetAppUpdatePolicy(GetProductGuid(),
                                                     &is_overridden));
  EXPECT_FALSE(is_overridden);
}

// Test that an app-specific override is used if present.
TEST_F(GoogleUpdateSettingsTest, GetAppUpdatePolicyAppOverride) {
  std::wstring app_policy_value(
      GoogleUpdateSettings::kUpdateOverrideValuePrefix);
  app_policy_value.append(GetProductGuid());

  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, GoogleUpdateSettings::kPoliciesKey,
                   KEY_SET_VALUE)
                .WriteValue(GoogleUpdateSettings::kUpdatePolicyValue,
                            static_cast<DWORD>(1)));
  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, GoogleUpdateSettings::kPoliciesKey,
                   KEY_SET_VALUE)
                .WriteValue(app_policy_value.c_str(), static_cast<DWORD>(0)));
  bool is_overridden = false;
  EXPECT_EQ(GoogleUpdateSettings::UPDATES_DISABLED,
            GoogleUpdateSettings::GetAppUpdatePolicy(GetProductGuid(),
                                                     &is_overridden));
  EXPECT_TRUE(is_overridden);

  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, GoogleUpdateSettings::kPoliciesKey,
                   KEY_SET_VALUE)
                .WriteValue(GoogleUpdateSettings::kUpdatePolicyValue,
                            static_cast<DWORD>(0)));
  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, GoogleUpdateSettings::kPoliciesKey,
                   KEY_SET_VALUE)
                .WriteValue(app_policy_value.c_str(), static_cast<DWORD>(1)));
  is_overridden = false;
  EXPECT_EQ(GoogleUpdateSettings::AUTOMATIC_UPDATES,
            GoogleUpdateSettings::GetAppUpdatePolicy(GetProductGuid(),
                                                     &is_overridden));
  EXPECT_TRUE(is_overridden);

  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, GoogleUpdateSettings::kPoliciesKey,
                   KEY_SET_VALUE)
                .WriteValue(app_policy_value.c_str(), static_cast<DWORD>(2)));
  is_overridden = false;
  EXPECT_EQ(GoogleUpdateSettings::MANUAL_UPDATES_ONLY,
            GoogleUpdateSettings::GetAppUpdatePolicy(GetProductGuid(),
                                                     &is_overridden));
  EXPECT_TRUE(is_overridden);

  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, GoogleUpdateSettings::kPoliciesKey,
                   KEY_SET_VALUE)
                .WriteValue(app_policy_value.c_str(), static_cast<DWORD>(3)));
  is_overridden = false;
  EXPECT_EQ(GoogleUpdateSettings::AUTO_UPDATES_ONLY,
            GoogleUpdateSettings::GetAppUpdatePolicy(GetProductGuid(),
                                                     &is_overridden));
  EXPECT_TRUE(is_overridden);

  // The default policy should be in force for bogus values.
  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(HKEY_LOCAL_MACHINE, GoogleUpdateSettings::kPoliciesKey,
                   KEY_SET_VALUE)
                .WriteValue(app_policy_value.c_str(), static_cast<DWORD>(4)));
  is_overridden = true;
  EXPECT_EQ(GoogleUpdateSettings::UPDATES_DISABLED,
            GoogleUpdateSettings::GetAppUpdatePolicy(GetProductGuid(),
                                                     &is_overridden));
  EXPECT_FALSE(is_overridden);
}

TEST_F(GoogleUpdateSettingsTest, PerAppUpdatesDisabledByPolicy) {
  const wchar_t* app_guid = install_static::GetAppGuid();
  EXPECT_TRUE(SetUpdatePolicyForAppGuid(
      app_guid, GoogleUpdateSettings::UPDATES_DISABLED));
  bool is_overridden = false;
  GoogleUpdateSettings::UpdatePolicy update_policy =
      GoogleUpdateSettings::GetAppUpdatePolicy(app_guid, &is_overridden);
  EXPECT_TRUE(is_overridden);
  EXPECT_EQ(GoogleUpdateSettings::UPDATES_DISABLED, update_policy);
  EXPECT_FALSE(GoogleUpdateSettings::AreAutoupdatesEnabled());

  EXPECT_TRUE(GoogleUpdateSettings::ReenableAutoupdates());
  update_policy =
      GoogleUpdateSettings::GetAppUpdatePolicy(app_guid, &is_overridden);
  // Should still have a policy but now that policy should explicitly enable
  // updates.
  EXPECT_TRUE(is_overridden);
  EXPECT_EQ(GoogleUpdateSettings::AUTOMATIC_UPDATES, update_policy);
  EXPECT_TRUE(GoogleUpdateSettings::AreAutoupdatesEnabled());
}

TEST_F(GoogleUpdateSettingsTest, PerAppUpdatesEnabledWithGlobalDisabled) {
  // Disable updates globally but enable them for Chrome (the app-specific
  // setting should take precedence).
  const wchar_t* app_guid = install_static::GetAppGuid();
  EXPECT_TRUE(SetUpdatePolicyForAppGuid(
      app_guid, GoogleUpdateSettings::AUTOMATIC_UPDATES));
  EXPECT_TRUE(SetGlobalUpdatePolicy(GoogleUpdateSettings::UPDATES_DISABLED));

  // Make sure we read this as still having updates enabled.
  EXPECT_TRUE(GoogleUpdateSettings::AreAutoupdatesEnabled());

  // Make sure that the reset action returns true and is a no-op.
  EXPECT_TRUE(GoogleUpdateSettings::ReenableAutoupdates());
  EXPECT_EQ(GoogleUpdateSettings::AUTOMATIC_UPDATES,
            GetUpdatePolicyForAppGuid(app_guid));
  EXPECT_EQ(GoogleUpdateSettings::UPDATES_DISABLED, GetGlobalUpdatePolicy());
}

TEST_F(GoogleUpdateSettingsTest, GlobalUpdatesDisabledByPolicy) {
  const wchar_t* app_guid = install_static::GetAppGuid();
  EXPECT_TRUE(SetGlobalUpdatePolicy(GoogleUpdateSettings::UPDATES_DISABLED));
  bool is_overridden = false;

  // The contract for GetAppUpdatePolicy states that |is_overridden| should be
  // set to false when updates are disabled on a non-app-specific basis.
  GoogleUpdateSettings::UpdatePolicy update_policy =
      GoogleUpdateSettings::GetAppUpdatePolicy(app_guid, &is_overridden);
  EXPECT_FALSE(is_overridden);
  EXPECT_EQ(GoogleUpdateSettings::UPDATES_DISABLED, update_policy);
  EXPECT_FALSE(GoogleUpdateSettings::AreAutoupdatesEnabled());

  EXPECT_TRUE(GoogleUpdateSettings::ReenableAutoupdates());
  update_policy =
      GoogleUpdateSettings::GetAppUpdatePolicy(app_guid, &is_overridden);
  // Policy should now be to enable updates, |is_overridden| should still be
  // false.
  EXPECT_FALSE(is_overridden);
  EXPECT_EQ(GoogleUpdateSettings::AUTOMATIC_UPDATES, update_policy);
  EXPECT_TRUE(GoogleUpdateSettings::AreAutoupdatesEnabled());
}

TEST_F(GoogleUpdateSettingsTest, UpdatesDisabledByTimeout) {
  // Disable updates altogether.
  EXPECT_TRUE(SetUpdateTimeoutOverride(0));
  EXPECT_FALSE(GoogleUpdateSettings::AreAutoupdatesEnabled());
  EXPECT_TRUE(GoogleUpdateSettings::ReenableAutoupdates());
  EXPECT_TRUE(GoogleUpdateSettings::AreAutoupdatesEnabled());

  // Set the update period to something unreasonable.
  EXPECT_TRUE(SetUpdateTimeoutOverride(
      GoogleUpdateSettings::kCheckPeriodOverrideMinutesMax + 1));
  EXPECT_FALSE(GoogleUpdateSettings::AreAutoupdatesEnabled());
  EXPECT_TRUE(GoogleUpdateSettings::ReenableAutoupdates());
  EXPECT_TRUE(GoogleUpdateSettings::AreAutoupdatesEnabled());
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

TEST_F(GoogleUpdateSettingsTest, GetDownloadPreference) {
  RegKey policy_key;

  if (policy_key.Open(HKEY_LOCAL_MACHINE, GoogleUpdateSettings::kPoliciesKey,
                      KEY_SET_VALUE) == ERROR_SUCCESS) {
    policy_key.DeleteValue(
        GoogleUpdateSettings::kDownloadPreferencePolicyValue);
  }
  policy_key.Close();

  // When no policy is present expect to return an empty string.
  EXPECT_TRUE(GoogleUpdateSettings::GetDownloadPreference().empty());

  // Expect "cacheable" when the correct policy is present.
  EXPECT_EQ(ERROR_SUCCESS, policy_key.Create(HKEY_LOCAL_MACHINE,
                                             GoogleUpdateSettings::kPoliciesKey,
                                             KEY_SET_VALUE));
  EXPECT_EQ(
      ERROR_SUCCESS,
      policy_key.WriteValue(
          GoogleUpdateSettings::kDownloadPreferencePolicyValue, L"cacheable"));
  EXPECT_STREQ(L"cacheable",
               GoogleUpdateSettings::GetDownloadPreference().c_str());

  EXPECT_EQ(ERROR_SUCCESS,
            policy_key.WriteValue(
                GoogleUpdateSettings::kDownloadPreferencePolicyValue,
                std::wstring(32, L'a').c_str()));
  EXPECT_STREQ(std::wstring(32, L'a').c_str(),
               GoogleUpdateSettings::GetDownloadPreference().c_str());

  // Expect an empty string when an unsupported policy is set.
  // It contains spaces.
  EXPECT_EQ(ERROR_SUCCESS,
            policy_key.WriteValue(
                GoogleUpdateSettings::kDownloadPreferencePolicyValue, L"a b"));
  EXPECT_TRUE(GoogleUpdateSettings::GetDownloadPreference().empty());

  // It contains non alphanumeric characters.
  EXPECT_EQ(ERROR_SUCCESS,
            policy_key.WriteValue(
                GoogleUpdateSettings::kDownloadPreferencePolicyValue, L"<a>"));
  EXPECT_TRUE(GoogleUpdateSettings::GetDownloadPreference().empty());

  // It is too long.
  EXPECT_EQ(ERROR_SUCCESS,
            policy_key.WriteValue(
                GoogleUpdateSettings::kDownloadPreferencePolicyValue,
                std::wstring(33, L'a').c_str()));
  EXPECT_TRUE(GoogleUpdateSettings::GetDownloadPreference().empty());
}

class SetProgressTest : public GoogleUpdateSettingsTest,
                        public testing::WithParamInterface<bool> {
 protected:
  SetProgressTest()
      : system_install_(GetParam()),
        root_key_(system_install_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER) {}

  const bool system_install_;
  const HKEY root_key_;
};

TEST_P(SetProgressTest, SetProgress) {
  std::wstring path(google_update::kRegPathClientState);
  path += L"\\";
  path += GetProductGuid();

  constexpr int kValues[] = {0, 25, 50, 99, 100};
  for (int value : kValues) {
    GoogleUpdateSettings::SetProgress(system_install_, path, value);
    DWORD progress = 0;
    base::win::RegKey key(root_key_, path.c_str(),
                          KEY_QUERY_VALUE | KEY_WOW64_32KEY);
    ASSERT_TRUE(key.Valid());
    ASSERT_EQ(ERROR_SUCCESS,
              key.ReadValueDW(google_update::kRegInstallerProgress, &progress));
    EXPECT_EQ(static_cast<DWORD>(value), progress);
  }
}

INSTANTIATE_TEST_SUITE_P(SetProgressUserLevel,
                         SetProgressTest,
                         testing::Values(false));
INSTANTIATE_TEST_SUITE_P(SetProgressSystemLevel,
                         SetProgressTest,
                         testing::Values(true));

// Test GoogleUpdateSettings::GetUninstallCommandLine at system- or user-level,
// according to the param.
class GetUninstallCommandLine : public GoogleUpdateSettingsTest,
                                public testing::WithParamInterface<bool> {
 protected:
  static const wchar_t kDummyCommand[];

  void SetUp() override {
    GoogleUpdateSettingsTest::SetUp();
    system_install_ = GetParam();
    root_key_ = system_install_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  }

  HKEY root_key_;
  bool system_install_;
};

const wchar_t GetUninstallCommandLine::kDummyCommand[] =
    L"\"goopdate.exe\" /spam";

// Tests that GetUninstallCommandLine returns an empty string if there's no
// Software\Google\Update key.
TEST_P(GetUninstallCommandLine, TestNoKey) {
  EXPECT_EQ(std::wstring(),
            GoogleUpdateSettings::GetUninstallCommandLine(system_install_));
}

// Tests that GetUninstallCommandLine returns an empty string if there's no
// UninstallCmdLine value in the Software\Google\Update key.
TEST_P(GetUninstallCommandLine, TestNoValue) {
  RegKey(root_key_, google_update::kRegPathGoogleUpdate, KEY_SET_VALUE);
  EXPECT_EQ(std::wstring(),
            GoogleUpdateSettings::GetUninstallCommandLine(system_install_));
}

// Tests that GetUninstallCommandLine returns an empty string if there's an
// empty UninstallCmdLine value in the Software\Google\Update key.
TEST_P(GetUninstallCommandLine, TestEmptyValue) {
  RegKey(root_key_, google_update::kRegPathGoogleUpdate, KEY_SET_VALUE)
      .WriteValue(google_update::kRegUninstallCmdLine, L"");
  EXPECT_EQ(std::wstring(),
            GoogleUpdateSettings::GetUninstallCommandLine(system_install_));
}

// Tests that GetUninstallCommandLine returns the correct string if there's an
// UninstallCmdLine value in the Software\Google\Update key.
TEST_P(GetUninstallCommandLine, TestRealValue) {
  RegKey(root_key_, google_update::kRegPathGoogleUpdate, KEY_SET_VALUE)
      .WriteValue(google_update::kRegUninstallCmdLine, kDummyCommand);
  EXPECT_EQ(std::wstring(kDummyCommand),
            GoogleUpdateSettings::GetUninstallCommandLine(system_install_));
  // Make sure that there's no value in the other level (user or system).
  EXPECT_EQ(std::wstring(),
            GoogleUpdateSettings::GetUninstallCommandLine(!system_install_));
}

INSTANTIATE_TEST_SUITE_P(GetUninstallCommandLineAtLevel,
                         GetUninstallCommandLine,
                         testing::Bool());

// Test GoogleUpdateSettings::GetGoogleUpdateVersion at system- or user-level,
// according to the param.
class GetGoogleUpdateVersion : public GoogleUpdateSettingsTest,
                               public testing::WithParamInterface<bool> {
 protected:
  static const wchar_t kDummyVersion[];

  void SetUp() override {
    GoogleUpdateSettingsTest::SetUp();
    system_install_ = GetParam();
    root_key_ = system_install_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  }

  HKEY root_key_;
  bool system_install_;
};

const wchar_t GetGoogleUpdateVersion::kDummyVersion[] = L"1.2.3.4";

// Tests that GetGoogleUpdateVersion returns an empty string if there's no
// Software\Google\Update key.
TEST_P(GetGoogleUpdateVersion, TestNoKey) {
  EXPECT_FALSE(
      GoogleUpdateSettings::GetGoogleUpdateVersion(system_install_).IsValid());
}

// Tests that GetGoogleUpdateVersion returns an empty string if there's no
// version value in the Software\Google\Update key.
TEST_P(GetGoogleUpdateVersion, TestNoValue) {
  RegKey(root_key_, google_update::kRegPathGoogleUpdate, KEY_SET_VALUE);
  EXPECT_FALSE(
      GoogleUpdateSettings::GetGoogleUpdateVersion(system_install_).IsValid());
}

// Tests that GetGoogleUpdateVersion returns an empty string if there's an
// empty version value in the Software\Google\Update key.
TEST_P(GetGoogleUpdateVersion, TestEmptyValue) {
  RegKey(root_key_, google_update::kRegPathGoogleUpdate, KEY_SET_VALUE)
      .WriteValue(google_update::kRegGoogleUpdateVersion, L"");
  EXPECT_FALSE(
      GoogleUpdateSettings::GetGoogleUpdateVersion(system_install_).IsValid());
}

// Tests that GetGoogleUpdateVersion returns the correct string if there's a
// version value in the Software\Google\Update key.
TEST_P(GetGoogleUpdateVersion, TestRealValue) {
  RegKey(root_key_, google_update::kRegPathGoogleUpdate, KEY_SET_VALUE)
      .WriteValue(google_update::kRegGoogleUpdateVersion, kDummyVersion);
  base::Version expected(base::WideToASCII(kDummyVersion));
  EXPECT_EQ(expected,
            GoogleUpdateSettings::GetGoogleUpdateVersion(system_install_));
  // Make sure that there's no value in the other level (user or system).
  EXPECT_FALSE(
      GoogleUpdateSettings::GetGoogleUpdateVersion(!system_install_).IsValid());
}

INSTANTIATE_TEST_SUITE_P(GetGoogleUpdateVersionAtLevel,
                         GetGoogleUpdateVersion,
                         testing::Bool());

// Tests that GetHashedCohortId returns an empty optional if there's no cohort
// key.
TEST_F(GoogleUpdateSettingsTest, GetHashedCohortIdTestNoKey) {
  EXPECT_FALSE(GoogleUpdateSettings::GetHashedCohortId());
}

// Tests that GetHashedCohortId returns an empty optional if there's no "id"
// value in the cohort key.
TEST_F(GoogleUpdateSettingsTest, GetHashedCohortIdTestNoValue) {
  RegKey(install_static::InstallDetails::Get().system_level()
             ? HKEY_LOCAL_MACHINE
             : HKEY_CURRENT_USER,
         install_static::GetClientStateKeyPath(
             install_static::InstallDetails::Get().app_guid())
             .append(L"\\cohort")
             .c_str(),
         KEY_SET_VALUE);
  EXPECT_FALSE(GoogleUpdateSettings::GetHashedCohortId());
}

TEST_F(GoogleUpdateSettingsTest, GetHashedCohortIdTestEmptyValue) {
  RegKey(install_static::InstallDetails::Get().system_level()
             ? HKEY_LOCAL_MACHINE
             : HKEY_CURRENT_USER,
         install_static::GetClientStateKeyPath(
             install_static::InstallDetails::Get().app_guid())
             .append(L"\\cohort")
             .c_str(),
         KEY_SET_VALUE)
      .WriteValue(google_update::kRegDefaultField, L"");
  EXPECT_FALSE(GoogleUpdateSettings::GetHashedCohortId());
}

TEST_F(GoogleUpdateSettingsTest, GetHashedCohortIdTestRealValue) {
  RegKey(install_static::InstallDetails::Get().system_level()
             ? HKEY_LOCAL_MACHINE
             : HKEY_CURRENT_USER,
         install_static::GetClientStateKeyPath(
             install_static::InstallDetails::Get().app_guid())
             .append(L"\\cohort")
             .c_str(),
         KEY_SET_VALUE)
      .WriteValue(google_update::kRegDefaultField, L"1:qesc2/qesff:qesee@0.5");
  EXPECT_TRUE(GoogleUpdateSettings::GetHashedCohortId());
  EXPECT_EQ(*GoogleUpdateSettings::GetHashedCohortId(),
            base::PersistentHash("1:qesc2/qesff"));
}

// Test values for use by the CollectStatsConsent test fixture.
class StatsState {
 public:
  enum StateSetting {
    NO_SETTING,
    FALSE_SETTING,
    TRUE_SETTING,
  };
  struct UserLevelState {};
  struct SystemLevelState {};
  static const UserLevelState kUserLevel;
  static const SystemLevelState kSystemLevel;

  StatsState(const UserLevelState&, StateSetting state_value)
      : system_level_(false),
        state_value_(state_value),
        state_medium_value_(NO_SETTING) {}
  StatsState(const SystemLevelState&,
             StateSetting state_value,
             StateSetting state_medium_value)
      : system_level_(true),
        state_value_(state_value),
        state_medium_value_(state_medium_value) {}
  bool system_level() const { return system_level_; }
  HKEY root_key() const {
    return system_level_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  }
  StateSetting state_value() const { return state_value_; }
  StateSetting state_medium_value() const { return state_medium_value_; }
  bool is_consent_granted() const {
    return (system_level_ && state_medium_value_ != NO_SETTING)
               ? (state_medium_value_ == TRUE_SETTING)
               : (state_value_ == TRUE_SETTING);
  }

 private:
  bool system_level_;
  StateSetting state_value_;
  StateSetting state_medium_value_;
};

const StatsState::UserLevelState StatsState::kUserLevel = {};
const StatsState::SystemLevelState StatsState::kSystemLevel = {};

// A value parameterized test for testing the stats collection consent setting.
class CollectStatsConsent : public ::testing::TestWithParam<StatsState> {
 protected:
  CollectStatsConsent();
  void SetUp() override;
  void ApplySetting(StatsState::StateSetting setting,
                    HKEY root_key,
                    const std::wstring& reg_key);

  registry_util::RegistryOverrideManager override_manager_;
  std::unique_ptr<install_static::ScopedInstallDetails> scoped_install_details_;
};

CollectStatsConsent::CollectStatsConsent() = default;

// Install the registry override and apply the settings to the registry.
void CollectStatsConsent::SetUp() {
  // Override both HKLM and HKCU as tests may touch either/both.
  ASSERT_NO_FATAL_FAILURE(
      override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
  ASSERT_NO_FATAL_FAILURE(
      override_manager_.OverrideRegistry(HKEY_CURRENT_USER));

  const StatsState& stats_state = GetParam();
  scoped_install_details_ =
      std::make_unique<install_static::ScopedInstallDetails>(
          stats_state.system_level(), 0 /* install_mode_index */);
  const HKEY root_key = stats_state.root_key();
  ASSERT_NO_FATAL_FAILURE(
      ApplySetting(stats_state.state_value(), root_key,
                   install_static::GetClientStateKeyPath()));
  ASSERT_NO_FATAL_FAILURE(
      ApplySetting(stats_state.state_medium_value(), root_key,
                   install_static::GetClientStateMediumKeyPath()));
}

// Write the correct value to represent |setting| in the registry.
void CollectStatsConsent::ApplySetting(StatsState::StateSetting setting,
                                       HKEY root_key,
                                       const std::wstring& reg_key) {
  if (setting != StatsState::NO_SETTING) {
    DWORD value = setting != StatsState::FALSE_SETTING ? 1 : 0;
    ASSERT_EQ(ERROR_SUCCESS,
              RegKey(root_key, reg_key.c_str(), KEY_SET_VALUE)
                  .WriteValue(google_update::kRegUsageStatsField, value));
  }
}

// Test that stats consent can be read.
TEST_P(CollectStatsConsent, GetCollectStatsConsent) {
  if (GetParam().is_consent_granted())
    EXPECT_TRUE(GoogleUpdateSettings::GetCollectStatsConsent());
  else
    EXPECT_FALSE(GoogleUpdateSettings::GetCollectStatsConsent());
}

// Test that stats consent can be flipped to the opposite setting, that the new
// setting takes affect, and that the correct registry location is modified.
TEST_P(CollectStatsConsent, SetCollectStatsConsent) {
  // When testing revoking consent, verify that backup client info is cleared.
  // To do so, first add some backup client info.
  if (GetParam().is_consent_granted()) {
    metrics::ClientInfo client_info;
    client_info.client_id = "01234567-89ab-cdef-fedc-ba9876543210";
    client_info.installation_date = 123;
    client_info.reporting_enabled_date = 345;
    GoogleUpdateSettings::StoreMetricsClientInfo(client_info);
  }

  EXPECT_TRUE(GoogleUpdateSettings::SetCollectStatsConsent(
      !GetParam().is_consent_granted()));

  const std::wstring reg_key =
      GetParam().system_level() ? install_static::GetClientStateMediumKeyPath()
                                : install_static::GetClientStateKeyPath();
  DWORD value = 0;
  EXPECT_EQ(ERROR_SUCCESS,
            RegKey(GetParam().root_key(), reg_key.c_str(), KEY_QUERY_VALUE)
                .ReadValueDW(google_update::kRegUsageStatsField, &value));
  if (GetParam().is_consent_granted()) {
    EXPECT_FALSE(GoogleUpdateSettings::GetCollectStatsConsent());
    EXPECT_EQ(0UL, value);
  } else {
    EXPECT_TRUE(GoogleUpdateSettings::GetCollectStatsConsent());
    EXPECT_EQ(1UL, value);
    // Verify that backup client info has been cleared.
    EXPECT_FALSE(GoogleUpdateSettings::LoadMetricsClientInfo());
  }
}

INSTANTIATE_TEST_SUITE_P(
    UserLevel,
    CollectStatsConsent,
    ::testing::Values(
        StatsState(StatsState::kUserLevel, StatsState::NO_SETTING),
        StatsState(StatsState::kUserLevel, StatsState::FALSE_SETTING),
        StatsState(StatsState::kUserLevel, StatsState::TRUE_SETTING)));
INSTANTIATE_TEST_SUITE_P(
    SystemLevel,
    CollectStatsConsent,
    ::testing::Values(StatsState(StatsState::kSystemLevel,
                                 StatsState::NO_SETTING,
                                 StatsState::NO_SETTING),
                      StatsState(StatsState::kSystemLevel,
                                 StatsState::NO_SETTING,
                                 StatsState::FALSE_SETTING),
                      StatsState(StatsState::kSystemLevel,
                                 StatsState::NO_SETTING,
                                 StatsState::TRUE_SETTING),
                      StatsState(StatsState::kSystemLevel,
                                 StatsState::FALSE_SETTING,
                                 StatsState::NO_SETTING),
                      StatsState(StatsState::kSystemLevel,
                                 StatsState::FALSE_SETTING,
                                 StatsState::FALSE_SETTING),
                      StatsState(StatsState::kSystemLevel,
                                 StatsState::FALSE_SETTING,
                                 StatsState::TRUE_SETTING),
                      StatsState(StatsState::kSystemLevel,
                                 StatsState::TRUE_SETTING,
                                 StatsState::NO_SETTING),
                      StatsState(StatsState::kSystemLevel,
                                 StatsState::TRUE_SETTING,
                                 StatsState::FALSE_SETTING),
                      StatsState(StatsState::kSystemLevel,
                                 StatsState::TRUE_SETTING,
                                 StatsState::TRUE_SETTING)));
