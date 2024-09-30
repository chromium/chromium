// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/common/extension.h"

#include <stddef.h>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/command.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/test/test_extension_dir.h"
#include "net/base/mime_sniffer.h"
#include "net/dns/mock_host_resolver.h"
#include "skia/ext/image_operations.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

using base::FilePath;
using extension_test_util::LoadManifest;
using extension_test_util::LoadManifestStrict;
using extensions::mojom::ManifestLocation;

namespace extensions {

// We persist location values in the preferences, so this is a sanity test that
// someone doesn't accidentally change them.
TEST(ExtensionTest, LocationValuesTest) {
  ASSERT_EQ(0, static_cast<int>(ManifestLocation::kInvalidLocation));
  ASSERT_EQ(1, static_cast<int>(ManifestLocation::kInternal));
  ASSERT_EQ(2, static_cast<int>(ManifestLocation::kExternalPref));
  ASSERT_EQ(3, static_cast<int>(ManifestLocation::kExternalRegistry));
  ASSERT_EQ(4, static_cast<int>(ManifestLocation::kUnpacked));
  ASSERT_EQ(5, static_cast<int>(ManifestLocation::kComponent));
  ASSERT_EQ(6, static_cast<int>(ManifestLocation::kExternalPrefDownload));
  ASSERT_EQ(7, static_cast<int>(ManifestLocation::kExternalPolicyDownload));
  ASSERT_EQ(8, static_cast<int>(ManifestLocation::kCommandLine));
  ASSERT_EQ(9, static_cast<int>(ManifestLocation::kExternalPolicy));
  ASSERT_EQ(10, static_cast<int>(ManifestLocation::kExternalComponent));
}

TEST(ExtensionTest, LocationPriorityTest) {
  for (int i = 0; i <= static_cast<int>(ManifestLocation::kMaxValue); i++) {
    ManifestLocation loc = static_cast<ManifestLocation>(i);

    // kInvalidLocation is not a valid location.
    if (loc == ManifestLocation::kInvalidLocation)
      continue;

    // Comparing a location that has no rank will hit a CHECK. Do a
    // compare with every valid location, to be sure each one is covered.

    // Check that no install source can override a componenet extension.
    ASSERT_EQ(
        ManifestLocation::kComponent,
        Manifest::GetHigherPriorityLocation(ManifestLocation::kComponent, loc));
    ASSERT_EQ(
        ManifestLocation::kComponent,
        Manifest::GetHigherPriorityLocation(loc, ManifestLocation::kComponent));

    // Check that any source can override a user install. This might change
    // in the future, in which case this test should be updated.
    ASSERT_EQ(loc, Manifest::GetHigherPriorityLocation(
                       ManifestLocation::kInternal, loc));
    ASSERT_EQ(loc, Manifest::GetHigherPriorityLocation(
                       loc, ManifestLocation::kInternal));
  }

  // Check a few interesting cases that we know can happen:
  ASSERT_EQ(ManifestLocation::kExternalPolicyDownload,
            Manifest::GetHigherPriorityLocation(
                ManifestLocation::kExternalPolicyDownload,
                ManifestLocation::kExternalPref));

  ASSERT_EQ(ManifestLocation::kExternalPref,
            Manifest::GetHigherPriorityLocation(
                ManifestLocation::kInternal, ManifestLocation::kExternalPref));
}

TEST(ExtensionTest, EnsureNewLinesInExtensionNameAreCollapsed) {
  std::string unsanitized_name = "Test\n\n\n\n\n\n\n\n\n\n\n\nNew lines\u0085";
  auto manifest = base::Value::Dict()
                      .Set("name", unsanitized_name)
                      .Set("manifest_version", 2)
                      .Set("description", "some description")
                      .Set("version", "0.1");
  scoped_refptr<const Extension> extension =
      ExtensionBuilder().SetManifest(std::move(manifest)).Build();
  ASSERT_TRUE(extension.get());
  EXPECT_EQ("TestNew lines", extension->name());
  // Ensure that non-localized name is not sanitized.
  EXPECT_EQ(unsanitized_name, extension->non_localized_name());
}

TEST(ExtensionTest, EnsureWhitespacesInExtensionNameAreCollapsed) {
  std::string unsanitized_name = "Test                        Whitespace";
  auto manifest = base::Value::Dict()
                      .Set("name", unsanitized_name)
                      .Set("manifest_version", 2)
                      .Set("description", "some description")
                      .Set("version", "0.1");
  scoped_refptr<const Extension> extension =
      ExtensionBuilder().SetManifest(std::move(manifest)).Build();
  ASSERT_TRUE(extension.get());
  EXPECT_EQ("Test Whitespace", extension->name());
  // Ensure that non-localized name is not sanitized.
  EXPECT_EQ(unsanitized_name, extension->non_localized_name());
}

TEST(ExtensionTest, RTLNameInLTRLocale) {
  // Test the case when a directional override is the first character.
  auto run_rtl_test = [](const wchar_t* name, const wchar_t* expected) {
    SCOPED_TRACE(
        base::StringPrintf("Name: %ls, Expected: %ls", name, expected));
    auto manifest = base::Value::Dict()
                        .Set("name", base::WideToUTF8(name))
                        .Set("manifest_version", 2)
                        .Set("description", "some description")
                        .Set("version",
                             "0.1");  // <NOTE> Moved this here to avoid the
                                      // MergeManifest call.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder().SetManifest(std::move(manifest)).Build();
    ASSERT_TRUE(extension);
    const int kResourceId = IDS_EXTENSION_PERMISSIONS_PROMPT_TITLE;
    const std::u16string expected_utf16 = base::WideToUTF16(expected);
    EXPECT_EQ(l10n_util::GetStringFUTF16(kResourceId, expected_utf16),
              l10n_util::GetStringFUTF16(kResourceId,
                                         base::UTF8ToUTF16(extension->name())));
    EXPECT_EQ(base::WideToUTF8(expected), extension->name());
  };

  run_rtl_test(L"\x202emoc.elgoog", L"\x202emoc.elgoog\x202c");
  run_rtl_test(L"\x202egoogle\x202e.com/\x202eguest",
               L"\x202egoogle\x202e.com/\x202eguest\x202c\x202c\x202c");
  run_rtl_test(L"google\x202e.com", L"google\x202e.com\x202c");

  run_rtl_test(L"كبير Google التطبيق",
#if !BUILDFLAG(IS_WIN)
               L"\x200e\x202bكبير Google التطبيق\x202c\x200e");
#else
               // On Windows for an LTR locale, no changes to the string are
               // made.
               L"كبير Google التطبيق");
#endif  // !BUILDFLAG(IS_WIN)
}

