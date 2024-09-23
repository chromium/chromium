// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/common/file_type_policies.h"

#include <string.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/content/common/file_type_policies_prefs.h"
#include "components/safe_browsing/content/common/file_type_policies_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::NiceMock;

namespace safe_browsing {

class MockFileTypePolicies : public FileTypePolicies {
 public:
  MockFileTypePolicies() {}

  MockFileTypePolicies(const MockFileTypePolicies&) = delete;
  MockFileTypePolicies& operator=(const MockFileTypePolicies&) = delete;

  ~MockFileTypePolicies() override {}

  MOCK_METHOD2(RecordUpdateMetrics, void(UpdateResult, const std::string&));
};

class FileTypePoliciesTest : public testing::Test {
 protected:
  FileTypePoliciesTest() = default;
  ~FileTypePoliciesTest() override = default;
  void SetUp() override {
    file_type::RegisterProfilePrefs(pref_service_.registry());
  }

 protected:
  NiceMock<MockFileTypePolicies> policies_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(FileTypePoliciesTest, UnpackResourceBundle) {
  EXPECT_CALL(policies_,
              RecordUpdateMetrics(FileTypePolicies::UpdateResult::SUCCESS,
                                  "ResourceBundle"));
  policies_.PopulateFromResourceBundle();

  // Look up a few well known types to ensure they're present.
  // Some types vary by OS, and we check one per OS to validate
  // that gen_file_type_proto.py does its job.
  //
  // NOTE: If the settings for these change in download_file_types.asciipb,
  // then you'll need to change them here as well.

  // Lookup .exe that varies on OS_WIN.
  base::FilePath exe_file(FILE_PATH_LITERAL("a/foo.exe"));
  DownloadFileType file_type =
      policies_.PolicyForFile(exe_file, GURL{}, nullptr);
  EXPECT_EQ("exe", file_type.extension());
  EXPECT_EQ(0l, file_type.uma_value());
  EXPECT_FALSE(file_type.is_archive());
  EXPECT_EQ(DownloadFileType::FULL_PING, file_type.ping_setting());
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(DownloadFileType::ALLOW_ON_USER_GESTURE,
            file_type.platform_settings(0).danger_level());
  EXPECT_EQ(DownloadFileType::DISALLOW_AUTO_OPEN,
            file_type.platform_settings(0).auto_open_hint());
#else
  EXPECT_EQ(DownloadFileType::NOT_DANGEROUS,
            file_type.platform_settings(0).danger_level());
  EXPECT_EQ(DownloadFileType::ALLOW_AUTO_OPEN,
            file_type.platform_settings(0).auto_open_hint());
#endif

  // Lookup .class that varies on OS_CHROMEOS, and also has a
  // default setting set.
  base::FilePath class_file(FILE_PATH_LITERAL("foo.class"));
  file_type = policies_.PolicyForFile(class_file, GURL{}, nullptr);
  EXPECT_EQ("class", file_type.extension());
  EXPECT_EQ(13l, file_type.uma_value());
  EXPECT_FALSE(file_type.is_archive());
  EXPECT_EQ(DownloadFileType::FULL_PING, file_type.ping_setting());
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(DownloadFileType::NOT_DANGEROUS,
            file_type.platform_settings(0).danger_level());
  EXPECT_EQ(DownloadFileType::ALLOW_AUTO_OPEN,
            file_type.platform_settings(0).auto_open_hint());
#else
  EXPECT_EQ(DownloadFileType::ALLOW_ON_USER_GESTURE,
            file_type.platform_settings(0).danger_level());
  EXPECT_EQ(DownloadFileType::DISALLOW_AUTO_OPEN,
            file_type.platform_settings(0).auto_open_hint());
#endif

  // Lookup .dmg that varies on OS_MACOS
  base::FilePath dmg_file(FILE_PATH_LITERAL("foo.dmg"));
  file_type = policies_.PolicyForFile(dmg_file, GURL{}, nullptr);
  EXPECT_EQ("dmg", file_type.extension());
  EXPECT_EQ(21, file_type.uma_value());
  EXPECT_FALSE(file_type.is_archive());
  EXPECT_EQ(DownloadFileType::FULL_PING, file_type.ping_setting());
#if BUILDFLAG(IS_APPLE)
  EXPECT_EQ(DownloadFileType::ALLOW_ON_USER_GESTURE,
            file_type.platform_settings(0).danger_level());
  EXPECT_EQ(DownloadFileType::DISALLOW_AUTO_OPEN,
            file_type.platform_settings(0).auto_open_hint());
#else
  EXPECT_EQ(DownloadFileType::NOT_DANGEROUS,
            file_type.platform_settings(0).danger_level());
  EXPECT_EQ(DownloadFileType::ALLOW_AUTO_OPEN,
            file_type.platform_settings(0).auto_open_hint());
#endif

  // Lookup .dex that varies on OS_ANDROID and OS_CHROMEOS
  base::FilePath dex_file(FILE_PATH_LITERAL("foo.dex"));
  file_type = policies_.PolicyForFile(dex_file, GURL{}, nullptr);
  EXPECT_EQ("dex", file_type.extension());
  EXPECT_EQ(143, file_type.uma_value());
  EXPECT_FALSE(file_type.is_archive());
  EXPECT_EQ(DownloadFileType::FULL_PING, file_type.ping_setting());
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(DownloadFileType::ALLOW_ON_USER_GESTURE,
            file_type.platform_settings(0).danger_level());
  EXPECT_EQ(DownloadFileType::DISALLOW_AUTO_OPEN,
            file_type.platform_settings(0).auto_open_hint());
#else
  EXPECT_EQ(DownloadFileType::NOT_DANGEROUS,
            file_type.platform_settings(0).danger_level());
  EXPECT_EQ(DownloadFileType::ALLOW_AUTO_OPEN,
            file_type.platform_settings(0).auto_open_hint());
#endif

  // Lookup .rpm that varies on OS_LINUX
  base::FilePath rpm_file(FILE_PATH_LITERAL("foo.rpm"));
  file_type = policies_.PolicyForFile(rpm_file, GURL{}, nullptr);
  EXPECT_EQ("rpm", file_type.extension());
  EXPECT_EQ(142, file_type.uma_value());
  EXPECT_FALSE(file_type.is_archive());
  EXPECT_EQ(DownloadFileType::FULL_PING, file_type.ping_setting());
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX)
  EXPECT_EQ(DownloadFileType::ALLOW_ON_USER_GESTURE,
            file_type.platform_settings(0).danger_level());
  EXPECT_EQ(DownloadFileType::DISALLOW_AUTO_OPEN,
            file_type.platform_settings(0).auto_open_hint());
#else
  EXPECT_EQ(DownloadFileType::NOT_DANGEROUS,
            file_type.platform_settings(0).danger_level());
  EXPECT_EQ(DownloadFileType::ALLOW_AUTO_OPEN,
            file_type.platform_settings(0).auto_open_hint());
#endif

