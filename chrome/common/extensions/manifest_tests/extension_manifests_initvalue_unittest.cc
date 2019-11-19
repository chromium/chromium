// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

// The ID of test manifests requiring allowlisting.
const char kAllowlistID[] = "lmadimbbgapmngbiclpjjngmdickadpl";

}  // namespace

namespace errors = manifest_errors;
namespace keys = manifest_keys;

class InitValueManifestTest : public ChromeManifestTest {
};

TEST_F(InitValueManifestTest, InitFromValueInvalid) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(kAllowlistID);
  Testcase testcases[] = {
      Testcase("init_invalid_version_missing.json", errors::kInvalidVersion),
      Testcase("init_invalid_version_invalid.json", errors::kInvalidVersion),
      Testcase("init_invalid_version_name_invalid.json",
               errors::kInvalidVersionName),
      Testcase("init_invalid_name_missing.json", errors::kInvalidName),
      Testcase("init_invalid_name_invalid.json", errors::kInvalidName),
      Testcase("init_invalid_description_invalid.json",
               errors::kInvalidDescription),
      Testcase("init_invalid_icons_invalid.json", errors::kInvalidIcons),
      Testcase("init_invalid_icons_path_invalid.json",
               errors::kInvalidIconPath),
      Testcase("init_invalid_script_invalid.json",
               errors::kInvalidContentScriptsList),
      Testcase("init_invalid_script_item_invalid.json",
               errors::kInvalidContentScript),
      Testcase("init_invalid_script_matches_missing.json",
               errors::kInvalidMatches),
      Testcase("init_invalid_script_matches_invalid.json",
               errors::kInvalidMatches),
      Testcase("init_invalid_script_matches_empty.json",
               errors::kInvalidMatchCount),
      Testcase("init_invalid_script_match_item_invalid.json",
               errors::kInvalidMatch),
      Testcase("init_invalid_script_match_item_invalid_2.json",
               errors::kInvalidMatch),
      Testcase("init_invalid_script_files_missing.json", errors::kMissingFile),
      Testcase("init_invalid_files_js_invalid.json", errors::kInvalidJsList),
      Testcase("init_invalid_files_empty.json", errors::kMissingFile),
      Testcase("init_invalid_files_js_empty_css_missing.json",
               errors::kMissingFile),
      Testcase("init_invalid_files_js_item_invalid.json", errors::kInvalidJs),
      Testcase("init_invalid_files_css_invalid.json", errors::kInvalidCssList),
      Testcase("init_invalid_files_css_item_invalid.json", errors::kInvalidCss),
      Testcase("init_invalid_permissions_invalid.json",
               errors::kInvalidPermissions),
      Testcase("init_invalid_host_permissions_invalid.json",
               errors::kInvalidHostPermissions),
      Testcase("init_invalid_permissions_item_invalid.json",
               errors::kInvalidPermission),
      Testcase("init_invalid_options_url_invalid.json",
               errors::kInvalidOptionsPage),
      Testcase("init_invalid_locale_invalid.json",
               errors::kInvalidDefaultLocale),
      Testcase("init_invalid_locale_empty.json", errors::kInvalidDefaultLocale),
      Testcase("init_invalid_min_chrome_invalid.json",
               errors::kInvalidMinimumChromeVersion),
      Testcase("init_invalid_chrome_version_too_low.json",
               errors::kChromeVersionTooLow),
      Testcase("init_invalid_short_name_empty.json", errors::kInvalidShortName),
      Testcase("init_invalid_short_name_type.json", errors::kInvalidShortName),
  };

  RunTestcases(testcases, base::size(testcases), EXPECT_TYPE_ERROR);
}

TEST_F(InitValueManifestTest, InitFromValueValid) {
  scoped_refptr<Extension> extension(LoadAndExpectSuccess(
      "init_valid_minimal.json"));

  base::FilePath path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("extensions");

  EXPECT_TRUE(crx_file::id_util::IdIsValid(extension->id()));
  EXPECT_EQ("1.0.0.0", extension->VersionString());
  EXPECT_EQ("my extension", extension->name());
  EXPECT_EQ(extension->name(), extension->short_name());
  EXPECT_EQ(extension->id(), extension->url().host());
  EXPECT_EQ(extension->path(), path);
  EXPECT_EQ(path, extension->path());

  // Test permissions scheme.
  // We allow unknown API permissions, so this will be valid until we better
  // distinguish between API and host permissions.
  LoadAndExpectSuccess("init_valid_permissions.json");

  // Test with an options page.
  extension = LoadAndExpectSuccess("init_valid_options.json");
  EXPECT_EQ(extensions::kExtensionScheme,
            OptionsPageInfo::GetOptionsPage(extension.get()).scheme());
  EXPECT_EQ("/options.html",
            OptionsPageInfo::GetOptionsPage(extension.get()).path());

  // Test optional short_name field.
  extension = LoadAndExpectSuccess("init_valid_short_name.json");
  EXPECT_EQ("a very descriptive extension name", extension->name());
  EXPECT_EQ("concise name", extension->short_name());

  // Test optional version_name field.
  extension = LoadAndExpectSuccess("init_valid_version_name.json");
  EXPECT_EQ("1.0.0.0", extension->VersionString());
  EXPECT_EQ("1.0 alpha", extension->GetVersionForDisplay());

  Testcase testcases[] = {
    // Test with a minimum_chrome_version.
    Testcase("init_valid_minimum_chrome.json"),

    // Test a hosted app with a minimum_chrome_version.
    Testcase("init_valid_app_minimum_chrome.json"),

    // Test a hosted app with a requirements section.
    Testcase("init_valid_app_requirements.json"),

    // Verify empty permission settings are considered valid.
    Testcase("init_valid_permissions_empty.json"),

    // We allow unknown API permissions, so this will be valid until we better
    // distinguish between API and host permissions.
    Testcase("init_valid_permissions_unknown.json")
  };

  RunTestcases(testcases, base::size(testcases), EXPECT_TYPE_SUCCESS);
}

TEST_F(InitValueManifestTest, InitFromValueValidNameInRTL) {
  std::string locale = l10n_util::GetApplicationLocale("");
  base::i18n::SetICUDefaultLocale("he");

  // No strong RTL characters in name.
  scoped_refptr<Extension> extension(LoadAndExpectSuccess(
      "init_valid_name_no_rtl.json"));

  base::string16 localized_name(base::ASCIIToUTF16("Dictionary (by Google)"));
  base::i18n::AdjustStringForLocaleDirection(&localized_name);
  EXPECT_EQ(localized_name, base::UTF8ToUTF16(extension->name()));

  // Strong RTL characters in name.
  extension = LoadAndExpectSuccess("init_valid_name_strong_rtl.json");

  localized_name = base::WideToUTF16(L"Dictionary (\x05D1\x05D2" L" Google)");
  base::i18n::AdjustStringForLocaleDirection(&localized_name);
  EXPECT_EQ(localized_name, base::UTF8ToUTF16(extension->name()));

  // Reset locale.
  base::i18n::SetICUDefaultLocale(locale);
}

}  // namespace extensions