TEST(ExtensionTest, GetResourceURLAndPath) {
  scoped_refptr<Extension> extension = LoadManifestStrict("empty_manifest",
      "empty.json");
  EXPECT_TRUE(extension.get());

  EXPECT_EQ(extension->url().spec() + "bar/baz.js",
            Extension::GetResourceURL(extension->url(), "bar/baz.js").spec());
  EXPECT_EQ(extension->url().spec() + "baz.js",
            Extension::GetResourceURL(extension->url(),
                                      "bar/../baz.js").spec());
  EXPECT_EQ(extension->url().spec() + "baz.js",
            Extension::GetResourceURL(extension->url(), "../baz.js").spec());

  // Test that absolute-looking paths ("/"-prefixed) are pasted correctly.
  EXPECT_EQ(extension->url().spec() + "test.html",
            extension->GetResourceURL("/test.html").spec());
}

TEST(ExtensionTest, GetResource) {
  const FilePath valid_path_test_cases[] = {
    FilePath(FILE_PATH_LITERAL("manifest.json")),
    FilePath(FILE_PATH_LITERAL("a/b/c/manifest.json")),
    FilePath(FILE_PATH_LITERAL("com/manifest.json")),
    FilePath(FILE_PATH_LITERAL("lpt/manifest.json")),
  };
  const FilePath invalid_path_test_cases[] = {
    // Directory name
    FilePath(FILE_PATH_LITERAL("src/")),
    // Contains a drive letter specification.
    FilePath(FILE_PATH_LITERAL("C:\\manifest.json")),
    // Use backslash '\\' as separator.
    FilePath(FILE_PATH_LITERAL("a\\b\\c\\manifest.json")),
    // Reserved Characters with extension
    FilePath(FILE_PATH_LITERAL("mani>fest.json")),
    FilePath(FILE_PATH_LITERAL("mani<fest.json")),
    FilePath(FILE_PATH_LITERAL("mani*fest.json")),
    FilePath(FILE_PATH_LITERAL("mani:fest.json")),
    FilePath(FILE_PATH_LITERAL("mani?fest.json")),
    FilePath(FILE_PATH_LITERAL("mani|fest.json")),
    // Reserved Characters without extension
    FilePath(FILE_PATH_LITERAL("mani>fest")),
    FilePath(FILE_PATH_LITERAL("mani<fest")),
    FilePath(FILE_PATH_LITERAL("mani*fest")),
    FilePath(FILE_PATH_LITERAL("mani:fest")),
    FilePath(FILE_PATH_LITERAL("mani?fest")),
    FilePath(FILE_PATH_LITERAL("mani|fest")),
    // Reserved Names with extension.
    FilePath(FILE_PATH_LITERAL("com1.json")),
    FilePath(FILE_PATH_LITERAL("com9.json")),
    FilePath(FILE_PATH_LITERAL("LPT1.json")),
    FilePath(FILE_PATH_LITERAL("LPT9.json")),
    FilePath(FILE_PATH_LITERAL("CON.json")),
    FilePath(FILE_PATH_LITERAL("PRN.json")),
    FilePath(FILE_PATH_LITERAL("AUX.json")),
    FilePath(FILE_PATH_LITERAL("NUL.json")),
    // Reserved Names without extension.
    FilePath(FILE_PATH_LITERAL("com1")),
    FilePath(FILE_PATH_LITERAL("com9")),
    FilePath(FILE_PATH_LITERAL("LPT1")),
    FilePath(FILE_PATH_LITERAL("LPT9")),
    FilePath(FILE_PATH_LITERAL("CON")),
    FilePath(FILE_PATH_LITERAL("PRN")),
    FilePath(FILE_PATH_LITERAL("AUX")),
    FilePath(FILE_PATH_LITERAL("NUL")),
    // Reserved Names as directory.
    FilePath(FILE_PATH_LITERAL("com1/manifest.json")),
    FilePath(FILE_PATH_LITERAL("com9/manifest.json")),
    FilePath(FILE_PATH_LITERAL("LPT1/manifest.json")),
    FilePath(FILE_PATH_LITERAL("LPT9/manifest.json")),
    FilePath(FILE_PATH_LITERAL("CON/manifest.json")),
    FilePath(FILE_PATH_LITERAL("PRN/manifest.json")),
    FilePath(FILE_PATH_LITERAL("AUX/manifest.json")),
    FilePath(FILE_PATH_LITERAL("NUL/manifest.json")),
  };

  scoped_refptr<Extension> extension = LoadManifestStrict("empty_manifest",
      "empty.json");
  EXPECT_TRUE(extension.get());
  for (size_t i = 0; i < std::size(valid_path_test_cases); ++i)
    EXPECT_TRUE(!extension->GetResource(valid_path_test_cases[i]).empty());
  for (size_t i = 0; i < std::size(invalid_path_test_cases); ++i)
    EXPECT_TRUE(extension->GetResource(invalid_path_test_cases[i]).empty());
}

