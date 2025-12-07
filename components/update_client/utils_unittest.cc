// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/utils.h"

#include <string>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/update_client/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include <shlobj.h>
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include "base/test/scoped_locale.h"
#endif

namespace update_client {

TEST(UpdateClientUtils, VerifyFileHash256) {
  EXPECT_TRUE(VerifyFileHash256(
      GetTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"),
      std::string(
          "7ab32f071cd9b5ef8e0d7913be161f532d98b3e9fa284a7cd8059c3409ce0498")));

  EXPECT_TRUE(VerifyFileHash256(
      GetTestFilePath("empty_file"),
      std::string(
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")));

  EXPECT_FALSE(
      VerifyFileHash256(GetTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"),
                        std::string("")));

  EXPECT_FALSE(
      VerifyFileHash256(GetTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"),
                        std::string("abcd")));

  EXPECT_FALSE(VerifyFileHash256(
      GetTestFilePath("jebgalgnebhfojomionfpkfelancnnkf.crx"),
      std::string(
          "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
}

// Tests that the brand matches ^[a-zA-Z]{4}?$
TEST(UpdateClientUtils, IsValidBrand) {
  // The valid brand code must be empty or exactly 4 chars long.
  EXPECT_TRUE(IsValidBrand(std::string("")));
  EXPECT_TRUE(IsValidBrand(std::string("TEST")));
  EXPECT_TRUE(IsValidBrand(std::string("test")));
  EXPECT_TRUE(IsValidBrand(std::string("TEst")));

  EXPECT_FALSE(IsValidBrand(std::string("T")));      // Too short.
  EXPECT_FALSE(IsValidBrand(std::string("TE")));     //
  EXPECT_FALSE(IsValidBrand(std::string("TES")));    //
  EXPECT_FALSE(IsValidBrand(std::string("TESTS")));  // Too long.
  EXPECT_FALSE(IsValidBrand(std::string("TES1")));   // Has digit.
  EXPECT_FALSE(IsValidBrand(std::string(" TES")));   // Begins with white space.
  EXPECT_FALSE(IsValidBrand(std::string("TES ")));   // Ends with white space.
  EXPECT_FALSE(IsValidBrand(std::string("T ES")));   // Contains white space.
  EXPECT_FALSE(IsValidBrand(std::string("<TE")));    // Has <.
  EXPECT_FALSE(IsValidBrand(std::string("TE>")));    // Has >.
  EXPECT_FALSE(IsValidBrand(std::string("\"")));     // Has "
  EXPECT_FALSE(IsValidBrand(std::string("\\")));     // Has backslash.
  EXPECT_FALSE(IsValidBrand(std::string("\xaa")));   // Has non-ASCII char.
}

TEST(UpdateClientUtils, GetCrxComponentId) {
  static constexpr uint8_t kHash[16] = {
      0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
      0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
  };
  CrxComponent component;
  component.pk_hash = base::ToVector(kHash);

  EXPECT_EQ(std::string("abcdefghijklmnopabcdefghijklmnop"),
            GetCrxComponentID(component));
}

TEST(UpdateClientUtils, GetCrxIdFromPublicKeyHash) {
  static constexpr uint8_t kHash[16] = {
      0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
      0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
  };

  EXPECT_EQ(std::string("abcdefghijklmnopabcdefghijklmnop"),
            GetCrxIdFromPublicKeyHash(kHash));
}

// Tests that the name of an InstallerAttribute matches ^[-_=a-zA-Z0-9]{1,256}$
TEST(UpdateClientUtils, IsValidInstallerAttributeName) {
  // Test the length boundaries.
  EXPECT_FALSE(IsValidInstallerAttribute(
      make_pair(std::string(0, 'a'), std::string("value"))));
  EXPECT_TRUE(IsValidInstallerAttribute(
      make_pair(std::string(1, 'a'), std::string("value"))));
  EXPECT_TRUE(IsValidInstallerAttribute(
      make_pair(std::string(256, 'a'), std::string("value"))));
  EXPECT_FALSE(IsValidInstallerAttribute(
      make_pair(std::string(257, 'a'), std::string("value"))));

  const char* const valid_names[] = {"A", "Z", "a", "a-b", "A_B",
                                     "z", "0", "9", "-_"};
  for (const char* name : valid_names) {
    EXPECT_TRUE(IsValidInstallerAttribute(
        make_pair(std::string(name), std::string("value"))));
  }

  const char* const invalid_names[] = {
      "",   "a=1", " name", "name ", "na me", "<name", "name>",
      "\"", "\\",  "\xaa",  ".",     ",",     ";",     "+"};
  for (const char* name : invalid_names) {
    EXPECT_FALSE(IsValidInstallerAttribute(
        make_pair(std::string(name), std::string("value"))));
  }
}

// Tests that the value of an InstallerAttribute matches
// ^[-.,;+_=$a-zA-Z0-9]{0,256}$
TEST(UpdateClientUtils, IsValidInstallerAttributeValue) {
  // Test the length boundaries.
  EXPECT_TRUE(IsValidInstallerAttribute(
      make_pair(std::string("name"), std::string(0, 'a'))));
  EXPECT_TRUE(IsValidInstallerAttribute(
      make_pair(std::string("name"), std::string(256, 'a'))));
  EXPECT_FALSE(IsValidInstallerAttribute(
      make_pair(std::string("name"), std::string(257, 'a'))));

  const char* const valid_values[] = {"",  "a=1", "A", "Z",       "a",
                                      "z", "0",   "9", "-.,;+_=$"};
  for (const char* value : valid_values) {
    EXPECT_TRUE(IsValidInstallerAttribute(
        make_pair(std::string("name"), std::string(value))));
  }

  const char* const invalid_values[] = {" ap", "ap ", "a p", "<ap",
                                        "ap>", "\"",  "\\",  "\xaa"};
  for (const char* value : invalid_values) {
    EXPECT_FALSE(IsValidInstallerAttribute(
        make_pair(std::string("name"), std::string(value))));
  }
}

TEST(UpdateClientUtils, RemoveUnsecureUrls) {
  std::vector<GURL> urls = {GURL("http://foo"), GURL("https://foo")};
  RemoveUnsecureUrls(&urls);
  EXPECT_EQ(1u, urls.size());
  EXPECT_EQ(urls[0], GURL("https://foo"));

  urls = {GURL("https://foo"), GURL("http://foo")};
  RemoveUnsecureUrls(&urls);
  EXPECT_EQ(1u, urls.size());
  EXPECT_EQ(urls[0], GURL("https://foo"));

  urls = {GURL("https://foo"), GURL("https://bar")};
  RemoveUnsecureUrls(&urls);
  EXPECT_EQ(2u, urls.size());
  EXPECT_EQ(urls[0], GURL("https://foo"));
  EXPECT_EQ(urls[1], GURL("https://bar"));

  urls = {GURL("http://foo")};
  RemoveUnsecureUrls(&urls);
  EXPECT_EQ(0u, urls.size());

  urls = {GURL("http://foo"), GURL("http://bar")};
  RemoveUnsecureUrls(&urls);
  EXPECT_EQ(0u, urls.size());
}

TEST(UpdateClientUtils, GetArchitecture) {
  const std::string arch = GetArchitecture();

#if BUILDFLAG(IS_WIN)
  EXPECT_TRUE(arch == kArchIntel || arch == kArchAmd64 || arch == kArchArm64)
      << arch;
#endif  // BUILDFLAG(IS_WIN)
}

namespace {
#if BUILDFLAG(IS_WIN)
base::FilePath CopyCmdExe(const base::FilePath& under_dir) {
  static constexpr wchar_t kCmdExe[] = L"cmd.exe";

  base::FilePath system_path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SYSTEM, &system_path));

  const base::FilePath cmd_exe_path = under_dir.Append(kCmdExe);
  EXPECT_TRUE(base::CopyFile(system_path.Append(kCmdExe), cmd_exe_path));
  return cmd_exe_path;
}
#endif  // BUILDFLAG(IS_WIN)
}  // namespace

TEST(UpdateClientUtils, RetryFileOperation) {
  base::FilePath tempdir;
  ASSERT_TRUE(base::CreateNewTempDirectory(
      FILE_PATH_LITERAL("Test_RetryFileOperation"), &tempdir));

#if BUILDFLAG(IS_WIN)
  // Launch a process that runs for 3 seconds.
  ASSERT_TRUE(
      base::LaunchProcess(
          base::StrCat({CopyCmdExe(tempdir).value(), L" /c \"timeout 3\""}), {})
          .IsValid());

  // Trying to delete once fails, because the process is running within
  // `tempdir`.
  ASSERT_FALSE(RetryFileOperation(&base::DeletePathRecursively, tempdir, 1,
                                  base::Seconds(1)));
#endif  // BUILDFLAG(IS_WIN)

  // Deleting with retries works.
  ASSERT_TRUE(RetryFileOperation(&base::DeletePathRecursively, tempdir));
}

struct UpdateClientUtilsUTF8StringTypeTestCase {
  const base::FilePath::StringType stringtype;
  const std::string utf8;
};

class UpdateClientUtilsUTF8StringTypeTest
    : public ::testing::TestWithParam<UpdateClientUtilsUTF8StringTypeTestCase> {
#if !defined(SYSTEM_NATIVE_UTF8) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))
 protected:
  base::ScopedLocale locale_{"en_US.UTF-8"};
#endif
};

INSTANTIATE_TEST_SUITE_P(
    UpdateClientUtilsUTF8StringTypeTestCases,
    UpdateClientUtilsUTF8StringTypeTest,
    ::testing::ValuesIn(std::vector<UpdateClientUtilsUTF8StringTypeTestCase>{
        {FILE_PATH_LITERAL("foo.txt"), "foo.txt"},

        // "aeo" with accents. Use http://0xcc.net/jsescape/ to decode them.
        {FILE_PATH_LITERAL("\u00E0\u00E8\u00F2.txt"),
         "\xC3\xA0\xC3\xA8\xC3\xB2.txt"},

        // Full-width "ABC".
        {FILE_PATH_LITERAL("\uFF21\uFF22\uFF23.txt"),
         "\xEF\xBC\xA1\xEF\xBC\xA2\xEF\xBC\xA3.txt"},
    }));

TEST_P(UpdateClientUtilsUTF8StringTypeTest, UTF8ToStringType) {
  EXPECT_EQ(UTF8ToStringType(GetParam().utf8), GetParam().stringtype);
}

TEST_P(UpdateClientUtilsUTF8StringTypeTest, StringTypeToUTF8) {
  EXPECT_EQ(StringTypeToUTF8(GetParam().stringtype), GetParam().utf8);
}

}  // namespace update_client
