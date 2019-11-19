// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/pup_data/pup_data.h"

#include <shlobj.h>

#include <algorithm>
#include <deque>
#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/proto/shared_pup_enums.pb.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_pup_data.h"
#include "chrome/chrome_cleaner/test/test_registry_util.h"
#include "chrome/chrome_cleaner/test/test_uws_catalog.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

const UwSId k42ID = 42;
const char k42Name[] = "Observed/42";
const int k42CSIDL[] = {0, 5, 25};
const wchar_t k42AbsoluteDiskPath[] = L"c:\\Windows\\system32\\42";
const wchar_t k42RelativeDiskPath1[] = L"Anywhere\\42";
const wchar_t k42RelativeDiskPath2[] = L"42";
const wchar_t k42RegistryKeyPath[] = L"Software\\42";

const UwSId k24ID = 24;
const char k24Name[] = "Removed/24";
const int k24CSIDL = 17;
const wchar_t k24DiskPath[] = L"c:\\Program Files (x86)\\24";
const wchar_t k24RegistryKeyPath[] = L"Software\\24";
const wchar_t k24RegistryValueName[] = L"ValueName";
const wchar_t k24RegistryValue[] = L"Value";

const UwSId k12ID = 12;
const char k12Name[] = "12";
const int k12CSIDL = 42;
const wchar_t k12DiskPath[] = L"c:\\Program Files\\12";
const wchar_t k12RegistryKeyPath1[] = L"Somewhere\\12";
const wchar_t k12RegistryKeyPath2[] = L"Elsewhere\\12";
const wchar_t k12RegistryValueName1[] = L"SomeName";
const wchar_t k12RegistryValueName2[] = L"OtherName";
const wchar_t k12RegistryValueSubstring[] = L"SubSet";

const wchar_t kScheduledTaskName[] = L"PUPData Dummy Task";

const wchar_t kGroupPolicyPath[] = L"%SystemRoot%\\system32\\GroupPolicy";
const wchar_t kMachinePolicyFolder[] = L"Machine";
const wchar_t kUserPolicyFolder[] = L"User";

}  // namespace

class PUPDataTest : public testing::Test {
 public:
  PUPDataTest() = default;

  // Will return silently if |footprint| can be found into |footprints| as
  // expected. Will ADD_FAILURE otherwise.
  void ExpectStaticDiskFootprint(
      const PUPData::StaticDiskFootprint* footprints,
      const PUPData::StaticDiskFootprint& footprint) {
    ASSERT_NE(nullptr, footprints);
    for (size_t i = 0; footprints[i].path != nullptr; ++i) {
      if (footprint.csidl == footprints[i].csidl &&
          base::FilePath::CompareEqualIgnoreCase(footprints[i].path,
                                                 footprint.path)) {
        return;
      }
    }
    ADD_FAILURE() << "Can't find disk footprint: " << footprint.path;
  }

  // Will return silently if |footprint| can be found into |footprints| as
  // expected. Will ADD_FAILURE otherwise.
  void ExpectStaticRegistryFootprint(
      const PUPData::StaticRegistryFootprint* footprints,
      const PUPData::StaticRegistryFootprint& footprint) {
    ASSERT_NE(nullptr, footprints);
    for (size_t i = 0; footprints[i].key_path != nullptr; ++i) {
      if (RegistryFootprintMatch(footprints[i], footprint))
        return;
    }
    ADD_FAILURE() << "Can't find registry footprint: (" << footprint.key_path
                  << ", " << footprint.value_name << ", "
                  << footprint.value_substring << ")";
  }

