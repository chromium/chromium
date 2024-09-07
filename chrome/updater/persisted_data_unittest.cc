// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/persisted_data.h"

#include <memory>
#include <string>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/activity.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/test/test_scope.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/update_client.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/registry.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"
#endif

namespace updater {

TEST(PersistedDataTest, Simple) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(
      GetUpdaterScopeForTesting(), pref.get(), nullptr);

  EXPECT_FALSE(metadata->GetProductVersion("someappid").IsValid());
  EXPECT_TRUE(metadata->GetFingerprint("someappid").empty());
  EXPECT_TRUE(metadata->GetAppIds().empty());

  metadata->SetProductVersion("someappid", base::Version("1.0"));
  EXPECT_STREQ("1.0",
               metadata->GetProductVersion("someappid").GetString().c_str());

  metadata->SetFingerprint("someappid", "fp1");
  EXPECT_STREQ("fp1", metadata->GetFingerprint("someappid").c_str());

  // Store some more apps in prefs, in addition to "someappid". Expect only
  // the app ids for apps with valid versions to be returned.
  metadata->SetProductVersion("appid1", base::Version("2.0"));
  metadata->SetFingerprint("appid2-nopv", "somefp");
  EXPECT_FALSE(metadata->GetProductVersion("appid2-nopv").IsValid());
  const auto app_ids = metadata->GetAppIds();
  EXPECT_EQ(2u, app_ids.size());
  EXPECT_TRUE(base::Contains(app_ids, "someappid"));
  EXPECT_TRUE(base::Contains(app_ids, "appid1"));
  EXPECT_FALSE(base::Contains(app_ids, "appid2-nopv"));  // No valid pv.

  const base::Time time1 = base::Time::FromSecondsSinceUnixEpoch(10);
  metadata->SetLastChecked(time1);
  EXPECT_EQ(metadata->GetLastChecked(), time1);
  const base::Time time2 = base::Time::FromSecondsSinceUnixEpoch(20);
  metadata->SetLastStarted(time2);
  EXPECT_EQ(metadata->GetLastStarted(), time2);
}

TEST(PersistedDataTest, MixedCase) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(
      GetUpdaterScopeForTesting(), pref.get(), nullptr);

  metadata->SetProductVersion("someappid", base::Version("1.0"));
  metadata->SetProductVersion("SOMEAPPID2", base::Version("2.0"));
  EXPECT_STREQ("1.0",
               metadata->GetProductVersion("someAPPID").GetString().c_str());
  EXPECT_STREQ("1.0",
               metadata->GetProductVersion("someappid").GetString().c_str());
  EXPECT_STREQ("2.0",
               metadata->GetProductVersion("someAPPID2").GetString().c_str());
  EXPECT_STREQ("2.0",
               metadata->GetProductVersion("someappid2").GetString().c_str());
}

TEST(PersistedDataTest, SharedPref) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(
      GetUpdaterScopeForTesting(), pref.get(), nullptr);

  metadata->SetProductVersion("someappid", base::Version("1.0"));
  EXPECT_STREQ("1.0",
               metadata->GetProductVersion("someappid").GetString().c_str());

  // Now, create a new PersistedData reading from the same path, verify
  // that it loads the value.
  metadata = base::MakeRefCounted<PersistedData>(GetUpdaterScopeForTesting(),
                                                 pref.get(), nullptr);
  EXPECT_STREQ("1.0",
               metadata->GetProductVersion("someappid").GetString().c_str());
}

TEST(PersistedDataTest, RemoveAppId) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(
      GetUpdaterScopeForTesting(), pref.get(), nullptr);

  RegistrationRequest data;
  data.app_id = "someappid";
  data.brand_code = "somebrand";
  data.ap = "arandom-ap=likethis";
  data.version = base::Version("1.0");
  data.existence_checker_path =
      base::FilePath(FILE_PATH_LITERAL("some/file/path"));

  metadata->RegisterApp(data);

  data.app_id = "someappid2";
  data.brand_code = "somebrand";
  data.ap = "arandom-ap=likethis";
  data.version = base::Version("2.0");
  data.existence_checker_path =
      base::FilePath(FILE_PATH_LITERAL("some/file/path"));

  metadata->RegisterApp(data);
  EXPECT_EQ(size_t{2}, metadata->GetAppIds().size());

  metadata->RemoveApp("someAPPID");
  EXPECT_EQ(size_t{1}, metadata->GetAppIds().size());

  metadata->RemoveApp("someappid2");
  EXPECT_TRUE(metadata->GetAppIds().empty());
}