TEST(ExtensionTest, GetAbsolutePathNoError) {
  scoped_refptr<Extension> extension = LoadManifestStrict("absolute_path",
      "absolute.json");
  EXPECT_TRUE(extension.get());
  std::string err;
  std::vector<InstallWarning> warnings;
  EXPECT_TRUE(file_util::ValidateExtension(extension.get(), &err, &warnings));
  EXPECT_EQ(0U, warnings.size());

  EXPECT_EQ(extension->path().AppendASCII("test.html").value(),
            extension->GetResource("test.html").GetFilePath().value());
  EXPECT_EQ(extension->path().AppendASCII("test.js").value(),
            extension->GetResource("test.js").GetFilePath().value());
}


TEST(ExtensionTest, IdIsValid) {
  EXPECT_TRUE(crx_file::id_util::IdIsValid("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(crx_file::id_util::IdIsValid("pppppppppppppppppppppppppppppppp"));
  EXPECT_TRUE(crx_file::id_util::IdIsValid("abcdefghijklmnopabcdefghijklmnop"));
  EXPECT_TRUE(crx_file::id_util::IdIsValid("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"));
  EXPECT_FALSE(crx_file::id_util::IdIsValid("abcdefghijklmnopabcdefghijklmno"));
  EXPECT_FALSE(
      crx_file::id_util::IdIsValid("abcdefghijklmnopabcdefghijklmnopa"));
  EXPECT_FALSE(
      crx_file::id_util::IdIsValid("0123456789abcdef0123456789abcdef"));
  EXPECT_FALSE(
      crx_file::id_util::IdIsValid("abcdefghijklmnopabcdefghijklmnoq"));
  EXPECT_FALSE(
      crx_file::id_util::IdIsValid("abcdefghijklmnopabcdefghijklmno0"));
}

// This test ensures that the mimetype sniffing code stays in sync with the
// actual crx files that we test other parts of the system with.
TEST(ExtensionTest, MimeTypeSniffing) {
  auto get_mime_type_from_crx = [](const base::FilePath& file_path) {
    SCOPED_TRACE(file_path.AsUTF8Unsafe());

    std::string data;
    EXPECT_TRUE(base::ReadFileToString(file_path, &data));

    std::string result;
    EXPECT_TRUE(net::SniffMimeType(
        data, GURL("http://www.example.com/foo.crx"), std::string(),
        net::ForceSniffFileUrlsForHtml::kDisabled, &result));

    return result;
  };

  base::FilePath dir_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &dir_path));
  dir_path = dir_path.AppendASCII("extensions");

  // First, test an extension packed a long time ago (but in this galaxy).
  // Specifically, this package is using the crx2 format, whereas modern chrome
  // uses crx3.
  EXPECT_EQ(
      Extension::kMimeType,
      get_mime_type_from_crx(dir_path.AppendASCII("legacy_crx_package.crx")));

  // Then, an extension whose crx has a bad magic number (it should be Cr24).
  EXPECT_EQ("application/octet-stream",
            get_mime_type_from_crx(dir_path.AppendASCII("bad_magic.crx")));

  // Finally, an extension that we pack right. This. Instant.
  // This verifies that the modern extensions Chrome packs are always
  // recognized as the extension mime type.
  // Regression test for https://crbug.com/831284.
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"(
      {
        "name": "New extension",
        "version": "0.2",
        "manifest_version": 2
      })");
  EXPECT_EQ(Extension::kMimeType, get_mime_type_from_crx(test_dir.Pack()));
}

