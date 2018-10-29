// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_api.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_features_unittest.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

const char* const kTestFeatures[] = {
    "test1", "test2", "test3",   "test4",   "test5",
    "test6", "test7", "parent1", "parent2", "parent3",
};

const char* const kAliasTestApis[] = {"alias_api_source"};

const char* const kSessionTypeTestFeatures[] = {
    "test6", "kiosk_only", "non_kiosk", "multiple_session_types",
    "autolaunched_kiosk"};

struct FeatureSessionTypesTestData {
  std::string api_name;
  bool expect_available;
  FeatureSessionType current_session_type;
};

class TestExtensionAPI : public ExtensionAPI {
 public:
  TestExtensionAPI() {}
  ~TestExtensionAPI() override {}

  void add_fake_schema(const std::string& name) { fake_schemas_.insert(name); }

 private:
  bool IsKnownAPI(const std::string& name, ExtensionsClient* client) override {
    return fake_schemas_.count(name) != 0;
  }

  std::set<std::string> fake_schemas_;
  DISALLOW_COPY_AND_ASSIGN(TestExtensionAPI);
};

}  // namespace

TEST(ExtensionAPITest, Creation) {
  ExtensionAPI* shared_instance = ExtensionAPI::GetSharedInstance();
  EXPECT_EQ(shared_instance, ExtensionAPI::GetSharedInstance());

  std::unique_ptr<ExtensionAPI> new_instance(
      ExtensionAPI::CreateWithDefaultConfiguration());
  EXPECT_NE(new_instance.get(),
            std::unique_ptr<ExtensionAPI>(
                ExtensionAPI::CreateWithDefaultConfiguration())
                .get());

  ExtensionAPI empty_instance;

  struct {
    ExtensionAPI* api;
    bool expect_populated;
  } test_data[] = {
    { shared_instance, true },
    { new_instance.get(), true },
    { &empty_instance, false }
  };

  for (size_t i = 0; i < arraysize(test_data); ++i) {
    EXPECT_EQ(test_data[i].expect_populated,
              test_data[i].api->GetSchema("bookmarks.create") != nullptr);
  }
}

TEST(ExtensionAPITest, SplitDependencyName) {
  struct {
    std::string input;
    std::string expected_feature_type;
    std::string expected_feature_name;
  } test_data[] = {{"", "api", ""},  // assumes "api" when no type is present
                   {"foo", "api", "foo"},
                   {"foo:", "foo", ""},
                   {":foo", "", "foo"},
                   {"foo:bar", "foo", "bar"},
                   {"foo:bar.baz", "foo", "bar.baz"}};

  for (size_t i = 0; i < arraysize(test_data); ++i) {
    std::string feature_type;
    std::string feature_name;
    ExtensionAPI::SplitDependencyName(
        test_data[i].input, &feature_type, &feature_name);
    EXPECT_EQ(test_data[i].expected_feature_type, feature_type) << i;
    EXPECT_EQ(test_data[i].expected_feature_name, feature_name) << i;
  }
}