  // Verify that all values in |pup_data_| are the same as the ones in
  // |test_data_|.
  void ExpectAllValues() {
    for (const auto& pup_id : *PUPData::GetUwSIds()) {
      const PUPData::UwSSignature& signature =
          PUPData::GetPUP(pup_id)->signature();
      const TestPUPData::CPPPUP& cpp_footprint =
          test_data_.cpp_pup_footprints().at(signature.id);
      // The last element of all the arrays are nullptr terminating entries.
      for (size_t j = 0; j < cpp_footprint.disk_footprints.size() - 1; ++j) {
        ExpectStaticDiskFootprint(signature.disk_footprints,
                                  cpp_footprint.disk_footprints[j]);
      }
      for (size_t j = 0; j < cpp_footprint.registry_footprints.size() - 1;
           ++j) {
        ExpectStaticRegistryFootprint(signature.registry_footprints,
                                      cpp_footprint.registry_footprints[j]);
      }
      // And there shouldn't be anything else.
      EXPECT_EQ(cpp_footprint.disk_footprints.size() - 1,
                DiskFootprintsSize(signature.disk_footprints));
      EXPECT_EQ(cpp_footprint.registry_footprints.size() - 1,
                RegistryFootprintsSize(signature.registry_footprints));
    }
  }

  // Wrapper to test flag chooser.
  bool ValidateFlags(bool (*chooser)(PUPData::Flags)) {
    return pup_data().HasFlaggedPUP(*PUPData::GetUwSIds(), chooser);
  }

  // Make sure |pup_data_| has |num_entries| entries in it.
  void ExpectNumPUPs(size_t num_entries) {
    EXPECT_EQ(num_entries, pup_data_.GetUwSIds()->size());
  }

  PUPData& pup_data() { return pup_data_; }

  TestPUPData& test_data() { return test_data_; }

  // Return true if |footprint1| and |footprint2| match the same registry info.
  bool RegistryFootprintMatch(
      const PUPData::StaticRegistryFootprint& footprint1,
      const PUPData::StaticRegistryFootprint& footprint2) {
    if (footprint1.registry_root != footprint2.registry_root)
      return false;
    // None should have a nullptr |key_path|.
    if (!footprint1.key_path || !footprint2.key_path)
      return false;
    if (footprint1.rule != footprint2.rule)
      return false;
    if (!base::FilePath::CompareEqualIgnoreCase(footprint1.key_path,
                                                footprint2.key_path)) {
      return false;
    }
    // If one has a nullptr |value_name|, the other one should too.
    if (footprint1.value_name == nullptr && footprint2.value_name == nullptr)
      return true;
    if (footprint1.value_name == nullptr || footprint2.value_name == nullptr)
      return false;
    // If one has a nullptr |value_substring|, the other one should too.
    if ((footprint1.value_substring != nullptr ||
         footprint2.value_substring != nullptr) &&
        (footprint1.value_substring == nullptr ||
         footprint2.value_substring == nullptr)) {
      return false;
    }
    // With a non-nullptr |value_name| and an equally nullptr or not
    // |value_substring| the |value_name| values must match.
    if (!base::FilePath::CompareEqualIgnoreCase(footprint1.value_name,
                                                footprint2.value_name)) {
      return false;
    }
    if (footprint1.value_substring == nullptr &&
        footprint2.value_substring == nullptr) {
      return true;
    }
    if (!base::FilePath::CompareEqualIgnoreCase(footprint1.value_substring,
                                                footprint2.value_substring)) {
      return false;
    }
    return true;
  }

 private:
  // Compute the size of a StaticDiskFootprint C Array.
  size_t DiskFootprintsSize(const PUPData::StaticDiskFootprint* footprints) {
    size_t i = 0;
    for (; footprints[i].path != nullptr; ++i) {
    }
    return i;
  }

  // Same thing for a StaticRegistryFootprint C Array.
  size_t RegistryFootprintsSize(
      const PUPData::StaticRegistryFootprint* footprints) {
    size_t i = 0;
    for (; footprints[i].key_path != nullptr; ++i) {
    }
    return i;
  }

  PUPData pup_data_;
  TestPUPData test_data_;
};

TEST_F(PUPDataTest, OnePUP) {
  test_data().AddPUP(
      k42ID, PUPData::FLAGS_NONE, k42Name, PUPData::kMaxFilesToRemoveSmallUwS);

  const PUPData::UwSSignature& signature = PUPData::GetPUP(k42ID)->signature();
  EXPECT_STREQ(k42Name, signature.name);
}