TEST(ExtensionTest, WantsFileAccess) {
  scoped_refptr<Extension> extension;
  GURL file_url("file:///etc/passwd");

  // Ignore the policy delegate for this test.
  PermissionsData::SetPolicyDelegate(nullptr);

  // <all_urls> permission
  extension = LoadManifest("permissions", "permissions_all_urls.json");
  EXPECT_TRUE(extension->wants_file_access());
  EXPECT_FALSE(
      extension->permissions_data()->CanAccessPage(file_url, -1, nullptr));
  extension = LoadManifest(
      "permissions", "permissions_all_urls.json", Extension::ALLOW_FILE_ACCESS);
  EXPECT_TRUE(extension->wants_file_access());
  EXPECT_TRUE(
      extension->permissions_data()->CanAccessPage(file_url, -1, nullptr));

  // file:///* permission
  extension = LoadManifest("permissions", "permissions_file_scheme.json");
  EXPECT_TRUE(extension->wants_file_access());
  EXPECT_FALSE(
      extension->permissions_data()->CanAccessPage(file_url, -1, nullptr));
  extension = LoadManifest("permissions",
                           "permissions_file_scheme.json",
                           Extension::ALLOW_FILE_ACCESS);
  EXPECT_TRUE(extension->wants_file_access());
  EXPECT_TRUE(
      extension->permissions_data()->CanAccessPage(file_url, -1, nullptr));

  // http://* permission
  extension = LoadManifest("permissions", "permissions_http_scheme.json");
  EXPECT_FALSE(extension->wants_file_access());
  EXPECT_FALSE(
      extension->permissions_data()->CanAccessPage(file_url, -1, nullptr));
  extension = LoadManifest("permissions",
                           "permissions_http_scheme.json",
                           Extension::ALLOW_FILE_ACCESS);
  EXPECT_FALSE(extension->wants_file_access());
  EXPECT_FALSE(
      extension->permissions_data()->CanAccessPage(file_url, -1, nullptr));

  // <all_urls> content script match
  extension = LoadManifest("permissions", "content_script_all_urls.json");
  EXPECT_TRUE(extension->wants_file_access());
  EXPECT_FALSE(extension->permissions_data()->CanRunContentScriptOnPage(
      file_url, -1, nullptr));
  extension = LoadManifest("permissions", "content_script_all_urls.json",
      Extension::ALLOW_FILE_ACCESS);
  EXPECT_TRUE(extension->wants_file_access());
  EXPECT_TRUE(extension->permissions_data()->CanRunContentScriptOnPage(
      file_url, -1, nullptr));

  // file:///* content script match
  extension = LoadManifest("permissions", "content_script_file_scheme.json");
  EXPECT_TRUE(extension->wants_file_access());
  EXPECT_FALSE(extension->permissions_data()->CanRunContentScriptOnPage(
      file_url, -1, nullptr));
  extension = LoadManifest("permissions", "content_script_file_scheme.json",
      Extension::ALLOW_FILE_ACCESS);
  EXPECT_TRUE(extension->wants_file_access());
  EXPECT_TRUE(extension->permissions_data()->CanRunContentScriptOnPage(
      file_url, -1, nullptr));

  // http://* content script match
  extension = LoadManifest("permissions", "content_script_http_scheme.json");
  EXPECT_FALSE(extension->wants_file_access());
  EXPECT_FALSE(extension->permissions_data()->CanRunContentScriptOnPage(
      file_url, -1, nullptr));
  extension = LoadManifest("permissions", "content_script_http_scheme.json",
      Extension::ALLOW_FILE_ACCESS);
  EXPECT_FALSE(extension->wants_file_access());
  EXPECT_FALSE(extension->permissions_data()->CanRunContentScriptOnPage(
      file_url, -1, nullptr));
}

TEST(ExtensionTest, ExtraFlags) {
  scoped_refptr<Extension> extension =
      LoadManifest("app", "manifest.json", Extension::FROM_WEBSTORE);
  EXPECT_TRUE(extension->from_webstore());

  extension = LoadManifest("app", "manifest.json", Extension::NO_FLAGS);
  EXPECT_FALSE(extension->from_webstore());
}

// Checks that manifest keys excluded from unrecognized key warnings are not
// registered as manifest features.
TEST(ExtensionTest, IgnoredUnrecognizedKeysAreNotManifestFeatures) {
  const extensions::FeatureProvider* manifest_features =
      extensions::FeatureProvider::GetManifestFeatures();
  ASSERT_TRUE(manifest_features);

  for (const auto& [key, value] : manifest_features->GetAllFeatures()) {
    EXPECT_FALSE(base::Contains(manifest_keys::kIgnoredUnrecognizedKeys, key));
  }
}

}  // namespace extensions