TEST(ExtensionAPITest, APIFeatures) {
  struct {
    std::string api_full_name;
    bool expect_is_available;
    Feature::Context context;
    GURL url;
  } test_data[] = {
    { "test1", false, Feature::WEB_PAGE_CONTEXT, GURL() },
    { "test1", true, Feature::BLESSED_EXTENSION_CONTEXT, GURL() },
    { "test1", true, Feature::UNBLESSED_EXTENSION_CONTEXT, GURL() },
    { "test1", true, Feature::CONTENT_SCRIPT_CONTEXT, GURL() },
    { "test2", true, Feature::WEB_PAGE_CONTEXT, GURL("http://google.com") },
    { "test2", false, Feature::BLESSED_EXTENSION_CONTEXT,
        GURL("http://google.com") },
    { "test2.foo", false, Feature::WEB_PAGE_CONTEXT,
        GURL("http://google.com") },
    { "test2.foo", true, Feature::CONTENT_SCRIPT_CONTEXT, GURL() },
    { "test3", false, Feature::WEB_PAGE_CONTEXT, GURL("http://google.com") },
    { "test3.foo", true, Feature::WEB_PAGE_CONTEXT, GURL("http://google.com") },
    { "test3.foo", true, Feature::BLESSED_EXTENSION_CONTEXT,
        GURL("http://bad.com") },
    { "test4", true, Feature::BLESSED_EXTENSION_CONTEXT,
        GURL("http://bad.com") },
    { "test4.foo", false, Feature::BLESSED_EXTENSION_CONTEXT,
        GURL("http://bad.com") },
    { "test4.foo", false, Feature::UNBLESSED_EXTENSION_CONTEXT,
        GURL("http://bad.com") },
    { "test4.foo.foo", true, Feature::CONTENT_SCRIPT_CONTEXT, GURL() },
    { "test5", true, Feature::WEB_PAGE_CONTEXT, GURL("http://foo.com") },
    { "test5", false, Feature::WEB_PAGE_CONTEXT, GURL("http://bar.com") },
    { "test5.blah", true, Feature::WEB_PAGE_CONTEXT, GURL("http://foo.com") },
    { "test5.blah", false, Feature::WEB_PAGE_CONTEXT, GURL("http://bar.com") },
    { "test6", false, Feature::BLESSED_EXTENSION_CONTEXT, GURL() },
    { "test6.foo", true, Feature::BLESSED_EXTENSION_CONTEXT, GURL() },
    { "test7", true, Feature::WEB_PAGE_CONTEXT, GURL("http://foo.com") },
    { "test7.foo", false, Feature::WEB_PAGE_CONTEXT, GURL("http://bar.com") },
    { "test7.foo", true, Feature::WEB_PAGE_CONTEXT, GURL("http://foo.com") },
    { "test7.bar", false, Feature::WEB_PAGE_CONTEXT, GURL("http://bar.com") },
    { "test7.bar", false, Feature::WEB_PAGE_CONTEXT, GURL("http://foo.com") },

    // Test parent/child.
    { "parent1", true, Feature::CONTENT_SCRIPT_CONTEXT, GURL() },
    { "parent1", false, Feature::WEB_PAGE_CONTEXT, GURL("http://foo.com") },
    { "parent1.child1", false, Feature::CONTENT_SCRIPT_CONTEXT, GURL() },
    { "parent1.child1", true, Feature::WEB_PAGE_CONTEXT,
        GURL("http://foo.com") },
    { "parent1.child2", true, Feature::CONTENT_SCRIPT_CONTEXT, GURL() },
    { "parent1.child2", false, Feature::WEB_PAGE_CONTEXT,
        GURL("http://foo.com") },
    { "parent2", true, Feature::CONTENT_SCRIPT_CONTEXT, GURL() },
    { "parent2", true, Feature::BLESSED_EXTENSION_CONTEXT, GURL() },
    { "parent2", true, Feature::UNBLESSED_EXTENSION_CONTEXT, GURL() },
    { "parent2.child3", false, Feature::CONTENT_SCRIPT_CONTEXT, GURL() },
    { "parent2.child3", true, Feature::BLESSED_EXTENSION_CONTEXT, GURL() },
    { "parent2.child3", false, Feature::UNBLESSED_EXTENSION_CONTEXT, GURL() },
    { "parent2.child3.child.child", true, Feature::CONTENT_SCRIPT_CONTEXT,
        GURL() },
    { "parent2.child3.child.child", false, Feature::BLESSED_EXTENSION_CONTEXT,
        GURL() },
    { "parent2.child3.child.child", true, Feature::UNBLESSED_EXTENSION_CONTEXT,
        GURL() },
    { "parent3", true, Feature::CONTENT_SCRIPT_CONTEXT, GURL() },
    { "parent3", false, Feature::BLESSED_EXTENSION_CONTEXT, GURL() },
    { "parent3", false, Feature::UNBLESSED_EXTENSION_CONTEXT, GURL() },
    { "parent3.noparent", true, Feature::CONTENT_SCRIPT_CONTEXT, GURL() },
    { "parent3.noparent", true, Feature::BLESSED_EXTENSION_CONTEXT, GURL() },
    { "parent3.noparent", true, Feature::UNBLESSED_EXTENSION_CONTEXT, GURL() },
    { "parent3.noparent.child", true, Feature::CONTENT_SCRIPT_CONTEXT, GURL() },
    { "parent3.noparent.child", true, Feature::BLESSED_EXTENSION_CONTEXT,
        GURL() },
    { "parent3.noparent.child", true, Feature::UNBLESSED_EXTENSION_CONTEXT,
        GURL() }
  };

  FeatureProvider api_feature_provider;
  AddUnittestAPIFeatures(&api_feature_provider);

  for (size_t i = 0; i < arraysize(test_data); ++i) {
    TestExtensionAPI api;
    api.RegisterDependencyProvider("api", &api_feature_provider);
    for (auto* key : kTestFeatures)
      api.add_fake_schema(key);
    ExtensionAPI::OverrideSharedInstanceForTest scope(&api);

    bool expected = test_data[i].expect_is_available;
    Feature::Availability availability = api.IsAvailable(
        test_data[i].api_full_name, nullptr, test_data[i].context,
        test_data[i].url, CheckAliasStatus::NOT_ALLOWED);
    EXPECT_EQ(expected, availability.is_available())
        << base::StringPrintf("Test %d: Feature '%s' was %s: %s",
                              static_cast<int>(i),
                              test_data[i].api_full_name.c_str(),
                              expected ? "not available" : "available",
                              availability.message().c_str());
  }
}