TEST_F(PUPDataTest, MultiPUPs) {
  test_data().AddPUP(
      k42ID, PUPData::FLAGS_NONE, k42Name, PUPData::kMaxFilesToRemoveSmallUwS);
  test_data().AddPUP(
      k24ID, PUPData::FLAGS_NONE, k24Name, PUPData::kMaxFilesToRemoveSmallUwS);
  test_data().AddPUP(
      k12ID, PUPData::FLAGS_NONE, k12Name, PUPData::kMaxFilesToRemoveSmallUwS);

  const PUPData::UwSSignature& signature12 =
      PUPData::GetPUP(k12ID)->signature();
  EXPECT_STREQ(k12Name, signature12.name);

  const PUPData::UwSSignature& signature24 =
      PUPData::GetPUP(k24ID)->signature();
  EXPECT_STREQ(k24Name, signature24.name);

  const PUPData::UwSSignature& signature42 =
      PUPData::GetPUP(k42ID)->signature();
  EXPECT_STREQ(k42Name, signature42.name);
}

TEST_F(PUPDataTest, MultiDisks) {
  test_data().AddDiskFootprint(k42ID, k42CSIDL[2], k42AbsoluteDiskPath,
                               PUPData::DISK_MATCH_ANY_FILE);
  test_data().AddDiskFootprint(k42ID, k42CSIDL[1], k42RelativeDiskPath2,
                               PUPData::DISK_MATCH_ANY_FILE);
  test_data().AddDiskFootprint(k42ID, k42CSIDL[0], k42RelativeDiskPath1,
                               PUPData::DISK_MATCH_ANY_FILE);

  ExpectNumPUPs(1);
  ExpectAllValues();

  test_data().AddDiskFootprint(k24ID, k24CSIDL, k24DiskPath,
                               PUPData::DISK_MATCH_ANY_FILE);
  ExpectNumPUPs(2);
  ExpectAllValues();

  test_data().AddDiskFootprint(k12ID, k12CSIDL, k12DiskPath,
                               PUPData::DISK_MATCH_ANY_FILE);
  ExpectNumPUPs(3);
  ExpectAllValues();
}

TEST_F(PUPDataTest, OneRegistry) {
  test_data().AddRegistryFootprint(k42ID,
                                   REGISTRY_ROOT_USERS,
                                   k42RegistryKeyPath,
                                   nullptr,
                                   nullptr,
                                   REGISTRY_VALUE_MATCH_KEY);

  ExpectNumPUPs(1);

  PUPData::StaticRegistryFootprint footprint = {};
  footprint.registry_root = REGISTRY_ROOT_USERS;
  footprint.key_path = k42RegistryKeyPath;
  ExpectAllValues();
}

TEST_F(PUPDataTest, MultiRegistry) {
  test_data().AddRegistryFootprint(k42ID,
                                   REGISTRY_ROOT_USERS,
                                   k42RegistryKeyPath,
                                   nullptr,
                                   nullptr,
                                   REGISTRY_VALUE_MATCH_KEY);
  test_data().AddRegistryFootprint(k24ID,
                                   REGISTRY_ROOT_LOCAL_MACHINE,
                                   k24RegistryKeyPath,
                                   k24RegistryValueName,
                                   nullptr,
                                   REGISTRY_VALUE_MATCH_VALUE_NAME);
  test_data().AddRegistryFootprint(k12ID,
                                   REGISTRY_ROOT_MACHINE_GROUP_POLICY,
                                   k12RegistryKeyPath1,
                                   k12RegistryValueName1,
                                   k12RegistryValueSubstring,
                                   REGISTRY_VALUE_MATCH_EXACT);
  ExpectNumPUPs(3);
  ExpectAllValues();
}

