// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome_launcher.h"

#include <memory>

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ProcessExtensions, NoExtension) {
  Switches switches;
  std::vector<std::string> extensions;
  base::FilePath extension_dir;
  std::vector<std::string> bg_pages;
  Status status = internal::ProcessExtensions(extensions, extension_dir,
                                              false, &switches, &bg_pages);
  ASSERT_TRUE(status.IsOk());
  ASSERT_FALSE(switches.HasSwitch("load-extension"));
  ASSERT_EQ(0u, bg_pages.size());
}

bool AddExtensionForInstall(const std::string& relative_path,
                            std::vector<std::string>* extensions) {
  base::FilePath source_root;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root);
  base::FilePath crx_file_path = source_root.AppendASCII(
      "chrome/test/data/chromedriver/" + relative_path);
  std::string crx_contents;
  if (!base::ReadFileToString(crx_file_path, &crx_contents))
    return false;

  std::string crx_encoded;
  base::Base64Encode(crx_contents, &crx_encoded);
  extensions->push_back(crx_encoded);
  return true;
}

TEST(ProcessExtensions, GenerateIds) {
  std::vector<std::string> extensions;
  base::ScopedTempDir extension_dir;
  Switches switches;
  std::vector<std::string> bg_pages;

  ASSERT_TRUE(AddExtensionForInstall("no_key_in_manifest.crx", &extensions));
  ASSERT_TRUE(AddExtensionForInstall("same_key_as_header.crx", &extensions));
  ASSERT_TRUE(AddExtensionForInstall("diff_key_from_header.crx", &extensions));

  ASSERT_TRUE(extension_dir.CreateUniqueTempDir());

  Status status = internal::ProcessExtensions(
      extensions, extension_dir.GetPath(), false, &switches, &bg_pages);

  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(3u, bg_pages.size());
  ASSERT_EQ("chrome-extension://llphabdmknikmpmkioimgdfbohinlekl/"
            "_generated_background_page.html", bg_pages[0]);
  ASSERT_EQ("chrome-extension://dfdeoklpcichfcnoaomfpagfiibhomnh/"
            "_generated_background_page.html", bg_pages[1]);
  ASSERT_EQ("chrome-extension://ioccpomhcpklobebcbeohnmffkmcokbm/"
            "_generated_background_page.html", bg_pages[2]);
}

TEST(ProcessExtensions, GenerateIdCrx3) {
  std::vector<std::string> extensions;
  base::ScopedTempDir extension_dir;
  Switches switches;
  std::vector<std::string> bg_pages;

  ASSERT_TRUE(AddExtensionForInstall("same_key_as_header.crx3", &extensions));

  ASSERT_TRUE(extension_dir.CreateUniqueTempDir());

  Status status = internal::ProcessExtensions(
      extensions, extension_dir.GetPath(), false, &switches, &bg_pages);

  ASSERT_EQ(kOk, status.code()) << status.message();
  ASSERT_EQ(1u, bg_pages.size());
  ASSERT_EQ(
      "chrome-extension://dfdeoklpcichfcnoaomfpagfiibhomnh/"
      "_generated_background_page.html",
      bg_pages[0]);
}

