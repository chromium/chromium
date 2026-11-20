// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/soda/constants.h"

#include <optional>
#include <string>
#include <string_view>

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

// The tests below pin how locales resolve to language packs today, so that
// changes to the resolution show up as test diffs.

TEST(SodaLanguageResolutionTest, MaybeMapToChineseLocale) {
  EXPECT_EQ(MaybeMapToChineseLocale("zh-TW"), "cmn-Hant-TW");
  EXPECT_EQ(MaybeMapToChineseLocale("ZH-cn"), "cmn-Hans-CN");
  EXPECT_EQ(MaybeMapToChineseLocale("cmn-hant-tw"), "cmn-Hant-TW");
  // Everything else passes through unchanged, including bare languages.
  EXPECT_EQ(MaybeMapToChineseLocale("zh"), "zh");
  EXPECT_EQ(MaybeMapToChineseLocale("es"), "es");
  EXPECT_EQ(MaybeMapToChineseLocale("en-GB"), "en-GB");
}

TEST(SodaLanguageResolutionTest, GetLanguageCode) {
  EXPECT_EQ(GetLanguageCode("en-US"), LanguageCode::kEnUs);
  EXPECT_EQ(GetLanguageCode("en-us"), LanguageCode::kEnUs);
  EXPECT_EQ(GetLanguageCode("es-ES"), LanguageCode::kEsEs);
  EXPECT_EQ(GetLanguageCode("zh-TW"), LanguageCode::kZhTw);
  EXPECT_EQ(GetLanguageCode("cmn-Hans-CN"), LanguageCode::kZhCn);
  // Only locales with their own component config resolve. Bare languages and
  // other regional variants don't.
  EXPECT_EQ(GetLanguageCode("es"), LanguageCode::kNone);
  EXPECT_EQ(GetLanguageCode("de"), LanguageCode::kNone);
  EXPECT_EQ(GetLanguageCode("en-GB"), LanguageCode::kNone);
  EXPECT_EQ(GetLanguageCode("fr-CA"), LanguageCode::kNone);
}

TEST(SodaLanguageResolutionTest, MatchingLanguageSubtag) {
  const auto code = [](std::string_view language) {
    const std::optional<SodaLanguagePackComponentConfig> config =
        GetLanguageComponentConfigMatchingLanguageSubtag(language);
    return config ? config->language_code : LanguageCode::kNone;
  };
  EXPECT_EQ(code("es"), LanguageCode::kEsEs);
  EXPECT_EQ(code("es-MX"), LanguageCode::kEsEs);
  EXPECT_EQ(code("en-GB"), LanguageCode::kEnUs);
  EXPECT_EQ(code("fr-CA"), LanguageCode::kFrFr);
  EXPECT_EQ(code("zh-TW"), LanguageCode::kZhTw);
  EXPECT_EQ(code("zh-CN"), LanguageCode::kZhCn);
  EXPECT_EQ(code("xx"), LanguageCode::kNone);
}

}  // namespace
}  // namespace speech
