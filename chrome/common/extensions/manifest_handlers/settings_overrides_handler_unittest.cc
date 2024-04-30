// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"

#include <memory>
#include <string_view>

#include "base/json/json_string_value_serializer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_url_handlers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kManifest[] =
    "{"
    " \"version\" : \"1.0.0.0\","
    " \"manifest_version\" : 2,"
    " \"name\" : \"Test\","
    " \"chrome_settings_overrides\" : {"
    "   \"homepage\" : \"http://www.homepage.com\","
    "   \"search_provider\" : {"
    "        \"name\" : \"first\","
    "        \"keyword\" : \"firstkey\","
    "        \"search_url\" : \"http://www.foo.com/s?q={searchTerms}\","
    "        \"favicon_url\" : \"http://www.foo.com/favicon.ico\","
    "        \"suggest_url\" : \"http://www.foo.com/s?q={searchTerms}\","
    "        \"encoding\" : \"UTF-8\","
    "        \"is_default\" : true"
    "    },"
    "   \"startup_pages\" : [\"http://www.startup.com\"]"
    "  }"
    "}";

const char kPrepopulatedManifest[] =
    "{"
    " \"version\" : \"1.0.0.0\","
    " \"manifest_version\" : 2,"
    " \"name\" : \"Test\","
    " \"chrome_settings_overrides\" : {"
    "   \"search_provider\" : {"
    "        \"search_url\" : \"http://www.foo.com/s?q={searchTerms}\","
    "        \"prepopulated_id\" : 3,"
    "        \"is_default\" : true"
    "    }"
    "  }"
    "}";

const char kBrokenManifestEmpty[] = R"(
{
  "version" : "1.0.0.0",
  "manifest_version" : 2,
  "name" : "Test",
  "chrome_settings_overrides" : {}
})";

const char kBrokenManifestHomepage[] = R"(
{
  "version" : "1.0.0.0",
  "manifest_version" : 2,
  "name" : "Test",
  "chrome_settings_overrides" : {
    "homepage" : "{invalid}"
  }
})";

const char kBrokenManifestStartupPages[] = R"(
{
  "version" : "1.0.0.0",
  "manifest_version" : 2,
  "name" : "Test",
  "chrome_settings_overrides" : {
    "startup_pages" : ["{invalid}"]
  }
})";

const char kManifestBrokenHomepageButCorrectStartupPages[] = R"(
{
  "version" : "1.0.0.0",
  "manifest_version" : 2,
  "name" : "Test",
  "chrome_settings_overrides" : {
    "homepage" : "{invalid}",
    "startup_pages" : ["http://www.startup.com"]
  }
})";

const char kManifestBrokenStartupPagesButCorrectHomepage[] = R"(
{
  "version" : "1.0.0.0",
  "manifest_version" : 2,
  "name" : "Test",
  "chrome_settings_overrides" : {
    "homepage": "http://www.homepage.com",
    "startup_pages" : ["{invalid}"]
  }
})";

using extensions::Extension;
using extensions::Manifest;
using extensions::SettingsOverrides;
using extensions::api::manifest_types::ChromeSettingsOverrides;
namespace manifest_keys = extensions::manifest_keys;

scoped_refptr<Extension> CreateExtension(const base::Value::Dict& manifest,
                                         std::string* error) {
  scoped_refptr<Extension> extension =
      Extension::Create(base::FilePath(FILE_PATH_LITERAL("//nonexistent")),
                        extensions::mojom::ManifestLocation::kInvalidLocation,
                        manifest, Extension::NO_FLAGS, error);
  return extension;
}

scoped_refptr<Extension> CreateExtension(std::string_view manifest,
                                         std::string* error) {
  JSONStringValueDeserializer json(manifest);
  std::unique_ptr<base::Value> root(json.Deserialize(nullptr, error));
  if (!root) {
    ADD_FAILURE() << "Could not deserialize manifest";
    return nullptr;
  }
  if (!root->is_dict()) {
    ADD_FAILURE() << "Manifest isn't a Dictionary";
    return nullptr;
  }
  return CreateExtension(root->GetDict(), error);
}