TEST(ProcessExtensions, SingleExtensionWithBgPage) {
  std::vector<std::string> extensions;
  ASSERT_TRUE(AddExtensionForInstall("ext_slow_loader.crx", &extensions));

  base::ScopedTempDir extension_dir;
  ASSERT_TRUE(extension_dir.CreateUniqueTempDir());

  Switches switches;
  std::vector<std::string> bg_pages;
  Status status = internal::ProcessExtensions(
      extensions, extension_dir.GetPath(), false, &switches, &bg_pages);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(switches.HasSwitch("load-extension"));
  base::FilePath temp_ext_path(switches.GetSwitchValueNative("load-extension"));
  ASSERT_TRUE(base::PathExists(temp_ext_path));
  std::string manifest_txt;
  ASSERT_TRUE(base::ReadFileToString(
      temp_ext_path.AppendASCII("manifest.json"), &manifest_txt));
  std::unique_ptr<base::Value> manifest =
      base::JSONReader::ReadDeprecated(manifest_txt);
  ASSERT_TRUE(manifest);
  base::DictionaryValue* manifest_dict = NULL;
  ASSERT_TRUE(manifest->GetAsDictionary(&manifest_dict));
  std::string key;
  ASSERT_TRUE(manifest_dict->GetString("key", &key));
  ASSERT_EQ(
      "MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAxbE7gPHcoZQX7Nv1Tpq8Osz3hhC"
      "fUPZpMCcsYALXYsICUMdFNPvsq4AsfzcIJN2Qc6C9GwlDgBEYQgC6zD9ULoSnHu3iJem49b"
      "yAzBciO5zMCXBKHx3HN63QJBOKXhIZMdpZePx22fnZjfeLA6zOnGoH6F0YXX+uNsU5qgV9l"
      "G4je2XwU050l1u1DAcuzrPVcma32rbfXC3mwZjQxgghCqn/hI0/OBcCU37sWBgPci8uk4Wd"
      "Ee1frQTN2vpQJ0fJl0SaPGCNmpmJ708n/qwpVvmqK99JPznJoT5dahuZoTyoGl092sbpstY"
      "H+nQ/66yDoztwerywl+lRYgMJstLlRUgrTwmaA4klweuctSfSA40+2kMWSOL+myCyziZ7Ec"
      "E98ZEGtLaPW5OYxO8r4aQz11q4NyGL/wVDevWHJmprlrjP/zd3GAo6/nwhu+WvdtHlnkGHr"
      "OVtOdjUPhL+FCvqaj/sMt3ELgIK5h8nt42xPxf8P/cYV+aRuZCd2hAqiNXOQ6HLU0TIdc2X"
      "dnQtotb3/wuPvRFXqU0o0SAeEwGRoOxr6WqkOLuBuvwNtcKc/cCqxWMlcnId5TWX+tPEpUM"
      "4Imgbf6jIB2FPpSXQMLHQkag+k95aiXqkpirlhUaBA5yrClFLjw+Ld2yqJfh961yncxF+IB"
      "EmivSdNH0cYZBISf8CAwEAAQ==",
      key);
  ASSERT_EQ(1u, bg_pages.size());
  ASSERT_EQ(
      "chrome-extension://ejapkfeonjhabbbnlpmcgholnoicapdb/"
      "_generated_background_page.html",
      bg_pages[0]);
}

TEST(ProcessExtensions, MultipleExtensionsNoBgPages) {
  std::vector<std::string> extensions;
  ASSERT_TRUE(AddExtensionForInstall("ext_test_1.crx", &extensions));
  ASSERT_TRUE(AddExtensionForInstall("ext_test_2.crx", &extensions));

  base::ScopedTempDir extension_dir;
  ASSERT_TRUE(extension_dir.CreateUniqueTempDir());

  Switches switches;
  std::vector<std::string> bg_pages;
  Status status = internal::ProcessExtensions(
      extensions, extension_dir.GetPath(), false, &switches, &bg_pages);
  ASSERT_TRUE(status.IsOk());
  ASSERT_TRUE(switches.HasSwitch("load-extension"));
  base::CommandLine::StringType ext_paths =
      switches.GetSwitchValueNative("load-extension");
  std::vector<base::CommandLine::StringType> ext_path_list =
      base::SplitString(ext_paths, base::CommandLine::StringType(1, ','),
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, ext_path_list.size());
  ASSERT_TRUE(base::PathExists(base::FilePath(ext_path_list[0])));
  ASSERT_TRUE(base::PathExists(base::FilePath(ext_path_list[1])));
  ASSERT_EQ(0u, bg_pages.size());
}

TEST(ProcessExtensions, CommandLineExtensions) {
  std::vector<std::string> extensions;
  ASSERT_TRUE(AddExtensionForInstall("ext_test_1.crx", &extensions));
  base::ScopedTempDir extension_dir;
  ASSERT_TRUE(extension_dir.CreateUniqueTempDir());

  Switches switches;
  switches.SetSwitch("load-extension", "/a");
  std::vector<std::string> bg_pages;
  Status status = internal::ProcessExtensions(
      extensions, extension_dir.GetPath(), false, &switches, &bg_pages);
  ASSERT_EQ(kOk, status.code());
  base::FilePath::StringType load = switches.GetSwitchValueNative(
      "load-extension");
  ASSERT_EQ(FILE_PATH_LITERAL("/a,"), load.substr(0, 3));
  ASSERT_TRUE(base::PathExists(base::FilePath(load.substr(3))));
}

namespace {

void AssertEQ(const base::DictionaryValue& dict, const std::string& key,
              const char* expected_value) {
  std::string value;
  ASSERT_TRUE(dict.GetString(key, &value));
  ASSERT_STREQ(value.c_str(), expected_value);
}

}  // namespace