TEST(PersistedDataTest, RegisterApp_SetFirstActive) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(
      GetUpdaterScopeForTesting(), pref.get(), nullptr);

  RegistrationRequest data;
  data.app_id = "someappid";
  data.brand_code = "somebrand";
  data.ap = "arandom-ap=likethis";
  data.version = base::Version("1.0");
  data.existence_checker_path =
      base::FilePath(FILE_PATH_LITERAL("some/file/path"));
  metadata->RegisterApp(data);
  EXPECT_EQ(metadata->GetDateLastActive("someappid"), -1);
  EXPECT_EQ(metadata->GetDateLastRollCall("someappid"), -1);

  data.version = base::Version("2.0");
  data.dla = 1221;
  data.dlrc = 1221;
  metadata->RegisterApp(data);
  EXPECT_EQ(metadata->GetDateLastActive("someappid"), 1221);
  EXPECT_EQ(metadata->GetDateLastRollCall("someappid"), 1221);

  data.version = base::Version("3.0");
  data.dla = std::nullopt;
  data.dlrc = std::nullopt;
  metadata->RegisterApp(data);
  EXPECT_EQ(metadata->GetDateLastActive("someappid"), 1221);
  EXPECT_EQ(metadata->GetDateLastRollCall("someappid"), 1221);
}

#if BUILDFLAG(IS_WIN)
TEST(PersistedDataTest, LastOSVersion) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(
      GetUpdaterScopeForTesting(), pref.get(), nullptr);

  EXPECT_EQ(metadata->GetLastOSVersion(), std::nullopt);

  // This will persist the current OS version into the persisted data.
  metadata->SetLastOSVersion();
  EXPECT_NE(metadata->GetLastOSVersion(), std::nullopt);

  // Compare the persisted data OS version to the version from `::GetVersionEx`.
  const OSVERSIONINFOEX metadata_os = metadata->GetLastOSVersion().value();

  OSVERSIONINFOEX os = {};
  os.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  EXPECT_TRUE(::GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&os)));
#pragma clang diagnostic pop

  EXPECT_EQ(metadata_os.dwOSVersionInfoSize, os.dwOSVersionInfoSize);
  EXPECT_EQ(metadata_os.dwMajorVersion, os.dwMajorVersion);
  EXPECT_EQ(metadata_os.dwMinorVersion, os.dwMinorVersion);
  EXPECT_EQ(metadata_os.dwBuildNumber, os.dwBuildNumber);
  EXPECT_EQ(metadata_os.dwPlatformId, os.dwPlatformId);
  EXPECT_STREQ(metadata_os.szCSDVersion, os.szCSDVersion);
  EXPECT_EQ(metadata_os.wServicePackMajor, os.wServicePackMajor);
  EXPECT_EQ(metadata_os.wServicePackMinor, os.wServicePackMinor);
  EXPECT_EQ(metadata_os.wSuiteMask, os.wSuiteMask);
  EXPECT_EQ(metadata_os.wProductType, os.wProductType);
}

TEST(PersistedDataTest, SetEulaRequired) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(
      GetUpdaterScopeForTesting(), pref.get(), nullptr);

  EXPECT_FALSE(metadata->GetEulaRequired());

  // This will set "eula_required=true" in the persisted data and also persist
  // `eulaaccepted=0` in the registry.
  metadata->SetEulaRequired(/*eula_required=*/true);
  EXPECT_TRUE(metadata->GetEulaRequired());
  DWORD eula_accepted = 0;
  const HKEY root = UpdaterScopeToHKeyRoot(GetUpdaterScopeForTesting());
  EXPECT_EQ(base::win::RegKey(root, UPDATER_KEY, Wow6432(KEY_READ))
                .ReadValueDW(L"eulaaccepted", &eula_accepted),
            ERROR_SUCCESS);
  EXPECT_EQ(eula_accepted, 0ul);

  // This will set "eula_required=false" in the persisted data and also delete
  // the `eulaaccepted` value in the registry.
  metadata->SetEulaRequired(/*eula_required=*/false);
  EXPECT_FALSE(metadata->GetEulaRequired());
  EXPECT_FALSE(base::win::RegKey(root, UPDATER_KEY, Wow6432(KEY_READ))
                   .HasValue(L"eulaaccepted"));
}
#endif

class PersistedDataRegistrationRequestTest : public ::testing::Test {
#if BUILDFLAG(IS_WIN)
 protected:
  void SetUp() override { DeleteBrandCodeValueInRegistry(); }
  void TearDown() override { DeleteBrandCodeValueInRegistry(); }

 private:
  void DeleteBrandCodeValueInRegistry() {
    base::win::RegKey(UpdaterScopeToHKeyRoot(GetUpdaterScopeForTesting()),
                      GetAppClientStateKey(L"someappid").c_str(),
                      Wow6432(KEY_SET_VALUE))
        .DeleteValue(kRegValueBrandCode);
  }
#endif
};