TEST(ExtensionAPITest, APIFeaturesAlias) {
  FeatureProvider api_feature_provider;
  AddUnittestAPIFeatures(&api_feature_provider);

  TestExtensionAPI api;
  api.RegisterDependencyProvider("api", &api_feature_provider);
  for (auto* key : kAliasTestApis)
    api.add_fake_schema(key);
  ExtensionAPI::OverrideSharedInstanceForTest scope(&api);

  ASSERT_FALSE(api.IsAvailable("alias_api_source", nullptr,
                               Feature::UNBLESSED_EXTENSION_CONTEXT, GURL(),
                               CheckAliasStatus::NOT_ALLOWED)
                   .is_available());
  ASSERT_TRUE(api.IsAvailable("alias_api_source", nullptr,
                              Feature::UNBLESSED_EXTENSION_CONTEXT, GURL(),
                              CheckAliasStatus::ALLOWED)
                  .is_available());
  ASSERT_TRUE(api.IsAvailable("alias_api_source.bar", nullptr,
                              Feature::UNBLESSED_EXTENSION_CONTEXT, GURL(),
                              CheckAliasStatus::ALLOWED)
                  .is_available());
  ASSERT_FALSE(api.IsAvailable("alias_api_source.foo", nullptr,
                               Feature::UNBLESSED_EXTENSION_CONTEXT, GURL(),
                               CheckAliasStatus::ALLOWED)
                   .is_available());

  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "extension")
                           .Set("version", "1")
                           .Set("manifest_version", 2)
                           .Build())
          .Build();
  const Feature* test_feature =
      api_feature_provider.GetFeature("alias_api_source");
  ASSERT_TRUE(test_feature);
  ASSERT_FALSE(api.IsAnyFeatureAvailableToContext(
      *test_feature, extension.get(), Feature::UNBLESSED_EXTENSION_CONTEXT,
      GURL(), CheckAliasStatus::NOT_ALLOWED));
  EXPECT_TRUE(api.IsAnyFeatureAvailableToContext(
      *test_feature, extension.get(), Feature::UNBLESSED_EXTENSION_CONTEXT,
      GURL(), CheckAliasStatus::ALLOWED));
}

TEST(ExtensionAPITest, IsAnyFeatureAvailableToContext) {
  scoped_refptr<const Extension> app =
      ExtensionBuilder()
          .SetManifest(
              DictionaryBuilder()
                  .Set("name", "app")
                  .Set("app",
                       DictionaryBuilder()
                           .Set("background",
                                DictionaryBuilder()
                                    .Set("scripts", ListBuilder()
                                                        .Append("background.js")
                                                        .Build())
                                    .Build())
                           .Build())
                  .Set("version", "1")
                  .Set("manifest_version", 2)
                  .Build())
          .Build();
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "extension")
                           .Set("version", "1")
                           .Set("manifest_version", 2)
                           .Build())
          .Build();

  struct {
    std::string api_full_name;
    bool expect_is_available;
    Feature::Context context;
    const Extension* extension;
    GURL url;
  } test_data[] = {
      {"test1", false, Feature::WEB_PAGE_CONTEXT, nullptr, GURL()},
      {"test1", true, Feature::UNBLESSED_EXTENSION_CONTEXT, nullptr, GURL()},
      {"test1", false, Feature::UNBLESSED_EXTENSION_CONTEXT, app.get(), GURL()},
      {"test1", true, Feature::UNBLESSED_EXTENSION_CONTEXT, extension.get(),
       GURL()},
      {"test2", true, Feature::CONTENT_SCRIPT_CONTEXT, nullptr, GURL()},
      {"test2", true, Feature::WEB_PAGE_CONTEXT, nullptr,
       GURL("http://google.com")},
      {"test2.foo", false, Feature::WEB_PAGE_CONTEXT, nullptr,
       GURL("http://google.com")},
      {"test3", true, Feature::CONTENT_SCRIPT_CONTEXT, nullptr, GURL()},
      {"test3", true, Feature::WEB_PAGE_CONTEXT, nullptr,
       GURL("http://foo.com")},
      {"test4.foo", true, Feature::CONTENT_SCRIPT_CONTEXT, nullptr, GURL()},
      {"test7", false, Feature::WEB_PAGE_CONTEXT, nullptr,
       GURL("http://google.com")},
      {"test7", true, Feature::WEB_PAGE_CONTEXT, nullptr,
       GURL("http://foo.com")},
      {"test7", false, Feature::WEB_PAGE_CONTEXT, nullptr,
       GURL("http://bar.com")}};

  FeatureProvider api_feature_provider;
  AddUnittestAPIFeatures(&api_feature_provider);

  for (size_t i = 0; i < arraysize(test_data); ++i) {
    TestExtensionAPI api;
    api.RegisterDependencyProvider("api", &api_feature_provider);
    for (auto* key : kTestFeatures)
      api.add_fake_schema(key);
    ExtensionAPI::OverrideSharedInstanceForTest scope(&api);

    const Feature* test_feature =
        api_feature_provider.GetFeature(test_data[i].api_full_name);
    ASSERT_TRUE(test_feature);
    EXPECT_EQ(test_data[i].expect_is_available,
              api.IsAnyFeatureAvailableToContext(
                  *test_feature, test_data[i].extension, test_data[i].context,
                  test_data[i].url, CheckAliasStatus::NOT_ALLOWED))
        << i;
  }
}