TEST_F(PUPDataTest, AllFootprintsAndActions) {
  test_data().AddDiskFootprint(k42ID, k42CSIDL[0], k42AbsoluteDiskPath,
                               PUPData::DISK_MATCH_ANY_FILE);
  test_data().AddDiskFootprint(k42ID, k42CSIDL[1], k42RelativeDiskPath1,
                               PUPData::DISK_MATCH_ANY_FILE);
  test_data().AddDiskFootprint(k42ID, k42CSIDL[2], k42RelativeDiskPath2,
                               PUPData::DISK_MATCH_ANY_FILE);

  test_data().AddRegistryFootprint(k42ID,
                                   REGISTRY_ROOT_CLASSES,
                                   k42RegistryKeyPath,
                                   nullptr,
                                   nullptr,
                                   REGISTRY_VALUE_MATCH_KEY);
  test_data().AddRegistryFootprint(k24ID,
                                   REGISTRY_ROOT_CLASSES,
                                   k24RegistryKeyPath,
                                   k24RegistryValueName,
                                   nullptr,
                                   REGISTRY_VALUE_MATCH_VALUE_NAME);
  test_data().AddRegistryFootprint(k12ID, REGISTRY_ROOT_USERS_GROUP_POLICY,
                                   k12RegistryKeyPath1, k12RegistryValueName1,
                                   k12RegistryValueSubstring,
                                   REGISTRY_VALUE_MATCH_CONTAINS);

  ExpectNumPUPs(3);
  ExpectAllValues();

  test_data().AddDiskFootprint(k12ID, k12CSIDL, k12DiskPath,
                               PUPData::DISK_MATCH_ANY_FILE);
  test_data().AddDiskFootprint(k24ID, k24CSIDL, k24DiskPath,
                               PUPData::DISK_MATCH_ANY_FILE);
  test_data().AddDiskFootprint(k42ID, k42CSIDL[0], k42RelativeDiskPath1,
                               PUPData::DISK_MATCH_ANY_FILE);

  test_data().AddRegistryFootprint(k12ID,
                                   REGISTRY_ROOT_USERS,
                                   k12RegistryKeyPath2,
                                   k12RegistryValueName2,
                                   nullptr,
                                   REGISTRY_VALUE_MATCH_VALUE_NAME);
  test_data().AddRegistryFootprint(k12ID,
                                   REGISTRY_ROOT_USERS,
                                   k12RegistryKeyPath2,
                                   nullptr,
                                   nullptr,
                                   REGISTRY_VALUE_MATCH_KEY);

  ExpectNumPUPs(3);
  ExpectAllValues();
}

TEST_F(PUPDataTest, GetRootKeyFromRegistryRoot) {
  HKEY hkey;
  base::FilePath policy;
  EXPECT_TRUE(PUPData::GetRootKeyFromRegistryRoot(REGISTRY_ROOT_LOCAL_MACHINE,
                                                  &hkey, &policy));
  EXPECT_EQ(HKEY_LOCAL_MACHINE, hkey);

  EXPECT_TRUE(PUPData::GetRootKeyFromRegistryRoot(REGISTRY_ROOT_CLASSES, &hkey,
                                                  &policy));
  EXPECT_EQ(HKEY_CLASSES_ROOT, hkey);

  EXPECT_TRUE(
      PUPData::GetRootKeyFromRegistryRoot(REGISTRY_ROOT_USERS, &hkey, &policy));
  EXPECT_EQ(HKEY_CURRENT_USER, hkey);

  EXPECT_TRUE(PUPData::GetRootKeyFromRegistryRoot(
      REGISTRY_ROOT_MACHINE_GROUP_POLICY, &hkey, &policy));
  EXPECT_EQ(HKEY_LOCAL_MACHINE, hkey);
  EXPECT_EQ(base::FilePath(kGroupPolicyPath).Append(kMachinePolicyFolder),
            policy);

  EXPECT_TRUE(PUPData::GetRootKeyFromRegistryRoot(
      REGISTRY_ROOT_USERS_GROUP_POLICY, &hkey, &policy));
  EXPECT_EQ(HKEY_CURRENT_USER, hkey);
  EXPECT_EQ(base::FilePath(kGroupPolicyPath).Append(kUserPolicyFolder), policy);

  // Test that policy parameter can be nullptr.
  EXPECT_TRUE(PUPData::GetRootKeyFromRegistryRoot(
      REGISTRY_ROOT_MACHINE_GROUP_POLICY, &hkey, nullptr));
  EXPECT_EQ(HKEY_LOCAL_MACHINE, hkey);

  EXPECT_TRUE(PUPData::GetRootKeyFromRegistryRoot(
      REGISTRY_ROOT_USERS_GROUP_POLICY, &hkey, nullptr));
  EXPECT_EQ(HKEY_CURRENT_USER, hkey);

  // Test the returned value of an invalid key root.
  EXPECT_FALSE(PUPData::GetRootKeyFromRegistryRoot(REGISTRY_ROOT_INVALID, &hkey,
                                                   nullptr));
}