  // Look .zip, an archive.  The same on all platforms.
  base::FilePath zip_file(FILE_PATH_LITERAL("b/bar.txt.zip"));
  file_type = policies_.PolicyForFile(zip_file, GURL{}, nullptr);
  EXPECT_EQ("zip", file_type.extension());
  EXPECT_EQ(7l, file_type.uma_value());
  EXPECT_TRUE(file_type.is_archive());
  EXPECT_EQ(DownloadFileType::FULL_PING, file_type.ping_setting());
  EXPECT_EQ(DownloadFileType::NOT_DANGEROUS,
            file_type.platform_settings(0).danger_level());

  // Check other accessors
  EXPECT_EQ(7l, policies_.UmaValueForFile(zip_file));
  EXPECT_TRUE(policies_.IsArchiveFile(zip_file));
  EXPECT_FALSE(policies_.IsArchiveFile(exe_file));

  // Verify settings on the default type.
  file_type = policies_.PolicyForFile(
      base::FilePath(FILE_PATH_LITERAL("a/foo.fooobar")), GURL{}, nullptr);
  EXPECT_EQ("", file_type.extension());
  EXPECT_EQ(18l, file_type.uma_value());
  EXPECT_FALSE(file_type.is_archive());
  EXPECT_EQ(DownloadFileType::FULL_PING, file_type.ping_setting());
  EXPECT_EQ(DownloadFileType::NOT_DANGEROUS,
            file_type.platform_settings(0).danger_level());
  EXPECT_EQ(DownloadFileType::ALLOW_AUTO_OPEN,
            file_type.platform_settings(0).auto_open_hint());
}

TEST_F(FileTypePoliciesTest, BadProto) {
  base::AutoLock lock(policies_.lock_);
  EXPECT_EQ(FileTypePolicies::UpdateResult::FAILED_EMPTY,
            policies_.PopulateFromBinaryPb(std::string()));

  EXPECT_EQ(FileTypePolicies::UpdateResult::FAILED_PROTO_PARSE,
            policies_.PopulateFromBinaryPb("foobar"));

  DownloadFileTypeConfig cfg;
  cfg.set_sampled_ping_probability(0.1f);
  EXPECT_EQ(FileTypePolicies::UpdateResult::FAILED_DEFAULT_SETTING_SET,
            policies_.PopulateFromBinaryPb(cfg.SerializeAsString()));

  cfg.mutable_default_file_type()->add_platform_settings();
  // This is missing a platform_setting.
  auto* file_type = cfg.add_file_types();
  EXPECT_EQ(FileTypePolicies::UpdateResult::FAILED_WRONG_SETTINGS_COUNT,
            policies_.PopulateFromBinaryPb(cfg.SerializeAsString()));

  file_type->add_platform_settings();
  EXPECT_EQ(FileTypePolicies::UpdateResult::SUCCESS,
            policies_.PopulateFromBinaryPb(cfg.SerializeAsString()));
}

TEST_F(FileTypePoliciesTest, BadUpdateFromExisting) {
  base::AutoLock lock(policies_.lock_);
  // Make a minimum viable config
  DownloadFileTypeConfig cfg;
  cfg.mutable_default_file_type()->add_platform_settings();
  cfg.add_file_types()->add_platform_settings();
  cfg.set_version_id(2);
  EXPECT_EQ(FileTypePolicies::UpdateResult::SUCCESS,
            policies_.PopulateFromBinaryPb(cfg.SerializeAsString()));

  // Can't update to the same version
  EXPECT_EQ(FileTypePolicies::UpdateResult::SKIPPED_VERSION_CHECK_EQUAL,
            policies_.PopulateFromBinaryPb(cfg.SerializeAsString()));

  // Can't go backward
  cfg.set_version_id(1);
  EXPECT_EQ(FileTypePolicies::UpdateResult::FAILED_VERSION_CHECK,
            policies_.PopulateFromBinaryPb(cfg.SerializeAsString()));
}

TEST_F(FileTypePoliciesTest, NoInspectionTypeReturnsDefault) {
  policies_.PopulateFromResourceBundle();
  EXPECT_EQ(policies_.GetMaxFileSizeToAnalyze(
                base::FilePath(FILE_PATH_LITERAL("/path/to/test.pdf"))),
            static_cast<uint64_t>(-1));
}

TEST_F(FileTypePoliciesTest, ChecksInspectionTypeNotDefault) {
  policies_.PopulateFromResourceBundle();
  // r01 is inspected as a RAR, so the max file size should match
  EXPECT_EQ(policies_.GetMaxFileSizeToAnalyze("r01"),
            policies_.GetMaxFileSizeToAnalyze("rar"));
  EXPECT_EQ(policies_.GetMaxFileSizeToAnalyze(
                base::FilePath(FILE_PATH_LITERAL("/path/to/test.r01"))),
            policies_.GetMaxFileSizeToAnalyze("rar"));
}

// Regression test for https://crbug.com/355016912. The policy for overriding
// file types should only override danger level.
TEST_F(FileTypePoliciesTest, NotDangerousOverrideShouldOnlyOverrideDangerType) {
  policies_.PopulateFromResourceBundle();
  base::Value::List list;
  list.Append(CreateNotDangerousOverridePolicyEntryForTesting(
      "exe", {"http://www.example.com"}));
  pref_service_.SetList(
      file_type::prefs::kExemptDomainFileTypePairsFromFileTypeDownloadWarnings,
      std::move(list));

  base::FilePath exe_file(FILE_PATH_LITERAL("a/foo.exe"));
  DownloadFileType file_type = policies_.PolicyForFile(
      exe_file, GURL("http://www.example.com"), &pref_service_);
  // The danger level should be overridden to NOT_DANGEROUS.
  EXPECT_EQ(DownloadFileType::NOT_DANGEROUS,
            file_type.platform_settings(0).danger_level());
  // The other fields should remain unchanged.
  EXPECT_EQ(0l, file_type.uma_value());
}

}  // namespace safe_browsing