TEST(ExtensionAPITest, SessionTypeFeature) {
  scoped_refptr<const Extension> app =
      ExtensionBuilder()
          .SetManifest(
              DictionaryBuilder()
                  .Set("name", "app")
                  .Set("app",
                       DictionaryBuilder()
                           .Set("background",
                                DictionaryBuilder()
                                    .Set("scripts", ListBuilder()
                                                        .Append("background.js")
                                                        .Build())
                                    .Build())
                           .Build())
                  .Set("version", "1")
                  .Set("manifest_version", 2)
                  .Build())
          .Build();

  const std::vector<FeatureSessionTypesTestData> kTestData(
      {{"kiosk_only", true, FeatureSessionType::KIOSK},
       {"kiosk_only", true, FeatureSessionType::AUTOLAUNCHED_KIOSK},
       {"kiosk_only", false, FeatureSessionType::REGULAR},
       {"kiosk_only", false, FeatureSessionType::UNKNOWN},
       {"non_kiosk", false, FeatureSessionType::KIOSK},
       {"non_kiosk", true, FeatureSessionType::REGULAR},
       {"non_kiosk", false, FeatureSessionType::UNKNOWN},
       {"autolaunched_kiosk", true, FeatureSessionType::AUTOLAUNCHED_KIOSK},
       {"autolaunched_kiosk", false, FeatureSessionType::KIOSK},
       {"autolaunched_kiosk", false, FeatureSessionType::REGULAR},
       {"multiple_session_types", true, FeatureSessionType::KIOSK},
       {"multiple_session_types", true, FeatureSessionType::REGULAR},
       {"multiple_session_types", false, FeatureSessionType::UNKNOWN},
       // test6.foo is available to apps and has no session type restrictions.
       {"test6.foo", true, FeatureSessionType::KIOSK},
       {"test6.foo", true, FeatureSessionType::AUTOLAUNCHED_KIOSK},
       {"test6.foo", true, FeatureSessionType::REGULAR},
       {"test6.foo", true, FeatureSessionType::UNKNOWN}});

  FeatureProvider api_feature_provider;
  AddUnittestAPIFeatures(&api_feature_provider);

  for (const auto& test : kTestData) {
    TestExtensionAPI api;
    api.RegisterDependencyProvider("api", &api_feature_provider);
    for (auto* key : kSessionTypeTestFeatures)
      api.add_fake_schema(key);
    ExtensionAPI::OverrideSharedInstanceForTest scope(&api);

    std::unique_ptr<base::AutoReset<FeatureSessionType>> current_session(
        ScopedCurrentFeatureSessionType(test.current_session_type));
    EXPECT_EQ(test.expect_available,
              api.IsAvailable(test.api_name, app.get(),
                              Feature::BLESSED_EXTENSION_CONTEXT, GURL(),
                              CheckAliasStatus::NOT_ALLOWED)
                  .is_available())
        << "Test case (" << test.api_name << ", "
        << static_cast<int>(test.current_session_type) << ").";
  }
}

TEST(ExtensionAPITest, LazyGetSchema) {
  std::unique_ptr<ExtensionAPI> apis(
      ExtensionAPI::CreateWithDefaultConfiguration());

  EXPECT_EQ(nullptr, apis->GetSchema(std::string()));
  EXPECT_EQ(nullptr, apis->GetSchema(std::string()));
  EXPECT_EQ(nullptr, apis->GetSchema("experimental"));
  EXPECT_EQ(nullptr, apis->GetSchema("experimental"));
  EXPECT_EQ(nullptr, apis->GetSchema("foo"));
  EXPECT_EQ(nullptr, apis->GetSchema("foo"));

  EXPECT_TRUE(apis->GetSchema("dns"));
  EXPECT_TRUE(apis->GetSchema("dns"));
  EXPECT_TRUE(apis->GetSchema("extension"));
  EXPECT_TRUE(apis->GetSchema("extension"));
  EXPECT_TRUE(apis->GetSchema("omnibox"));
  EXPECT_TRUE(apis->GetSchema("omnibox"));
  EXPECT_TRUE(apis->GetSchema("storage"));
  EXPECT_TRUE(apis->GetSchema("storage"));
}

scoped_refptr<Extension> CreateExtensionWithPermissions(
    const std::set<std::string>& permissions) {
  base::DictionaryValue manifest;
  manifest.SetString("name", "extension");
  manifest.SetString("version", "1.0");
  manifest.SetInteger("manifest_version", 2);
  {
    std::unique_ptr<base::ListValue> permissions_list(new base::ListValue());
    for (auto i = permissions.begin(); i != permissions.end(); ++i) {
      permissions_list->AppendString(*i);
    }
    manifest.Set("permissions", std::move(permissions_list));
  }

  std::string error;
  scoped_refptr<Extension> extension(Extension::Create(
      base::FilePath(), Manifest::UNPACKED,
      manifest, Extension::NO_FLAGS, &error));
  CHECK(extension.get());
  CHECK(error.empty());

  return extension;
}

scoped_refptr<Extension> CreateExtensionWithPermission(
    const std::string& permission) {
  std::set<std::string> permissions;
  permissions.insert(permission);
  return CreateExtensionWithPermissions(permissions);
}