TEST_F(PUPDataTest, HasReportOnlyFlag) {
  EXPECT_TRUE(PUPData::HasReportOnlyFlag(PUPData::FLAGS_REMOVAL_FORCE_REBOOT));
  EXPECT_TRUE(PUPData::HasReportOnlyFlag(PUPData::FLAGS_NONE));

  EXPECT_FALSE(PUPData::HasReportOnlyFlag(PUPData::FLAGS_ACTION_REMOVE));
  EXPECT_FALSE(PUPData::HasReportOnlyFlag(PUPData::FLAGS_ACTION_REMOVE |
                                          PUPData::FLAGS_REMOVAL_FORCE_REBOOT));
}

TEST_F(PUPDataTest, HasRemovalFlag) {
  EXPECT_TRUE(PUPData::HasRemovalFlag(PUPData::FLAGS_ACTION_REMOVE));
  EXPECT_TRUE(PUPData::HasRemovalFlag(PUPData::FLAGS_ACTION_REMOVE |
                                      PUPData::FLAGS_REMOVAL_FORCE_REBOOT));
  EXPECT_FALSE(PUPData::HasRemovalFlag(PUPData::FLAGS_REMOVAL_FORCE_REBOOT));
  EXPECT_FALSE(PUPData::HasRemovalFlag(PUPData::FLAGS_NONE));
}

TEST_F(PUPDataTest, HasConfirmedUwSFlag) {
  EXPECT_FALSE(PUPData::HasConfirmedUwSFlag(PUPData::FLAGS_ACTION_REMOVE));
  EXPECT_TRUE(PUPData::HasConfirmedUwSFlag(PUPData::FLAGS_ACTION_REMOVE |
                                           PUPData::FLAGS_STATE_CONFIRMED_UWS));
}

TEST_F(PUPDataTest, ChoosePUPs) {
  test_data().AddPUP(k12ID,
                     PUPData::FLAGS_ACTION_REMOVE,
                     nullptr,
                     PUPData::kMaxFilesToRemoveSmallUwS);
  test_data().AddPUP(
      k24ID, PUPData::FLAGS_NONE, nullptr, PUPData::kMaxFilesToRemoveSmallUwS);

  std::vector<UwSId> found_pups;
  found_pups.push_back(k12ID);
  found_pups.push_back(k24ID);

  std::vector<UwSId> found_pups_to_report;
  pup_data().ChoosePUPs(
      found_pups, &PUPData::HasReportOnlyFlag, &found_pups_to_report);
  EXPECT_THAT(found_pups_to_report, testing::ElementsAre(k24ID));

  std::vector<UwSId> found_pups_to_remove;
  pup_data().ChoosePUPs(
      found_pups, &PUPData::HasRemovalFlag, &found_pups_to_remove);
  EXPECT_THAT(found_pups_to_remove, testing::ElementsAre(k12ID));
}

TEST_F(PUPDataTest, OpenMachineRegistryKey) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  // Create the key so we can then try to open it.
  base::win::RegKey reg_key(HKEY_LOCAL_MACHINE, k42RegistryKeyPath,
                            KEY_ALL_ACCESS);
  reg_key.Close();
  const RegKeyPath reg_key_path(HKEY_LOCAL_MACHINE, k42RegistryKeyPath);
  EXPECT_TRUE(reg_key_path.Open(KEY_ALL_ACCESS, &reg_key));
  EXPECT_TRUE(reg_key.Valid());
  // Make sure we can read and write.
  EXPECT_EQ(ERROR_SUCCESS,
            reg_key.WriteValue(k24RegistryValueName, k24RegistryValue));
  base::string16 value;
  EXPECT_EQ(ERROR_SUCCESS, reg_key.ReadValue(k24RegistryValueName, &value));
  EXPECT_STREQ(k24RegistryValue, value.c_str());
  reg_key.Close();
}