scoped_refptr<Extension> CreateExtensionWithSearchProvider(
    base::Value::Dict search_provider,
    std::string* error) {
  auto manifest = base::Value::Dict()
                      .Set("name", "name")
                      .Set("manifest_version", 2)
                      .Set("version", "0.1")
                      .Set("description", "desc")
                      .Set("chrome_settings_overrides",
                           base::Value::Dict().Set("search_provider",
                                                   std::move(search_provider)));
  return CreateExtension(std::move(manifest), error);
}

TEST(OverrideSettingsTest, ParseManifest) {
  std::string error;
  scoped_refptr<Extension> extension = CreateExtension(kManifest, &error);
  ASSERT_TRUE(extension.get());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  ASSERT_TRUE(
      extension->manifest()->FindPath(manifest_keys::kSettingsOverride));

  SettingsOverrides* settings_override = static_cast<SettingsOverrides*>(
      extension->GetManifestData(manifest_keys::kSettingsOverride));
  ASSERT_TRUE(settings_override);
  ASSERT_TRUE(settings_override->search_engine);
  EXPECT_TRUE(settings_override->search_engine->is_default);
  const auto& search_engine = settings_override->search_engine;
  EXPECT_EQ("first", *search_engine->name);
  EXPECT_EQ("firstkey", *search_engine->keyword);
  EXPECT_EQ("http://www.foo.com/s?q={searchTerms}", search_engine->search_url);
  EXPECT_EQ("http://www.foo.com/favicon.ico", *search_engine->favicon_url);
  EXPECT_EQ("http://www.foo.com/s?q={searchTerms}",
            *search_engine->suggest_url);
  EXPECT_EQ("UTF-8", *search_engine->encoding);

  EXPECT_EQ(std::vector<GURL>(1, GURL("http://www.startup.com")),
            settings_override->startup_pages);

  ASSERT_TRUE(settings_override->homepage);
  EXPECT_EQ(GURL("http://www.homepage.com"), *settings_override->homepage);
#else
  EXPECT_FALSE(
      extension->manifest()->FindPath(manifest_keys::kSettingsOverride));
#endif
}

TEST(OverrideSettingsTest, ParsePrepopulatedId) {
  std::string error;
  scoped_refptr<Extension> extension =
      CreateExtension(kPrepopulatedManifest, &error);
  ASSERT_TRUE(extension.get());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  ASSERT_TRUE(
      extension->manifest()->FindPath(manifest_keys::kSettingsOverride));

  SettingsOverrides* settings_override = static_cast<SettingsOverrides*>(
      extension->GetManifestData(manifest_keys::kSettingsOverride));
  ASSERT_TRUE(settings_override);
  ASSERT_TRUE(settings_override->search_engine);
  EXPECT_TRUE(settings_override->search_engine->is_default);
  const auto& search_engine = settings_override->search_engine;
  ASSERT_TRUE(search_engine->prepopulated_id);
  EXPECT_EQ("http://www.foo.com/s?q={searchTerms}", search_engine->search_url);
  EXPECT_EQ(3, *search_engine->prepopulated_id);
#else
  EXPECT_FALSE(
      extension->manifest()->FindPath(manifest_keys::kSettingsOverride));
#endif
}

TEST(OverrideSettingsTest, ParseManifestBrokenHomepageButCorrectStartupPages) {
  std::string error;
  scoped_refptr<Extension> extension =
      CreateExtension(kManifestBrokenHomepageButCorrectStartupPages, &error);
  ASSERT_TRUE(extension.get());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  ASSERT_TRUE(
      extension->manifest()->FindPath(manifest_keys::kSettingsOverride));

  SettingsOverrides* settings_override = static_cast<SettingsOverrides*>(
      extension->GetManifestData(manifest_keys::kSettingsOverride));
  ASSERT_TRUE(settings_override);
  EXPECT_EQ(std::vector<GURL>(1, GURL("http://www.startup.com")),
            settings_override->startup_pages);
#else
  EXPECT_FALSE(
      extension->manifest()->FindPath(manifest_keys::kSettingsOverride));
#endif
}