TEST(ExtensionAPITest, ExtensionWithUnprivilegedAPIs) {
  scoped_refptr<Extension> extension;
  {
    std::set<std::string> permissions;
    permissions.insert("storage");
    permissions.insert("history");
    extension = CreateExtensionWithPermissions(permissions);
  }

  std::unique_ptr<ExtensionAPI> extension_api(
      ExtensionAPI::CreateWithDefaultConfiguration());

  const FeatureProvider& api_features = *FeatureProvider::GetAPIFeatures();

  // "storage" is completely unprivileged.
  EXPECT_TRUE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("storage"), nullptr,
      Feature::BLESSED_EXTENSION_CONTEXT, GURL(),
      CheckAliasStatus::NOT_ALLOWED));
  EXPECT_TRUE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("storage"), nullptr,
      Feature::UNBLESSED_EXTENSION_CONTEXT, GURL(),
      CheckAliasStatus::NOT_ALLOWED));
  EXPECT_TRUE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("storage"), nullptr,
      Feature::CONTENT_SCRIPT_CONTEXT, GURL(), CheckAliasStatus::NOT_ALLOWED));

  // "extension" is partially unprivileged.
  EXPECT_TRUE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("extension"), nullptr,
      Feature::BLESSED_EXTENSION_CONTEXT, GURL(),
      CheckAliasStatus::NOT_ALLOWED));
  EXPECT_TRUE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("extension"), nullptr,
      Feature::UNBLESSED_EXTENSION_CONTEXT, GURL(),
      CheckAliasStatus::NOT_ALLOWED));
  EXPECT_TRUE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("extension"), nullptr,
      Feature::CONTENT_SCRIPT_CONTEXT, GURL(), CheckAliasStatus::NOT_ALLOWED));
  EXPECT_TRUE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("extension.getURL"), nullptr,
      Feature::CONTENT_SCRIPT_CONTEXT, GURL(), CheckAliasStatus::NOT_ALLOWED));

  // "history" is entirely privileged.
  EXPECT_TRUE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("history"), nullptr,
      Feature::BLESSED_EXTENSION_CONTEXT, GURL(),
      CheckAliasStatus::NOT_ALLOWED));
  EXPECT_FALSE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("history"), nullptr,
      Feature::UNBLESSED_EXTENSION_CONTEXT, GURL(),
      CheckAliasStatus::NOT_ALLOWED));
  EXPECT_FALSE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("history"), nullptr,
      Feature::CONTENT_SCRIPT_CONTEXT, GURL(), CheckAliasStatus::NOT_ALLOWED));
}

scoped_refptr<Extension> CreateHostedApp() {
  base::DictionaryValue values;
  values.SetString(manifest_keys::kName, "test");
  values.SetString(manifest_keys::kVersion, "0.1");
  values.Set(manifest_keys::kWebURLs, std::make_unique<base::ListValue>());
  values.SetString(manifest_keys::kLaunchWebURL,
                   "http://www.example.com");
  std::string error;
  scoped_refptr<Extension> extension(Extension::Create(
      base::FilePath(), Manifest::INTERNAL, values, Extension::NO_FLAGS,
      &error));
  CHECK(extension.get());
  return extension;
}

scoped_refptr<Extension> CreatePackagedAppWithPermissions(
    const std::set<std::string>& permissions) {
  base::DictionaryValue values;
  values.SetString(manifest_keys::kName, "test");
  values.SetString(manifest_keys::kVersion, "0.1");
  values.SetString(manifest_keys::kPlatformAppBackground,
      "http://www.example.com");

  auto app = std::make_unique<base::DictionaryValue>();
  auto background = std::make_unique<base::DictionaryValue>();
  auto scripts = std::make_unique<base::ListValue>();
  scripts->AppendString("test.js");
  background->Set("scripts", std::move(scripts));
  app->Set("background", std::move(background));
  values.Set(manifest_keys::kApp, std::move(app));
  {
    auto permissions_list = std::make_unique<base::ListValue>();
    for (auto i = permissions.begin(); i != permissions.end(); ++i) {
      permissions_list->AppendString(*i);
    }
    values.Set("permissions", std::move(permissions_list));
  }

  std::string error;
  scoped_refptr<Extension> extension(Extension::Create(
      base::FilePath(), Manifest::INTERNAL, values, Extension::NO_FLAGS,
      &error));
  CHECK(extension.get()) << error;
  return extension;
}

TEST(ExtensionAPITest, HostedAppPermissions) {
  scoped_refptr<Extension> extension = CreateHostedApp();

  std::unique_ptr<ExtensionAPI> extension_api(
      ExtensionAPI::CreateWithDefaultConfiguration());

  // "runtime" and "tabs" should not be available in hosted apps.
  EXPECT_FALSE(extension_api
                   ->IsAvailable("runtime", extension.get(),
                                 Feature::BLESSED_EXTENSION_CONTEXT, GURL(),
                                 CheckAliasStatus::NOT_ALLOWED)
                   .is_available());
  EXPECT_FALSE(extension_api
                   ->IsAvailable("runtime.id", extension.get(),
                                 Feature::BLESSED_EXTENSION_CONTEXT, GURL(),
                                 CheckAliasStatus::NOT_ALLOWED)
                   .is_available());
  EXPECT_FALSE(extension_api
                   ->IsAvailable("runtime.sendMessage", extension.get(),
                                 Feature::BLESSED_EXTENSION_CONTEXT, GURL(),
                                 CheckAliasStatus::NOT_ALLOWED)
                   .is_available());
  EXPECT_FALSE(extension_api
                   ->IsAvailable("runtime.sendNativeMessage", extension.get(),
                                 Feature::BLESSED_EXTENSION_CONTEXT, GURL(),
                                 CheckAliasStatus::NOT_ALLOWED)
                   .is_available());
  EXPECT_FALSE(extension_api
                   ->IsAvailable("tabs.create", extension.get(),
                                 Feature::BLESSED_EXTENSION_CONTEXT, GURL(),
                                 CheckAliasStatus::NOT_ALLOWED)
                   .is_available());
}