TEST(PrepareUserDataDir, CustomPrefs) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::DictionaryValue prefs;
  prefs.SetString("myPrefsKey", "ok");
  prefs.SetKey("pref.sub", base::Value("1"));
  base::DictionaryValue local_state;
  local_state.SetString("myLocalKey", "ok");
  local_state.SetKey("local.state.sub", base::Value("2"));
  Status status =
      internal::PrepareUserDataDir(temp_dir.GetPath(), &prefs, &local_state);
  ASSERT_EQ(kOk, status.code());

  base::FilePath prefs_file = temp_dir.GetPath()
                                  .AppendASCII(chrome::kInitialProfile)
                                  .Append(chrome::kPreferencesFilename);
  std::string prefs_str;
  ASSERT_TRUE(base::ReadFileToString(prefs_file, &prefs_str));
  std::unique_ptr<base::Value> prefs_value =
      base::JSONReader::ReadDeprecated(prefs_str);
  const base::DictionaryValue* prefs_dict = NULL;
  ASSERT_TRUE(prefs_value->GetAsDictionary(&prefs_dict));
  AssertEQ(*prefs_dict, "myPrefsKey", "ok");
  AssertEQ(*prefs_dict, "pref.sub", "1");

  base::FilePath local_state_file =
      temp_dir.GetPath().Append(chrome::kLocalStateFilename);
  std::string local_state_str;
  ASSERT_TRUE(base::ReadFileToString(local_state_file, &local_state_str));
  std::unique_ptr<base::Value> local_state_value =
      base::JSONReader::ReadDeprecated(local_state_str);
  const base::DictionaryValue* local_state_dict = NULL;
  ASSERT_TRUE(local_state_value->GetAsDictionary(&local_state_dict));
  AssertEQ(*local_state_dict, "myLocalKey", "ok");
  AssertEQ(*local_state_dict, "local.state.sub", "2");
}

TEST(DesktopLauncher, ParseDevToolsActivePortFile_Success) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  char data[] = "12345\nblahblah";
  base::FilePath temp_file =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("DevToolsActivePort"));
  ASSERT_TRUE(base::WriteFile(temp_file, data, strlen(data)));
  int port;
  ASSERT_TRUE(
      internal::ParseDevToolsActivePortFile(temp_dir.GetPath(), &port).IsOk());
  ASSERT_EQ(port, 12345);
}

TEST(DesktopLauncher, ParseDevToolsActivePortFile_NoNewline) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  char data[] = "12345";
  base::FilePath temp_file =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("DevToolsActivePort"));
  ASSERT_TRUE(base::WriteFile(temp_file, data, strlen(data)));
  int port = 1111;
  ASSERT_FALSE(
      internal::ParseDevToolsActivePortFile(temp_dir.GetPath(), &port).IsOk());
  ASSERT_EQ(port, 1111);
}

TEST(DesktopLauncher, ParseDevToolsActivePortFile_NotNumber) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  char data[] = "12345asdf\nblahblah";
  base::FilePath temp_file =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("DevToolsActivePort"));
  ASSERT_TRUE(base::WriteFile(temp_file, data, strlen(data)));
  int port;
  ASSERT_FALSE(
      internal::ParseDevToolsActivePortFile(temp_dir.GetPath(), &port).IsOk());
}

TEST(DesktopLauncher, ParseDevToolsActivePortFile_NoFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_file =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("DevToolsActivePort"));
  int port = 1111;
  ASSERT_FALSE(
      internal::ParseDevToolsActivePortFile(temp_dir.GetPath(), &port).IsOk());
  ASSERT_EQ(port, 1111);
}

TEST(DesktopLauncher, RemoveOldDevToolsActivePortFile_Success) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_file =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("DevToolsActivePort"));
  char data[] = "12345asdf\nblahblah";
  base::WriteFile(temp_file, data, strlen(data));
  ASSERT_TRUE(
      internal::RemoveOldDevToolsActivePortFile(temp_dir.GetPath()).IsOk());
  ASSERT_FALSE(base::PathExists(temp_file));
  ASSERT_TRUE(base::PathExists(temp_dir.GetPath()));
}

#if defined(OS_WIN)
TEST(DesktopLauncher, RemoveOldDevToolsActivePortFile_Failure) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_file =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("DevToolsActivePort"));
  FILE* fd = base::OpenFile(temp_file, "w");
  ASSERT_FALSE(
      internal::RemoveOldDevToolsActivePortFile(temp_dir.GetPath()).IsOk());
  ASSERT_TRUE(base::PathExists(temp_file));
  base::CloseFile(fd);
}
#endif