TEST_F(PersistedDataRegistrationRequestTest, RegistrationRequest) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(
      GetUpdaterScopeForTesting(), pref.get(), nullptr);

  RegistrationRequest data;
  data.app_id = "someappid";
  data.brand_code = "somebrand";
  data.ap = "arandom-ap=likethis";
  data.version = base::Version("1.0");
  data.existence_checker_path =
      base::FilePath(FILE_PATH_LITERAL("some/file/path"));
  data.cohort = "testcohort";
  data.cohort_name = "testcohortname";
  data.cohort_hint = "testcohorthint";

  metadata->RegisterApp(data);
  EXPECT_TRUE(metadata->GetProductVersion("someappid").IsValid());
  EXPECT_STREQ("1.0",
               metadata->GetProductVersion("someappid").GetString().c_str());
  EXPECT_EQ(FILE_PATH_LITERAL("some/file/path"),
            metadata->GetExistenceCheckerPath("someappid").value());
  EXPECT_STREQ("arandom-ap=likethis", metadata->GetAP("someappid").c_str());
  EXPECT_STREQ("somebrand", metadata->GetBrandCode("someappid").c_str());
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(
      base::win::RegKey(UpdaterScopeToHKeyRoot(GetUpdaterScopeForTesting()),
                        GetAppClientStateKey(L"someappid").c_str(),
                        Wow6432(KEY_SET_VALUE))
          .WriteValue(kRegValueBrandCode, L"nbrnd"),
      ERROR_SUCCESS);
  EXPECT_STREQ(metadata->GetBrandCode("someappid").c_str(), "nbrnd");
#endif

  EXPECT_STREQ("testcohort", metadata->GetCohort("someappid").c_str());
  EXPECT_STREQ("testcohortname", metadata->GetCohortName("someappid").c_str());
  EXPECT_STREQ("testcohorthint", metadata->GetCohortHint("someappid").c_str());

#if BUILDFLAG(IS_WIN)
  base::win::RegKey key;
  EXPECT_EQ(key.Open(UpdaterScopeToHKeyRoot(GetUpdaterScopeForTesting()),
                     GetAppClientStateKey(L"someappid").c_str(),
                     Wow6432(KEY_QUERY_VALUE)),
            ERROR_SUCCESS);
  std::wstring ap;
  EXPECT_EQ(key.ReadValue(L"ap", &ap), ERROR_SUCCESS);
  EXPECT_EQ(ap, L"arandom-ap=likethis");
#endif
}

TEST_F(PersistedDataRegistrationRequestTest, RegistrationRequestPartial) {
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(
      GetUpdaterScopeForTesting(), pref.get(), nullptr);

  RegistrationRequest data;
  data.app_id = "someappid";
  data.brand_code = "somebrand";
  data.ap = "arandom-ap=likethis";
  data.version = base::Version("1.0");
  data.existence_checker_path =
      base::FilePath(FILE_PATH_LITERAL("some/file/path"));
  metadata->RegisterApp(data);
  EXPECT_TRUE(metadata->GetProductVersion("someappid").IsValid());
  EXPECT_STREQ("1.0",
               metadata->GetProductVersion("someappid").GetString().c_str());
  EXPECT_EQ(FILE_PATH_LITERAL("some/file/path"),
            metadata->GetExistenceCheckerPath("someappid").value());
  EXPECT_STREQ("arandom-ap=likethis", metadata->GetAP("someappid").c_str());
  EXPECT_STREQ("somebrand", metadata->GetBrandCode("someappid").c_str());

  RegistrationRequest data2;
  data2.app_id = data.app_id;
  data2.ap = "different_ap";
  metadata->RegisterApp(data2);
  EXPECT_STREQ("1.0",
               metadata->GetProductVersion(data.app_id).GetString().c_str());
  EXPECT_EQ(FILE_PATH_LITERAL("some/file/path"),
            metadata->GetExistenceCheckerPath(data.app_id).value());
  EXPECT_STREQ("different_ap", metadata->GetAP(data.app_id).c_str());
  EXPECT_STREQ("somebrand", metadata->GetBrandCode(data.app_id).c_str());

  RegistrationRequest data3;
  data3.app_id = "someappid3";
  data3.brand_code = "somebrand";
  data3.version = base::Version("1.0");
  metadata->RegisterApp(data3);
  EXPECT_TRUE(metadata->GetProductVersion("someappid3").IsValid());
  EXPECT_STREQ("1.0",
               metadata->GetProductVersion("someappid3").GetString().c_str());
  EXPECT_EQ(FILE_PATH_LITERAL(""),
            metadata->GetExistenceCheckerPath("someappid3").value());
  EXPECT_STREQ("", metadata->GetAP("someappid3").c_str());
  EXPECT_STREQ("somebrand", metadata->GetBrandCode("someappid3").c_str());
}

}  // namespace updater