TEST(OverrideSettingsTest, ParseManifestBrokenStartupPagesButCorrectHomepage) {
  std::string error;
  scoped_refptr<Extension> extension =
      CreateExtension(kManifestBrokenStartupPagesButCorrectHomepage, &error);
  ASSERT_TRUE(extension.get());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  ASSERT_TRUE(
      extension->manifest()->FindPath(manifest_keys::kSettingsOverride));
  SettingsOverrides* settings_override = static_cast<SettingsOverrides*>(
      extension->GetManifestData(manifest_keys::kSettingsOverride));
  ASSERT_TRUE(settings_override);
  EXPECT_TRUE(settings_override->startup_pages.empty());
  EXPECT_EQ(GURL("http://www.homepage.com"), *settings_override->homepage);
#else
  EXPECT_FALSE(
      extension->manifest()->FindPath(manifest_keys::kSettingsOverride));
#endif
}

TEST(OverrideSettingsTest, ParseBrokenManifestEmptySettingsOverride) {
  std::string error;
  scoped_refptr<Extension> extension =
      CreateExtension(kBrokenManifestEmpty, &error);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  EXPECT_FALSE(extension.get());
  EXPECT_EQ(
      extensions::ErrorUtils::FormatErrorMessage(
          extensions::manifest_errors::kInvalidEmptyDictionary,
          extensions::manifest_keys::kSettingsOverride),
      error);
#else
  ASSERT_TRUE(extension.get());
  EXPECT_FALSE(
      extension->manifest()->FindPath(manifest_keys::kSettingsOverride));
#endif
}

TEST(OverrideSettingsTest, ParseBrokenManifestHomepage) {
  std::string error;
  scoped_refptr<Extension> extension =
      CreateExtension(kBrokenManifestHomepage, &error);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  EXPECT_FALSE(extension.get());
  EXPECT_EQ(extensions::ErrorUtils::FormatErrorMessage(
                extensions::manifest_errors::kInvalidHomepageOverrideURL,
                "{invalid}"),
            error);
#else
  ASSERT_TRUE(extension.get());
  EXPECT_FALSE(
      extension->manifest()->FindPath(manifest_keys::kSettingsOverride));
#endif
}

TEST(OverrideSettingsTest, ParseBrokenManifestStartupPages) {
  std::string error;
  scoped_refptr<Extension> extension =
      CreateExtension(kBrokenManifestStartupPages, &error);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  EXPECT_FALSE(extension.get());
  EXPECT_EQ(
      extensions::ErrorUtils::FormatErrorMessage(
          extensions::manifest_errors::kInvalidStartupOverrideURL, "{invalid}"),
      error);
#else
  ASSERT_TRUE(extension.get());
  EXPECT_FALSE(
      extension->manifest()->FindPath(manifest_keys::kSettingsOverride));
#endif
}

TEST(OverrideSettingsTest, SearchProviderMissingKeys) {
  struct KeyValue {
    const char* key;
    const char* value;
  } kMandatorySearchProviderKeyValues[] = {
      {"name", "first"},
      {"keyword", "firstkey"},
      {"encoding", "UTF-8"},
      {"favicon_url", "http://www.foo.com/favicon.ico"},
  };

  auto search_provider =
      base::Value::Dict()
          .Set("search_url", "http://www.foo.com/s?q={searchTerms}")
          .Set("is_default", true);
  for (const KeyValue& kv : kMandatorySearchProviderKeyValues)
    search_provider.Set(kv.key, kv.value);
  base::Value::Dict search_provider_with_all_keys_dict =
      std::move(search_provider);

  // Missing all keys from |kMandatorySearchProviderValues|.
  for (const KeyValue& kv : kMandatorySearchProviderKeyValues) {
    SCOPED_TRACE(testing::Message()
                 << "key = " << kv.key << " value = " << kv.value);
    // Build a search provider entry with |kv.key| missing:
    base::Value::Dict provider_with_missing_key =
        search_provider_with_all_keys_dict.Clone();
    ASSERT_TRUE(provider_with_missing_key.Remove(kv.key));

    std::string error;
    scoped_refptr<Extension> extension = CreateExtensionWithSearchProvider(
        std::move(provider_with_missing_key), &error);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
    EXPECT_FALSE(extension.get());
    EXPECT_EQ(extensions::ErrorUtils::FormatErrorMessage(
                  extensions::manifest_errors::kInvalidSearchEngineMissingKeys,
                  kv.key),
              error);
#else
    ASSERT_TRUE(extension.get());
    EXPECT_FALSE(
        extension->manifest()->FindPath(manifest_keys::kSettingsOverride));
#endif
  }
}

}  // namespace