TEST(ExtensionAPITest, AppAndFriendsAvailability) {
  std::unique_ptr<ExtensionAPI> extension_api(
      ExtensionAPI::CreateWithDefaultConfiguration());

  // Make sure chrome.app.runtime and chrome.app.window are available to apps,
  // and chrome.app is not.
  {
    std::set<std::string> permissions;
    permissions.insert("app.runtime");
    permissions.insert("app.window");
    scoped_refptr<Extension> extension =
        CreatePackagedAppWithPermissions(permissions);
    EXPECT_FALSE(extension_api
                     ->IsAvailable("app", extension.get(),
                                   Feature::BLESSED_EXTENSION_CONTEXT,
                                   GURL("http://foo.com"),
                                   CheckAliasStatus::NOT_ALLOWED)
                     .is_available());
    EXPECT_TRUE(extension_api
                    ->IsAvailable("app.runtime", extension.get(),
                                  Feature::BLESSED_EXTENSION_CONTEXT,
                                  GURL("http://foo.com"),
                                  CheckAliasStatus::NOT_ALLOWED)
                    .is_available());
    EXPECT_TRUE(extension_api
                    ->IsAvailable("app.window", extension.get(),
                                  Feature::BLESSED_EXTENSION_CONTEXT,
                                  GURL("http://foo.com"),
                                  CheckAliasStatus::NOT_ALLOWED)
                    .is_available());
  }
  // Make sure chrome.app.runtime and chrome.app.window are not available to
  // extensions, and chrome.app is.
  {
    std::set<std::string> permissions;
    scoped_refptr<Extension> extension =
        CreateExtensionWithPermissions(permissions);
    EXPECT_TRUE(extension_api
                    ->IsAvailable("app", extension.get(),
                                  Feature::BLESSED_EXTENSION_CONTEXT,
                                  GURL("http://foo.com"),
                                  CheckAliasStatus::NOT_ALLOWED)
                    .is_available());
    EXPECT_FALSE(extension_api
                     ->IsAvailable("app.runtime", extension.get(),
                                   Feature::BLESSED_EXTENSION_CONTEXT,
                                   GURL("http://foo.com"),
                                   CheckAliasStatus::NOT_ALLOWED)
                     .is_available());
    EXPECT_FALSE(extension_api
                     ->IsAvailable("app.window", extension.get(),
                                   Feature::BLESSED_EXTENSION_CONTEXT,
                                   GURL("http://foo.com"),
                                   CheckAliasStatus::NOT_ALLOWED)
                     .is_available());
  }
}

TEST(ExtensionAPITest, ExtensionWithDependencies) {
  // Extension with the "ttsEngine" permission but not the "tts" permission; it
  // should not automatically get "tts" permission.
  {
    scoped_refptr<Extension> extension =
        CreateExtensionWithPermission("ttsEngine");
    std::unique_ptr<ExtensionAPI> api(
        ExtensionAPI::CreateWithDefaultConfiguration());
    EXPECT_TRUE(api->IsAvailable("ttsEngine", extension.get(),
                                 Feature::BLESSED_EXTENSION_CONTEXT, GURL(),
                                 CheckAliasStatus::NOT_ALLOWED)
                    .is_available());
    EXPECT_FALSE(api->IsAvailable("tts", extension.get(),
                                  Feature::BLESSED_EXTENSION_CONTEXT, GURL(),
                                  CheckAliasStatus::NOT_ALLOWED)
                     .is_available());
  }

  // Conversely, extension with the "tts" permission but not the "ttsEngine"
  // permission shouldn't get the "ttsEngine" permission.
  {
    scoped_refptr<Extension> extension =
        CreateExtensionWithPermission("tts");
    std::unique_ptr<ExtensionAPI> api(
        ExtensionAPI::CreateWithDefaultConfiguration());
    EXPECT_FALSE(api->IsAvailable("ttsEngine", extension.get(),
                                  Feature::BLESSED_EXTENSION_CONTEXT, GURL(),
                                  CheckAliasStatus::NOT_ALLOWED)
                     .is_available());
    EXPECT_TRUE(api->IsAvailable("tts", extension.get(),
                                 Feature::BLESSED_EXTENSION_CONTEXT, GURL(),
                                 CheckAliasStatus::NOT_ALLOWED)
                    .is_available());
  }
}

bool MatchesURL(
    ExtensionAPI* api, const std::string& api_name, const std::string& url) {
  return api
      ->IsAvailable(api_name, nullptr, Feature::WEB_PAGE_CONTEXT, GURL(url),
                    CheckAliasStatus::NOT_ALLOWED)
      .is_available();
}