TEST_F(PUPDataTest, OpenUsersRegistryKey) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_CURRENT_USER);
  base::win::RegKey reg_key;
  reg_key.Create(HKEY_CURRENT_USER, k42RegistryKeyPath, KEY_ALL_ACCESS);
  reg_key.WriteValue(k24RegistryValueName, k24RegistryValue);
  reg_key.Close();

  const RegKeyPath reg_key_path(HKEY_CURRENT_USER, k42RegistryKeyPath);
  EXPECT_TRUE(reg_key_path.Open(KEY_READ, &reg_key));
  // Make sure we can read the empty default value of the key.
  base::string16 value;
  EXPECT_EQ(ERROR_SUCCESS, reg_key.ReadValue(k24RegistryValueName, &value));
  EXPECT_STREQ(k24RegistryValue, value.c_str());
  EXPECT_TRUE(reg_key.Valid());
  reg_key.Close();
}

TEST_F(PUPDataTest, OpenClassesRegistryKey) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_CLASSES_ROOT);
  base::win::RegKey reg_key;
  reg_key.Create(HKEY_CLASSES_ROOT, k42RegistryKeyPath, KEY_ALL_ACCESS);
  reg_key.Close();

  const RegKeyPath reg_key_path(HKEY_CLASSES_ROOT, k42RegistryKeyPath);
  EXPECT_TRUE(reg_key_path.Open(KEY_WRITE, &reg_key));
  EXPECT_TRUE(reg_key.Valid());
  EXPECT_EQ(ERROR_SUCCESS,
            reg_key.WriteValue(k24RegistryValueName, k24RegistryValue));
  reg_key.Close();
}

TEST_F(PUPDataTest, CommonSeparators) {
  base::string16 delimiters(PUPData::kCommonDelimiters,
                            PUPData::kCommonDelimitersLength);
  EXPECT_EQ(3UL, delimiters.size());
  EXPECT_NE(delimiters.find(L','), std::string::npos);
  EXPECT_NE(delimiters.find(L' '), std::string::npos);
  EXPECT_NE(delimiters.find(L'\0'), std::string::npos);
  EXPECT_EQ(delimiters.find(L'\\'), std::string::npos);
  EXPECT_EQ(delimiters.find(L'/'), std::string::npos);
  EXPECT_EQ(delimiters.find(L'\xFE20'), std::string::npos);
}

TEST_F(PUPDataTest, CommaSeparators) {
  base::string16 delimiters(PUPData::kCommaDelimiter,
                            PUPData::kCommaDelimiterLength);
  EXPECT_EQ(1UL, delimiters.size());
  EXPECT_NE(delimiters.find(L','), std::string::npos);
  EXPECT_EQ(delimiters.find(L' '), std::string::npos);
  EXPECT_EQ(delimiters.find(L'\0'), std::string::npos);
  EXPECT_EQ(delimiters.find(L'\\'), std::string::npos);
  EXPECT_EQ(delimiters.find(L'/'), std::string::npos);
  EXPECT_EQ(delimiters.find(L'\xFE20'), std::string::npos);
}

TEST_F(PUPDataTest, HasRemovalFlaggedPUP) {
  test_data().AddPUP(k42ID,
                     PUPData::FLAGS_ACTION_REMOVE,
                     nullptr,
                     PUPData::kMaxFilesToRemoveSmallUwS);
  EXPECT_TRUE(ValidateFlags(&PUPData::HasRemovalFlag));
  EXPECT_FALSE(ValidateFlags(&PUPData::HasReportOnlyFlag));
  EXPECT_FALSE(ValidateFlags(&PUPData::HasRebootFlag));
}

TEST_F(PUPDataTest, HasReportOnlyFlaggedPUP) {
  test_data().AddPUP(
      k42ID, PUPData::FLAGS_NONE, nullptr, PUPData::kMaxFilesToRemoveSmallUwS);
  EXPECT_TRUE(ValidateFlags(&PUPData::HasReportOnlyFlag));
  EXPECT_FALSE(ValidateFlags(&PUPData::HasRemovalFlag));
  EXPECT_FALSE(ValidateFlags(&PUPData::HasRebootFlag));
}

