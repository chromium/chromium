// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "chrome/common/webui_url_constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace errors = manifest_errors;

class ContentScriptsManifestTest : public ChromeManifestTest {
};

TEST_F(ContentScriptsManifestTest, MatchPattern) {
  Testcase testcases[] = {
      // chrome:// urls are not allowed.
      Testcase("content_script_chrome_url_invalid.json",
               ErrorUtils::FormatErrorMessage(
                   errors::kInvalidMatch, base::NumberToString(0),
                   base::NumberToString(0),
                   URLPattern::GetParseResultString(
                       URLPattern::ParseResult::kInvalidScheme))),

      // Match paterns must be strings.
      Testcase("content_script_match_pattern_not_string.json",
               ErrorUtils::FormatErrorMessage(
                   errors::kInvalidMatch, base::NumberToString(0),
                   base::NumberToString(0), errors::kExpectString))};
  RunTestcases(testcases, base::size(testcases), EXPECT_TYPE_ERROR);

  LoadAndExpectSuccess("ports_in_content_scripts.json");
}

TEST_F(ContentScriptsManifestTest, OnChromeUrlsWithFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kExtensionsOnChromeURLs);
  scoped_refptr<Extension> extension =
    LoadAndExpectSuccess("content_script_chrome_url_invalid.json");
  const GURL newtab_url(chrome::kChromeUINewTabURL);
  EXPECT_TRUE(
      ContentScriptsInfo::ExtensionHasScriptAtURL(extension.get(), newtab_url));
}

TEST_F(ContentScriptsManifestTest, ScriptableHosts) {
  // TODO(yoz): Test GetScriptableHosts.
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("content_script_yahoo.json");
  URLPatternSet scriptable_hosts =
      ContentScriptsInfo::GetScriptableHosts(extension.get());

  URLPatternSet expected;
  expected.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://yahoo.com/*"));

  EXPECT_EQ(expected, scriptable_hosts);
}

TEST_F(ContentScriptsManifestTest, ContentScriptIds) {
  scoped_refptr<Extension> extension1 =
      LoadAndExpectSuccess("content_script_yahoo.json");
  scoped_refptr<Extension> extension2 =
      LoadAndExpectSuccess("content_script_yahoo.json");
  const UserScriptList& user_scripts1 =
      ContentScriptsInfo::GetContentScripts(extension1.get());
  ASSERT_EQ(1u, user_scripts1.size());
  int id = user_scripts1[0]->id();
  const UserScriptList& user_scripts2 =
      ContentScriptsInfo::GetContentScripts(extension2.get());
  ASSERT_EQ(1u, user_scripts2.size());
  // The id of the content script should be one higher than the previous.
  EXPECT_EQ(id + 1, user_scripts2[0]->id());
}

TEST_F(ContentScriptsManifestTest, FailLoadingNonUTF8Scripts) {
  base::FilePath install_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &install_dir));
  install_dir = install_dir.AppendASCII("extensions")
                    .AppendASCII("bad")
                    .AppendASCII("bad_encoding");

  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      install_dir, Manifest::UNPACKED, Extension::NO_FLAGS, &error));
  ASSERT_TRUE(extension.get() == NULL);
  ASSERT_STREQ(
      "Could not load file 'bad_encoding.js' for content script. "
      "It isn't UTF-8 encoded.",
      error.c_str());
}

}  // namespace extensions
