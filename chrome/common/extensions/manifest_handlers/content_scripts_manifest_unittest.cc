// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "chrome/common/webui_url_constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/script_constants.h"
#include "extensions/common/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace errors = manifest_errors;

class ContentScriptsManifestTest : public ChromeManifestTest {
};

TEST_F(ContentScriptsManifestTest, MatchPattern) {
  Testcase testcases[] = {
      // chrome:// urls are not allowed.
      Testcase("content_script_invalid_match_chrome_url.json",
               ErrorUtils::FormatErrorMessage(
                   errors::kInvalidMatch, base::NumberToString(0),
                   base::NumberToString(0),
                   URLPattern::GetParseResultString(
                       URLPattern::ParseResult::kInvalidScheme))),

      // chrome-extension:// urls are not allowed.
      Testcase("content_script_invalid_match_chrome_extension_url.json",
               ErrorUtils::FormatErrorMessage(
                   errors::kInvalidMatch, base::NumberToString(0),
                   base::NumberToString(0),
                   URLPattern::GetParseResultString(
                       URLPattern::ParseResult::kInvalidScheme))),

      // isolated-app:// urls are not allowed.
      Testcase("content_script_invalid_match_isolated_app_url.json",
               ErrorUtils::FormatErrorMessage(
                   errors::kInvalidMatch, base::NumberToString(0),
                   base::NumberToString(0),
                   URLPattern::GetParseResultString(
                       URLPattern::ParseResult::kInvalidScheme))),

      // Match paterns must be strings.
      Testcase("content_script_match_pattern_not_string.json",
               "Error at key 'content_scripts'. Parsing array failed at index "
               "0: Error at key 'matches': Parsing array failed at index 0: "
               "expected string, got integer")};
  RunTestcases(testcases, std::size(testcases), EXPECT_TYPE_ERROR);

  LoadAndExpectSuccess("ports_in_content_scripts.json");
}

TEST_F(ContentScriptsManifestTest, OnChromeUrlsWithFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kExtensionsOnChromeURLs);
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("content_script_invalid_match_chrome_url.json");
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

  const UserScriptList& user_scripts2 =
      ContentScriptsInfo::GetContentScripts(extension2.get());
  ASSERT_EQ(1u, user_scripts2.size());

  // The two content scripts should have different ids.
  EXPECT_NE(user_scripts2[0]->id(), user_scripts1[0]->id());
}

TEST_F(ContentScriptsManifestTest, FailLoadingNonUTF8Scripts) {
  base::FilePath install_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &install_dir));
  install_dir = install_dir.AppendASCII("extensions")
                    .AppendASCII("bad")
                    .AppendASCII("bad_encoding");

  std::string error;
  scoped_refptr<Extension> extension(
      file_util::LoadExtension(install_dir, mojom::ManifestLocation::kUnpacked,
                               Extension::NO_FLAGS, &error));
  ASSERT_TRUE(extension.get() == nullptr);
  ASSERT_STREQ(
      "Could not load file 'bad_encoding.js' for content script. "
      "It isn't UTF-8 encoded.",
      error.c_str());
}

TEST_F(ContentScriptsManifestTest, MatchOriginAsFallback) {
  scoped_refptr<const Extension> extension =
      LoadAndExpectSuccess("content_script_match_origin_as_fallback.json");
  ASSERT_TRUE(extension);
  const UserScriptList& user_scripts =
      ContentScriptsInfo::GetContentScripts(extension.get());
  ASSERT_EQ(7u, user_scripts.size());

  // The first script specifies `"match_origin_as_fallback": true`.
  EXPECT_EQ(MatchOriginAsFallbackBehavior::kAlways,
            user_scripts[0]->match_origin_as_fallback());
  // The second specifies `"match_origin_as_fallback": false`.
  EXPECT_EQ(MatchOriginAsFallbackBehavior::kNever,
            user_scripts[1]->match_origin_as_fallback());
  // The third specifies `"match_about_blank": true`.
  EXPECT_EQ(MatchOriginAsFallbackBehavior::kMatchForAboutSchemeAndClimbTree,
            user_scripts[2]->match_origin_as_fallback());
  // The fourth specifies `"match_about_blank": false`.
  EXPECT_EQ(MatchOriginAsFallbackBehavior::kNever,
            user_scripts[3]->match_origin_as_fallback());
  // The fifth specifies `"match_origin_as_fallback": false` *and*
  // `"match_about_blank": true`. "match_origin_as_fallback" takes precedence.
  EXPECT_EQ(MatchOriginAsFallbackBehavior::kNever,
            user_scripts[4]->match_origin_as_fallback());
  // The sixth specifies `"match_origin_as_fallback": true` *and*
  // `"match_about_blank": false`. "match_origin_as_fallback" takes precedence.
  EXPECT_EQ(MatchOriginAsFallbackBehavior::kAlways,
            user_scripts[5]->match_origin_as_fallback());
  // The seventh and final does not specify a value for either.
  EXPECT_EQ(MatchOriginAsFallbackBehavior::kNever,
            user_scripts[6]->match_origin_as_fallback());
}

TEST_F(ContentScriptsManifestTest, MatchOriginAsFallback_InvalidCases) {
  LoadAndExpectError(
      "content_script_match_origin_as_fallback_invalid_with_paths.json",
      errors::kMatchOriginAsFallbackCantHavePaths);
}

TEST_F(ContentScriptsManifestTest, ExecutionWorld) {
  scoped_refptr<const Extension> extension =
      LoadAndExpectSuccess("content_script_execution_world.json");
  const UserScriptList& user_scripts =
      ContentScriptsInfo::GetContentScripts(extension.get());
  ASSERT_EQ(3u, user_scripts.size());

  // Content scripts which don't specify an execution world will default to the
  // isolated world.
  EXPECT_EQ(mojom::ExecutionWorld::kIsolated,
            user_scripts[0]->execution_world());

  // Content scripts which specify an execution world will run on the world that
  // was specified.
  EXPECT_EQ(mojom::ExecutionWorld::kMain, user_scripts[1]->execution_world());
  EXPECT_EQ(mojom::ExecutionWorld::kIsolated,
            user_scripts[2]->execution_world());
}

TEST_F(ContentScriptsManifestTest, ExecutionWorld_InvalidForMV2) {
  scoped_refptr<const Extension> extension = LoadAndExpectWarning(
      "content_script_execution_world_warning_for_mv2.json",
      errors::kExecutionWorldRestrictedToMV3);
  const UserScriptList& user_scripts =
      ContentScriptsInfo::GetContentScripts(extension.get());
  ASSERT_EQ(1u, user_scripts.size());

  // The content script parsed from the manifest should be executing in the
  // isolated world.
  EXPECT_EQ(mojom::ExecutionWorld::kIsolated,
            user_scripts[0]->execution_world());
}

}  // namespace extensions