TEST(ExtensionAPITest, URLMatching) {
  std::unique_ptr<ExtensionAPI> api(
      ExtensionAPI::CreateWithDefaultConfiguration());

  // "app" API is available to all URLs that content scripts can be injected.
  EXPECT_TRUE(MatchesURL(api.get(), "app", "http://example.com/example.html"));
  EXPECT_TRUE(MatchesURL(api.get(), "app", "https://blah.net"));
  EXPECT_TRUE(MatchesURL(api.get(), "app", "file://somefile.html"));

  // Also to internal URLs.
  EXPECT_TRUE(MatchesURL(api.get(), "app", "about:flags"));
  EXPECT_TRUE(MatchesURL(api.get(), "app", "chrome://flags"));

  // "app" should be available to chrome-extension URLs.
  EXPECT_TRUE(MatchesURL(api.get(), "app",
                          "chrome-extension://fakeextension"));

  // "storage" API (for example) isn't available to any URLs.
  EXPECT_FALSE(MatchesURL(api.get(), "storage",
                          "http://example.com/example.html"));
  EXPECT_FALSE(MatchesURL(api.get(), "storage", "https://blah.net"));
  EXPECT_FALSE(MatchesURL(api.get(), "storage", "file://somefile.html"));
  EXPECT_FALSE(MatchesURL(api.get(), "storage", "about:flags"));
  EXPECT_FALSE(MatchesURL(api.get(), "storage", "chrome://flags"));
  EXPECT_FALSE(MatchesURL(api.get(), "storage",
                          "chrome-extension://fakeextension"));
}

TEST(ExtensionAPITest, GetAPINameFromFullName) {
  struct {
    std::string input;
    std::string api_name;
    std::string child_name;
  } test_data[] = {
    { "", "", "" },
    { "unknown", "", "" },
    { "bookmarks", "bookmarks", "" },
    { "bookmarks.", "bookmarks", "" },
    { ".bookmarks", "", "" },
    { "bookmarks.create", "bookmarks", "create" },
    { "bookmarks.create.", "bookmarks", "create." },
    { "bookmarks.create.monkey", "bookmarks", "create.monkey" },
    { "bookmarkManagerPrivate", "bookmarkManagerPrivate", "" },
    { "bookmarkManagerPrivate.copy", "bookmarkManagerPrivate", "copy" }
  };

  std::unique_ptr<ExtensionAPI> api(
      ExtensionAPI::CreateWithDefaultConfiguration());
  for (size_t i = 0; i < arraysize(test_data); ++i) {
    std::string child_name;
    std::string api_name = api->GetAPINameFromFullName(test_data[i].input,
                                                       &child_name);
    EXPECT_EQ(test_data[i].api_name, api_name) << test_data[i].input;
    EXPECT_EQ(test_data[i].child_name, child_name) << test_data[i].input;
  }
}

TEST(ExtensionAPITest, DefaultConfigurationFeatures) {
  std::unique_ptr<ExtensionAPI> api(
      ExtensionAPI::CreateWithDefaultConfiguration());

  const SimpleFeature* browser_action = static_cast<const SimpleFeature*>(
      api->GetFeatureDependency("api:browserAction"));
  const SimpleFeature* browser_action_set_title =
      static_cast<const SimpleFeature*>(
          api->GetFeatureDependency("api:browserAction.setTitle"));

  struct {
    const SimpleFeature* feature;
    // TODO(aa): More stuff to test over time.
  } test_data[] = {{browser_action}, {browser_action_set_title}};

  for (size_t i = 0; i < arraysize(test_data); ++i) {
    const SimpleFeature* feature = test_data[i].feature;
    ASSERT_TRUE(feature) << i;

    EXPECT_TRUE(feature->allowlist().empty());
    EXPECT_TRUE(feature->extension_types().empty());

    EXPECT_FALSE(feature->location());
    EXPECT_TRUE(feature->platforms().empty());
    EXPECT_FALSE(feature->min_manifest_version());
    EXPECT_FALSE(feature->max_manifest_version());
  }
}

static const base::DictionaryValue* GetDictChecked(
    const base::DictionaryValue* dict,
    const std::string& key) {
  const base::DictionaryValue* out = nullptr;
  CHECK(dict->GetDictionary(key, &out)) << key;
  return out;
}

static std::string GetStringChecked(const base::DictionaryValue* dict,
                                    const std::string& key) {
  std::string out;
  CHECK(dict->GetString(key, &out)) << key;
  return out;
}

