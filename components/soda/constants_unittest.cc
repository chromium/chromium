// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/soda/constants.h"

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_path_override.h"
#include "components/component_updater/component_updater_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace speech {
namespace {

class SodaConstantsTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    path_override_.emplace(component_updater::DIR_COMPONENT_USER,
                           temp_dir_.GetPath());
  }

  // Creates `SODA/<version>/`, and the SODA binary inside it if `with_binary`.
  base::FilePath CreateSodaVersion(const std::string& version,
                                   bool with_binary) {
    const base::FilePath version_dir = GetSodaDirectory().AppendASCII(version);
    const base::FilePath binary_path =
        version_dir.Append(kSodaBinaryRelativePath);
    EXPECT_TRUE(base::CreateDirectory(with_binary ? binary_path.DirName()
                                                  : version_dir));
    if (with_binary) {
      EXPECT_TRUE(base::WriteFile(binary_path, ""));
    }
    return version_dir;
  }

  // Creates `SODALanguagePacks/en-US/<version>/`, and the models directory
  // inside it if `with_models`.
  base::FilePath CreateLanguagePackVersion(const std::string& version,
                                           bool with_models) {
    const base::FilePath version_dir =
        GetSodaLanguagePacksDirectory().AppendASCII("en-US").AppendASCII(
            version);
    EXPECT_TRUE(base::CreateDirectory(
        with_models ? version_dir.Append(kSodaLanguagePackDirectoryRelativePath)
                    : version_dir));
    return version_dir;
  }

 private:
  base::ScopedTempDir temp_dir_;
  std::optional<base::ScopedPathOverride> path_override_;
};

TEST_F(SodaConstantsTest, GetLatestSodaDirectoryIsEmptyWhenNotInstalled) {
  ASSERT_FALSE(GetSodaDirectory().empty());
  EXPECT_TRUE(GetLatestSodaDirectory().empty());
  EXPECT_TRUE(GetSodaBinaryPath().empty());
}

TEST_F(SodaConstantsTest, GetLatestSodaDirectoryComparesVersionsNumerically) {
  // "1.2.5" sorts after "1.2.12" as a string, but 1.2.12 is the newer version.
  CreateSodaVersion("1.2.3", /*with_binary=*/true);
  CreateSodaVersion("1.2.5", /*with_binary=*/true);
  const base::FilePath v1_2_12 =
      CreateSodaVersion("1.2.12", /*with_binary=*/true);

  EXPECT_EQ(GetLatestSodaDirectory(), v1_2_12);
  EXPECT_EQ(GetSodaBinaryPath(), v1_2_12.Append(kSodaBinaryRelativePath));
}

TEST_F(SodaConstantsTest, GetLatestSodaDirectoryIgnoresNonVersionEntries) {
  const base::FilePath v1_2_12 =
      CreateSodaVersion("1.2.12", /*with_binary=*/true);
  ASSERT_TRUE(base::CreateDirectory(GetSodaDirectory().AppendASCII("latest")));
  // A regular file, i.e. not an installation at all.
  ASSERT_TRUE(base::WriteFile(GetSodaDirectory().AppendASCII("9.9.9"), ""));

  EXPECT_EQ(GetLatestSodaDirectory(), v1_2_12);
}

TEST_F(SodaConstantsTest, GetLatestSodaDirectorySkipsIncompleteInstallations) {
  const base::FilePath v1_2_12 =
      CreateSodaVersion("1.2.12", /*with_binary=*/true);
  // A newer version that has been created but not unpacked yet.
  CreateSodaVersion("1.2.13", /*with_binary=*/false);
  // A newer version that has been partially unpacked: the directory that
  // normally holds the binary exists, but the binary itself doesn't.
  ASSERT_TRUE(base::CreateDirectory(GetSodaDirectory()
                                        .AppendASCII("1.2.14")
                                        .Append(kSodaBinaryRelativePath)
                                        .DirName()));

  EXPECT_EQ(GetLatestSodaDirectory(), v1_2_12);
}

TEST_F(SodaConstantsTest, GetLatestSodaDirectoryFallsBackToHighestVersion) {
  // If nothing looks usable, still return the newest version rather than an
  // arbitrary older one.
  CreateSodaVersion("1.2.5", /*with_binary=*/false);
  const base::FilePath v1_2_12 =
      CreateSodaVersion("1.2.12", /*with_binary=*/false);

  EXPECT_EQ(GetLatestSodaDirectory(), v1_2_12);
}

TEST_F(SodaConstantsTest,
       GetLatestSodaLanguagePackDirectoryIsEmptyWhenNotInstalled) {
  EXPECT_TRUE(GetLatestSodaLanguagePackDirectory("en-US").empty());
}

TEST_F(SodaConstantsTest,
       GetLatestSodaLanguagePackDirectoryComparesVersionsNumerically) {
  CreateLanguagePackVersion("1.2.5", /*with_models=*/true);
  const base::FilePath v1_2_12 =
      CreateLanguagePackVersion("1.2.12", /*with_models=*/true);
  CreateLanguagePackVersion("1.2.13", /*with_models=*/false);

  EXPECT_EQ(GetLatestSodaLanguagePackDirectory("en-US"),
            v1_2_12.Append(kSodaLanguagePackDirectoryRelativePath));
}

}  // namespace
}  // namespace speech
