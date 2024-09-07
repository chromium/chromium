// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/update_client/utils.h"

#include <string>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/update_client/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include <shlobj.h>
#endif  // BUILDFLAG(IS_WIN)

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
  static const uint8_t kHash[16] = {
      0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
      0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
  };
  CrxComponent component;
  component.pk_hash.assign(kHash, kHash + sizeof(kHash));

  EXPECT_EQ(std::string("abcdefghijklmnopabcdefghijklmnop"),
            GetCrxComponentID(component));
}

TEST(UpdateClientUtils, GetCrxIdFromPublicKeyHash) {
  static const uint8_t kHash[16] = {
      0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
      0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
  };

  EXPECT_EQ(std::string("abcdefghijklmnopabcdefghijklmnop"),
            GetCrxIdFromPublicKeyHash({std::cbegin(kHash), std::cend(kHash)}));
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
  const GURL test1[] = {GURL("http://foo"), GURL("https://foo")};
  std::vector<GURL> urls(std::begin(test1), std::end(test1));
  RemoveUnsecureUrls(&urls);
  EXPECT_EQ(1u, urls.size());
  EXPECT_EQ(urls[0], GURL("https://foo"));

  const GURL test2[] = {GURL("https://foo"), GURL("http://foo")};
  urls.assign(std::begin(test2), std::end(test2));
  RemoveUnsecureUrls(&urls);
  EXPECT_EQ(1u, urls.size());
  EXPECT_EQ(urls[0], GURL("https://foo"));

  const GURL test3[] = {GURL("https://foo"), GURL("https://bar")};
  urls.assign(std::begin(test3), std::end(test3));
  RemoveUnsecureUrls(&urls);
  EXPECT_EQ(2u, urls.size());
  EXPECT_EQ(urls[0], GURL("https://foo"));
  EXPECT_EQ(urls[1], GURL("https://bar"));

  const GURL test4[] = {GURL("http://foo")};
  urls.assign(std::begin(test4), std::end(test4));
  RemoveUnsecureUrls(&urls);
  EXPECT_EQ(0u, urls.size());

  const GURL test5[] = {GURL("http://foo"), GURL("http://bar")};
  urls.assign(std::begin(test5), std::end(test5));
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
  constexpr wchar_t kCmdExe[] = L"cmd.exe";

  base::FilePath system_path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SYSTEM, &system_path));

  const base::FilePath cmd_exe_path = under_dir.Append(kCmdExe);
  EXPECT_TRUE(base::CopyFile(system_path.Append(kCmdExe), cmd_exe_path));
  return cmd_exe_path;
}
#endif  // BUILDFLAG(IS_WIN)
}  // namespace

TEST(UpdateClientUtils, RetryDeletePathRecursively) {
  base::FilePath tempdir;
  ASSERT_TRUE(base::CreateNewTempDirectory(
      FILE_PATH_LITERAL("Test_RetryDeletePathRecursively"), &tempdir));

#if BUILDFLAG(IS_WIN)
  // Launch a process that runs for 3 seconds.
  ASSERT_TRUE(
      base::LaunchProcess(
          base::StrCat({CopyCmdExe(tempdir).value(), L" /c \"timeout 3\""}), {})
          .IsValid());

  // Trying to delete once fails, because the process is running within
  // `tempdir`.
  ASSERT_FALSE(RetryDeletePathRecursivelyCustom(tempdir, 1, base::Seconds(1)));
#endif  // BUILDFLAG(IS_WIN)

  // Deleting with retries works.
  ASSERT_TRUE(RetryDeletePathRecursively(tempdir));
}

}  // namespace update_client