TEST(ExtensionAPITest, TypesHaveNamespace) {
  std::unique_ptr<ExtensionAPI> api(
      ExtensionAPI::CreateWithDefaultConfiguration());

  // Returns the dictionary that has |key|: |value|.
  auto get_dict_from_list = [](
      const base::ListValue* list, const std::string& key,
      const std::string& value) -> const base::DictionaryValue* {
    const base::DictionaryValue* ret = nullptr;
    for (const auto& val : *list) {
      const base::DictionaryValue* dict = nullptr;
      if (!val.GetAsDictionary(&dict))
        continue;
      std::string str;
      if (dict->GetString(key, &str) && str == value) {
        ret = dict;
        break;
      }
    }
    return ret;
  };

  const base::DictionaryValue* schema = api->GetSchema("sessions");
  ASSERT_TRUE(schema);

  const base::ListValue* types = nullptr;
  ASSERT_TRUE(schema->GetList("types", &types));
  {
    const base::DictionaryValue* session_type =
        get_dict_from_list(types, "id", "sessions.Session");
    ASSERT_TRUE(session_type);
    const base::DictionaryValue* props =
        GetDictChecked(session_type, "properties");
    const base::DictionaryValue* tab = GetDictChecked(props, "tab");
    EXPECT_EQ("tabs.Tab", GetStringChecked(tab, "$ref"));
    const base::DictionaryValue* window = GetDictChecked(props, "window");
    EXPECT_EQ("windows.Window", GetStringChecked(window, "$ref"));
  }
  {
    const base::DictionaryValue* device_type =
        get_dict_from_list(types, "id", "sessions.Device");
    ASSERT_TRUE(device_type);
    const base::DictionaryValue* props =
        GetDictChecked(device_type, "properties");
    const base::DictionaryValue* sessions = GetDictChecked(props, "sessions");
    const base::DictionaryValue* items = GetDictChecked(sessions, "items");
    EXPECT_EQ("sessions.Session", GetStringChecked(items, "$ref"));
  }
  const base::ListValue* functions = nullptr;
  ASSERT_TRUE(schema->GetList("functions", &functions));
  {
    const base::DictionaryValue* get_recently_closed =
        get_dict_from_list(functions, "name", "getRecentlyClosed");
    ASSERT_TRUE(get_recently_closed);
    const base::ListValue* parameters = nullptr;
    ASSERT_TRUE(get_recently_closed->GetList("parameters", &parameters));
    const base::DictionaryValue* filter =
        get_dict_from_list(parameters, "name", "filter");
    ASSERT_TRUE(filter);
    EXPECT_EQ("sessions.Filter", GetStringChecked(filter, "$ref"));
  }

  schema = api->GetSchema("types");
  ASSERT_TRUE(schema);
  ASSERT_TRUE(schema->GetList("types", &types));
  {
    const base::DictionaryValue* chrome_setting =
        get_dict_from_list(types, "id", "types.ChromeSetting");
    ASSERT_TRUE(chrome_setting);
    EXPECT_EQ("types.ChromeSetting",
              GetStringChecked(chrome_setting, "customBindings"));
  }
}

// Tests API availability with an empty manifest.
TEST(ExtensionAPITest, NoPermissions) {
  const struct {
    const char* permission_name;
    bool expect_success;
  } kTests[] = {
      // Test default module/package permission.
      {"extension", true},
      {"i18n", true},
      {"permissions", true},
      {"runtime", true},
      {"test", true},
      // These require manifest keys.
      {"browserAction", false},
      {"pageAction", false},
      {"pageActions", false},
      // Some negative tests.
      {"bookmarks", false},
      {"cookies", false},
      {"history", false},
      // Make sure we find the module name after stripping '.'
      {"runtime.abcd.onStartup", true},
      // Test Tabs/Windows functions.
      {"tabs.create", true},
      {"tabs.duplicate", true},
      {"tabs.onRemoved", true},
      {"tabs.remove", true},
      {"tabs.update", true},
      {"tabs.getSelected", true},
      {"tabs.onUpdated", true},
      {"windows.get", true},
      {"windows.create", true},
      {"windows.remove", true},
      {"windows.update", true},
      // Test some allowlisted functions. These require no permissions.
      {"app.getDetails", true},
      {"app.getIsInstalled", true},
      {"app.installState", true},
      {"app.runningState", true},
      {"management.getPermissionWarningsByManifest", true},
      {"management.uninstallSelf", true},
      // But other functions in those modules do.
      {"management.getPermissionWarningsById", false},
      {"runtime.connectNative", false},
  };

  std::unique_ptr<ExtensionAPI> extension_api(
      ExtensionAPI::CreateWithDefaultConfiguration());
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();

  for (size_t i = 0; i < arraysize(kTests); ++i) {
    EXPECT_EQ(kTests[i].expect_success,
              extension_api
                  ->IsAvailable(kTests[i].permission_name, extension.get(),
                                Feature::BLESSED_EXTENSION_CONTEXT, GURL(),
                                CheckAliasStatus::NOT_ALLOWED)
                  .is_available())
        << "Permission being tested: " << kTests[i].permission_name;
  }
}

// Tests that permissions that require manifest keys are available when those
// keys are present.
TEST(ExtensionAPITest, ManifestKeys) {
  std::unique_ptr<ExtensionAPI> extension_api(
      ExtensionAPI::CreateWithDefaultConfiguration());

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test")
          .SetAction(ExtensionBuilder::ActionType::BROWSER_ACTION)
          .Build();

  EXPECT_TRUE(extension_api
                  ->IsAvailable("browserAction", extension.get(),
                                Feature::BLESSED_EXTENSION_CONTEXT, GURL(),
                                CheckAliasStatus::NOT_ALLOWED)
                  .is_available());
  EXPECT_FALSE(extension_api
                   ->IsAvailable("pageAction", extension.get(),
                                 Feature::BLESSED_EXTENSION_CONTEXT, GURL(),
                                 CheckAliasStatus::NOT_ALLOWED)
                   .is_available());
}

}  // namespace extensions