TEST_F(PUPDataTest, HasRebootFlaggedPUP) {
  test_data().AddPUP(k42ID,
                     PUPData::FLAGS_REMOVAL_FORCE_REBOOT,
                     nullptr,
                     PUPData::kMaxFilesToRemoveSmallUwS);
  EXPECT_TRUE(ValidateFlags(&PUPData::HasRebootFlag));
  EXPECT_FALSE(ValidateFlags(&PUPData::HasRemovalFlag));
  EXPECT_TRUE(ValidateFlags(&PUPData::HasReportOnlyFlag));
}

TEST_F(PUPDataTest, HasRebootAndRemoveFlaggedPUP) {
  test_data().AddPUP(
      k42ID,
      PUPData::FLAGS_ACTION_REMOVE | PUPData::FLAGS_REMOVAL_FORCE_REBOOT,
      nullptr,
      PUPData::kMaxFilesToRemoveSmallUwS);
  EXPECT_TRUE(ValidateFlags(&PUPData::HasRebootFlag));
  EXPECT_TRUE(ValidateFlags(&PUPData::HasRemovalFlag));
  EXPECT_FALSE(ValidateFlags(&PUPData::HasReportOnlyFlag));
}

TEST_F(PUPDataTest, HasSomeFlaggedPUP) {
  test_data().AddPUP(k12ID,
                     PUPData::FLAGS_ACTION_REMOVE,
                     nullptr,
                     PUPData::kMaxFilesToRemoveSmallUwS);
  test_data().AddPUP(k24ID,
                     PUPData::FLAGS_REMOVAL_FORCE_REBOOT,
                     nullptr,
                     PUPData::kMaxFilesToRemoveSmallUwS);
  EXPECT_TRUE(ValidateFlags(&PUPData::HasRemovalFlag));
  EXPECT_TRUE(ValidateFlags(&PUPData::HasReportOnlyFlag));
  EXPECT_TRUE(ValidateFlags(&PUPData::HasRebootFlag));
}

TEST_F(PUPDataTest, DeleteRegistryKeyAndValue) {
  test_data().AddPUP(k24ID,
                     PUPData::FLAGS_ACTION_REMOVE,
                     nullptr,
                     PUPData::kMaxFilesToRemoveSmallUwS);
  PUPData::PUP* pup = pup_data().GetPUP(k24ID);

  const RegKeyPath key_path(HKEY_LOCAL_MACHINE, k24RegistryKeyPath);
  PUPData::DeleteRegistryKey(key_path, pup);
  PUPData::DeleteRegistryValue(key_path, k24RegistryValueName, pup);

  ExpectRegistryFootprint(*pup, key_path, L"", L"", REGISTRY_VALUE_MATCH_KEY);

  ExpectRegistryFootprint(*pup,
                          key_path,
                          k24RegistryValueName,
                          L"",
                          REGISTRY_VALUE_MATCH_VALUE_NAME);
}

TEST_F(PUPDataTest, UpdateRegistryValue) {
  test_data().AddPUP(k24ID,
                     PUPData::FLAGS_ACTION_REMOVE,
                     nullptr,
                     PUPData::kMaxFilesToRemoveSmallUwS);
  PUPData::PUP* pup = pup_data().GetPUP(k24ID);

  const RegKeyPath key_path(HKEY_LOCAL_MACHINE, k24RegistryKeyPath);
  PUPData::UpdateRegistryValue(key_path,
                               k24RegistryValueName,
                               k24RegistryValue,
                               REGISTRY_VALUE_MATCH_PARTIAL,
                               pup);

  ExpectRegistryFootprint(*pup,
                          key_path,
                          k24RegistryValueName,
                          k24RegistryValue,
                          REGISTRY_VALUE_MATCH_PARTIAL);
}

TEST_F(PUPDataTest, DeleteScheduledTask) {
  test_data().AddPUP(k24ID,
                     PUPData::FLAGS_ACTION_REMOVE,
                     nullptr,
                     PUPData::kMaxFilesToRemoveSmallUwS);
  PUPData::PUP* pup = pup_data().GetPUP(k24ID);

  EXPECT_TRUE(pup->expanded_scheduled_tasks.empty());
  PUPData::DeleteScheduledTask(kScheduledTaskName, pup);
  EXPECT_THAT(pup->expanded_scheduled_tasks,
              testing::ElementsAre(kScheduledTaskName));
}

TEST_F(PUPDataTest, GetAllPUPs) {
  test_data().AddPUP(k24ID,
                     PUPData::FLAGS_ACTION_REMOVE,
                     nullptr,
                     PUPData::kMaxFilesToRemoveSmallUwS);

  test_data().AddPUP(k42ID,
                     PUPData::FLAGS_ACTION_REMOVE,
                     nullptr,
                     PUPData::kMaxFilesToRemoveSmallUwS);

  const PUPData::PUPDataMap* pup_map = test_data().GetAllPUPs();
  EXPECT_EQ(pup_map->size(), 2UL);

  PUPData::PUPDataMap::const_iterator pup_it = pup_map->find(k24ID);
  EXPECT_TRUE(pup_it != pup_map->end());
  EXPECT_EQ(pup_it->second->signature().id, k24ID);

  pup_it = pup_map->find(k42ID);
  EXPECT_TRUE(pup_it != pup_map->end());
  EXPECT_EQ(pup_it->second->signature().id, k42ID);

  pup_it = pup_map->find(k12ID);
  EXPECT_TRUE(pup_it == pup_map->end());
}

TEST_F(PUPDataTest, GetUwSIds) {
  test_data().AddPUP(k24ID,
                     PUPData::FLAGS_ACTION_REMOVE,
                     nullptr,
                     PUPData::kMaxFilesToRemoveSmallUwS);

  test_data().AddPUP(k42ID,
                     PUPData::FLAGS_ACTION_REMOVE,
                     nullptr,
                     PUPData::kMaxFilesToRemoveSmallUwS);

  const std::vector<UwSId>* pup_ids = PUPData::GetUwSIds();
  EXPECT_EQ(pup_ids->size(), 2UL);
  EXPECT_TRUE(base::Contains(*pup_ids, k24ID));
  EXPECT_TRUE(base::Contains(*pup_ids, k42ID));
  EXPECT_FALSE(base::Contains(*pup_ids, k12ID));
}

TEST_F(PUPDataTest, InitializeTest) {
  PUPData::InitializePUPData({});
  EXPECT_EQ(PUPData::GetUwSIds()->size(), 0UL);

  PUPData::InitializePUPData({&TestUwSCatalog::GetInstance()});
  EXPECT_EQ(PUPData::GetUwSIds()->size(),
            TestUwSCatalog::GetInstance().GetUwSIds().size());
}

// Verify that SanitizePath is written to handle all the CSIDL values used in
// the PuP data.
TEST(SanitizePathVsRawPupDataCsidlTest, TestAllCsidlValues) {
  using chrome_cleaner::PUPData;
  using chrome_cleaner::sanitization_internal::PATH_CSIDL_END;
  using chrome_cleaner::sanitization_internal::PATH_CSIDL_START;

  // Get set containing the distinct CSIDL values used in rewrite_rules[].
  std::set<int> csidl_list;
  for (const auto& entry : chrome_cleaner::PathKeyToSanitizeString()) {
    int id = entry.first;
    // Exclude non-CSIDL replacements.
    if (id < PATH_CSIDL_START || id > PATH_CSIDL_END) {
      continue;
    }

    // id represents a key used by PathService to lookup a FilePath. A
    // PathService Provider was registered to handle the CSIDL values with an
    // offset of PATH_CSIDL_START to avoid collisions with other PathService
    // Providers.
    int csidl = id - PATH_CSIDL_START;
    csidl_list.insert(csidl);
  }

  // Report any unchecked CSIDLs as unsanitized.
  for (const auto& pup_id : *PUPData::GetUwSIds()) {
    const PUPData::UwSSignature& signature =
        PUPData::GetPUP(pup_id)->signature();
    for (const PUPData::StaticDiskFootprint* disk_footprint =
             signature.disk_footprints;
         disk_footprint->path != nullptr;
         ++disk_footprint) {
      int csidl = disk_footprint->csidl;
      if (csidl != PUPData::kInvalidCsidl &&
          csidl_list.find(csidl) == csidl_list.end()) {
        ADD_FAILURE() << "CSIDL " << csidl << " is not sanitized in "
                      << signature.name << " with footprint "
                      << disk_footprint->path;
      }
    }
  }
}

}  // namespace chrome_cleaner
