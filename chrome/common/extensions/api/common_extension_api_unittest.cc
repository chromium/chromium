// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_features_unittest.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/feature_session_type.mojom.h"
#include "extensions/test/test_context_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

const char* const kTestFeatures[] = {
    "test1", "test2", "test3",  "test4",  "test5",   "test6",   "test7",
    "test8", "test9", "test10", "test11", "parent1", "parent2", "parent3",
};

const char* const kAliasTestApis[] = {"alias_api_source"};

const char* const kSessionTypeTestFeatures[] = {
    "test6", "kiosk_only", "non_kiosk", "multiple_session_types",
    "autolaunched_kiosk"};

struct FeatureSessionTypesTestData {
  std::string api_name;
  bool expect_available;
  mojom::FeatureSessionType current_session_type;
};

class TestExtensionAPI : public ExtensionAPI {
 public:
  TestExtensionAPI() {}

  TestExtensionAPI(const TestExtensionAPI&) = delete;
  TestExtensionAPI& operator=(const TestExtensionAPI&) = delete;

  ~TestExtensionAPI() override {}

  void add_fake_schema(const std::string& name) { fake_schemas_.insert(name); }

 private:
  bool IsKnownAPI(const std::string& name, ExtensionsClient* client) override {
    return fake_schemas_.count(name) != 0;
  }

  std::set<std::string> fake_schemas_;
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
    raw_ptr<ExtensionAPI> api;
    bool expect_populated;
  } test_data[] = {
    { shared_instance, true },
    { new_instance.get(), true },
    { &empty_instance, false }
  };

  for (size_t i = 0; i < std::size(test_data); ++i) {
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

  for (size_t i = 0; i < std::size(test_data); ++i) {
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
    mojom::ContextType context;
    GURL url;
  } test_data[] = {
      {"test1", false, mojom::ContextType::kWebPage, GURL()},
      {"test1", true, mojom::ContextType::kPrivilegedExtension, GURL()},
      {"test1", true, mojom::ContextType::kUnprivilegedExtension, GURL()},
      {"test1", true, mojom::ContextType::kContentScript, GURL()},
      {"test2", true, mojom::ContextType::kWebPage, GURL("http://google.com")},
      {"test2", false, mojom::ContextType::kPrivilegedExtension,
       GURL("http://google.com")},
      {"test2.foo", false, mojom::ContextType::kWebPage,
       GURL("http://google.com")},
      {"test2.foo", true, mojom::ContextType::kContentScript, GURL()},
      {"test3", false, mojom::ContextType::kWebPage, GURL("http://google.com")},
      {"test3.foo", true, mojom::ContextType::kWebPage,
       GURL("http://google.com")},
      {"test3.foo", true, mojom::ContextType::kPrivilegedExtension,
       GURL("http://bad.com")},
      {"test4", true, mojom::ContextType::kPrivilegedExtension,
       GURL("http://bad.com")},
      {"test4.foo", false, mojom::ContextType::kPrivilegedExtension,
       GURL("http://bad.com")},
      {"test4.foo", false, mojom::ContextType::kUnprivilegedExtension,
       GURL("http://bad.com")},
      {"test4.foo.foo", true, mojom::ContextType::kContentScript, GURL()},
      {"test5", true, mojom::ContextType::kWebPage, GURL("http://foo.com")},
      {"test5", false, mojom::ContextType::kWebPage, GURL("http://bar.com")},
      {"test5.blah", true, mojom::ContextType::kWebPage,
       GURL("http://foo.com")},
      {"test5.blah", false, mojom::ContextType::kWebPage,
       GURL("http://bar.com")},
      {"test6", false, mojom::ContextType::kPrivilegedExtension, GURL()},
      {"test6.foo", true, mojom::ContextType::kPrivilegedExtension, GURL()},
      {"test7", true, mojom::ContextType::kWebPage, GURL("http://foo.com")},
      {"test7.foo", false, mojom::ContextType::kWebPage,
       GURL("http://bar.com")},
      {"test7.foo", true, mojom::ContextType::kWebPage, GURL("http://foo.com")},
      {"test7.bar", false, mojom::ContextType::kWebPage,
       GURL("http://bar.com")},
      {"test7.bar", false, mojom::ContextType::kWebPage,
       GURL("http://foo.com")},
      {"test8", true, mojom::ContextType::kWebUi, GURL("chrome://test/")},
      {"test8", true, mojom::ContextType::kWebUi, GURL("chrome://other-test/")},
      {"test8", false, mojom::ContextType::kWebUi, GURL("chrome://dangerous/")},
      {"test8", false, mojom::ContextType::kWebUi,
       GURL("chrome-untrusted://test/")},
      {"test8", false, mojom::ContextType::kUntrustedWebUi,
       GURL("chrome-untrusted://test/")},
      {"test8", false, mojom::ContextType::kUntrustedWebUi,
       GURL("chrome://test/*")},
      {"test9", true, mojom::ContextType::kUntrustedWebUi,
       GURL("chrome-untrusted://test/")},
      {"test9", true, mojom::ContextType::kUntrustedWebUi,
       GURL("chrome-untrusted://other-test/")},
      {"test9", false, mojom::ContextType::kUntrustedWebUi,
       GURL("chrome-untrusted://dangerous/")},
      {"test9", false, mojom::ContextType::kUntrustedWebUi,
       GURL("chrome://test/")},
      {"test9", false, mojom::ContextType::kWebUi, GURL("chrome://test/")},
      {"test9", false, mojom::ContextType::kWebUi,
       GURL("chrome-untrusted://test/*")},

      // Test parent/child.
      {"parent1", true, mojom::ContextType::kContentScript, GURL()},
      {"parent1", false, mojom::ContextType::kWebPage, GURL("http://foo.com")},
      {"parent1.child1", false, mojom::ContextType::kContentScript, GURL()},
      {"parent1.child1", true, mojom::ContextType::kWebPage,
       GURL("http://foo.com")},
      {"parent1.child2", true, mojom::ContextType::kContentScript, GURL()},
      {"parent1.child2", false, mojom::ContextType::kWebPage,
       GURL("http://foo.com")},
      {"parent2", true, mojom::ContextType::kContentScript, GURL()},
      {"parent2", true, mojom::ContextType::kPrivilegedExtension, GURL()},
      {"parent2", true, mojom::ContextType::kUnprivilegedExtension, GURL()},
      {"parent2.child3", false, mojom::ContextType::kContentScript, GURL()},
      {"parent2.child3", true, mojom::ContextType::kPrivilegedExtension,
       GURL()},
      {"parent2.child3", false, mojom::ContextType::kUnprivilegedExtension,
       GURL()},
      {"parent2.child3.child.child", true, mojom::ContextType::kContentScript,
       GURL()},
      {"parent2.child3.child.child", false,
       mojom::ContextType::kPrivilegedExtension, GURL()},
      {"parent2.child3.child.child", true,
       mojom::ContextType::kUnprivilegedExtension, GURL()},
      {"parent3", true, mojom::ContextType::kContentScript, GURL()},
      {"parent3", false, mojom::ContextType::kPrivilegedExtension, GURL()},
      {"parent3", false, mojom::ContextType::kUnprivilegedExtension, GURL()},
      {"parent3.noparent", true, mojom::ContextType::kContentScript, GURL()},
      {"parent3.noparent", true, mojom::ContextType::kPrivilegedExtension,
       GURL()},
      {"parent3.noparent", true, mojom::ContextType::kUnprivilegedExtension,
       GURL()},
      {"parent3.noparent.child", true, mojom::ContextType::kContentScript,
       GURL()},
      {"parent3.noparent.child", true, mojom::ContextType::kPrivilegedExtension,
       GURL()},
      {"parent3.noparent.child", true,
       mojom::ContextType::kUnprivilegedExtension, GURL()}};

  FeatureProvider api_feature_provider;
  AddUnittestAPIFeatures(&api_feature_provider);

  for (size_t i = 0; i < std::size(test_data); ++i) {
    TestExtensionAPI api;
    api.RegisterDependencyProvider("api", &api_feature_provider);
    for (auto* key : kTestFeatures)
      api.add_fake_schema(key);
    ExtensionAPI::OverrideSharedInstanceForTest scope(&api);

    bool expected = test_data[i].expect_is_available;
    Feature::Availability availability = api.IsAvailable(
        test_data[i].api_full_name, nullptr, test_data[i].context,
        test_data[i].url, CheckAliasStatus::NOT_ALLOWED, kUnspecifiedContextId,
        TestContextData());
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
                               mojom::ContextType::kUnprivilegedExtension,
                               GURL(), CheckAliasStatus::NOT_ALLOWED,
                               kUnspecifiedContextId, TestContextData())
                   .is_available());
  ASSERT_TRUE(api.IsAvailable("alias_api_source", nullptr,
                              mojom::ContextType::kUnprivilegedExtension,
                              GURL(), CheckAliasStatus::ALLOWED,
                              kUnspecifiedContextId, TestContextData())
                  .is_available());
  ASSERT_TRUE(api.IsAvailable("alias_api_source.bar", nullptr,
                              mojom::ContextType::kUnprivilegedExtension,
                              GURL(), CheckAliasStatus::ALLOWED,
                              kUnspecifiedContextId, TestContextData())
                  .is_available());
  ASSERT_FALSE(api.IsAvailable("alias_api_source.foo", nullptr,
                               mojom::ContextType::kUnprivilegedExtension,
                               GURL(), CheckAliasStatus::ALLOWED,
                               kUnspecifiedContextId, TestContextData())
                   .is_available());

  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(base::Value::Dict()
                           .Set("name", "extension")
                           .Set("version", "1")
                           .Set("manifest_version", 2))
          .Build();
  const Feature* test_feature =
      api_feature_provider.GetFeature("alias_api_source");
  ASSERT_TRUE(test_feature);
  ASSERT_FALSE(api.IsAnyFeatureAvailableToContext(
      *test_feature, extension.get(),
      mojom::ContextType::kUnprivilegedExtension, GURL(),
      CheckAliasStatus::NOT_ALLOWED, kUnspecifiedContextId, TestContextData()));
  EXPECT_TRUE(api.IsAnyFeatureAvailableToContext(
      *test_feature, extension.get(),
      mojom::ContextType::kUnprivilegedExtension, GURL(),
      CheckAliasStatus::ALLOWED, kUnspecifiedContextId, TestContextData()));
}

TEST(ExtensionAPITest, IsAnyFeatureAvailableToContext) {
  scoped_refptr<const Extension> app =
      ExtensionBuilder()
          .SetManifest(
              base::Value::Dict()
                  .Set("name", "app")
                  .Set("app", base::Value::Dict().Set(
                                  "background",
                                  base::Value::Dict().Set(
                                      "scripts", base::Value::List().Append(
                                                     "background.js"))))
                  .Set("version", "1")
                  .Set("manifest_version", 2))
          .Build();
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(base::Value::Dict()
                           .Set("name", "extension")
                           .Set("version", "1")
                           .Set("manifest_version", 2))
          .Build();

  struct {
    std::string api_full_name;
    bool expect_is_available;
    mojom::ContextType context;
    raw_ptr<const Extension> extension;
    GURL url;
  } test_data[] = {
      {"test1", false, mojom::ContextType::kWebPage, nullptr, GURL()},
      {"test1", true, mojom::ContextType::kUnprivilegedExtension, nullptr,
       GURL()},
      {"test1", false, mojom::ContextType::kUnprivilegedExtension, app.get(),
       GURL()},
      {"test1", true, mojom::ContextType::kUnprivilegedExtension,
       extension.get(), GURL()},
      {"test2", true, mojom::ContextType::kContentScript, nullptr, GURL()},
      {"test2", true, mojom::ContextType::kWebPage, nullptr,
       GURL("http://google.com")},
      {"test2.foo", false, mojom::ContextType::kWebPage, nullptr,
       GURL("http://google.com")},
      {"test3", true, mojom::ContextType::kContentScript, nullptr, GURL()},
      {"test3", true, mojom::ContextType::kWebPage, nullptr,
       GURL("http://foo.com")},
      {"test4.foo", true, mojom::ContextType::kContentScript, nullptr, GURL()},
      {"test7", false, mojom::ContextType::kWebPage, nullptr,
       GURL("http://google.com")},
      {"test7", true, mojom::ContextType::kWebPage, nullptr,
       GURL("http://foo.com")},
      {"test7", false, mojom::ContextType::kWebPage, nullptr,
       GURL("http://bar.com")},
      {"test10", true, mojom::ContextType::kWebUi, nullptr,
       GURL("chrome://test/")},
      {"test10", true, mojom::ContextType::kWebUi, nullptr,
       GURL("chrome://other-test/")},
      {"test10", false, mojom::ContextType::kUntrustedWebUi, nullptr,
       GURL("chrome-untrusted://test/")},
      {"test11", true, mojom::ContextType::kUntrustedWebUi, nullptr,
       GURL("chrome-untrusted://test/")},
      {"test11", true, mojom::ContextType::kUntrustedWebUi, nullptr,
       GURL("chrome-untrusted://other-test/")},
      {"test11", false, mojom::ContextType::kWebUi, nullptr,
       GURL("chrome://test/")},
  };

  FeatureProvider api_feature_provider;
  AddUnittestAPIFeatures(&api_feature_provider);

  for (size_t i = 0; i < std::size(test_data); ++i) {
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
                  test_data[i].url, CheckAliasStatus::NOT_ALLOWED,
                  kUnspecifiedContextId, TestContextData()))
        << i;
  }
}

TEST(ExtensionAPITest, SessionTypeFeature) {
  scoped_refptr<const Extension> app =
      ExtensionBuilder()
          .SetManifest(
              base::Value::Dict()
                  .Set("name", "app")
                  .Set("app", base::Value::Dict().Set(
                                  "background",
                                  base::Value::Dict().Set(
                                      "scripts", base::Value::List().Append(
                                                     "background.js"))))
                  .Set("version", "1")
                  .Set("manifest_version", 2))
          .Build();

  const std::vector<FeatureSessionTypesTestData> kTestData(
      {{"kiosk_only", true, mojom::FeatureSessionType::kKiosk},
       {"kiosk_only", true, mojom::FeatureSessionType::kAutolaunchedKiosk},
       {"kiosk_only", false, mojom::FeatureSessionType::kRegular},
       {"kiosk_only", false, mojom::FeatureSessionType::kUnknown},
       {"non_kiosk", false, mojom::FeatureSessionType::kKiosk},
       {"non_kiosk", true, mojom::FeatureSessionType::kRegular},
       {"non_kiosk", false, mojom::FeatureSessionType::kUnknown},
       {"autolaunched_kiosk", true,
        mojom::FeatureSessionType::kAutolaunchedKiosk},
       {"autolaunched_kiosk", false, mojom::FeatureSessionType::kKiosk},
       {"autolaunched_kiosk", false, mojom::FeatureSessionType::kRegular},
       {"multiple_session_types", true, mojom::FeatureSessionType::kKiosk},
       {"multiple_session_types", true, mojom::FeatureSessionType::kRegular},
       {"multiple_session_types", false, mojom::FeatureSessionType::kUnknown},
       // test6.foo is available to apps and has no session type restrictions.
       {"test6.foo", true, mojom::FeatureSessionType::kKiosk},
       {"test6.foo", true, mojom::FeatureSessionType::kAutolaunchedKiosk},
       {"test6.foo", true, mojom::FeatureSessionType::kRegular},
       {"test6.foo", true, mojom::FeatureSessionType::kUnknown}});

  FeatureProvider api_feature_provider;
  AddUnittestAPIFeatures(&api_feature_provider);

  for (const auto& test : kTestData) {
    TestExtensionAPI api;
    api.RegisterDependencyProvider("api", &api_feature_provider);
    for (auto* key : kSessionTypeTestFeatures)
      api.add_fake_schema(key);
    ExtensionAPI::OverrideSharedInstanceForTest scope(&api);

    std::unique_ptr<base::AutoReset<mojom::FeatureSessionType>> current_session(
        ScopedCurrentFeatureSessionType(test.current_session_type));
    EXPECT_EQ(test.expect_available,
              api.IsAvailable(test.api_name, app.get(),
                              mojom::ContextType::kPrivilegedExtension, GURL(),
                              CheckAliasStatus::NOT_ALLOWED,
                              kUnspecifiedContextId, TestContextData())
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
  auto manifest = base::Value::Dict()
                      .Set("name", "extension")
                      .Set("version", "1.0")
                      .Set("manifest_version", 2);
  {
    base::Value::List permissions_list;
    for (const auto& i : permissions) {
      permissions_list.Append(i);
    }
    manifest.Set("permissions", std::move(permissions_list));
  }

  std::string error;
  scoped_refptr<Extension> extension(
      Extension::Create(base::FilePath(), mojom::ManifestLocation::kUnpacked,
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
      mojom::ContextType::kPrivilegedExtension, GURL(),
      CheckAliasStatus::NOT_ALLOWED, kUnspecifiedContextId, TestContextData()));
  EXPECT_TRUE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("storage"), nullptr,
      mojom::ContextType::kUnprivilegedExtension, GURL(),
      CheckAliasStatus::NOT_ALLOWED, kUnspecifiedContextId, TestContextData()));
  EXPECT_TRUE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("storage"), nullptr,
      mojom::ContextType::kContentScript, GURL(), CheckAliasStatus::NOT_ALLOWED,
      kUnspecifiedContextId, TestContextData()));

  // "extension" is partially unprivileged.
  EXPECT_TRUE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("extension"), nullptr,
      mojom::ContextType::kPrivilegedExtension, GURL(),
      CheckAliasStatus::NOT_ALLOWED, kUnspecifiedContextId, TestContextData()));
  EXPECT_TRUE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("extension"), nullptr,
      mojom::ContextType::kUnprivilegedExtension, GURL(),
      CheckAliasStatus::NOT_ALLOWED, kUnspecifiedContextId, TestContextData()));
  EXPECT_TRUE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("extension"), nullptr,
      mojom::ContextType::kContentScript, GURL(), CheckAliasStatus::NOT_ALLOWED,
      kUnspecifiedContextId, TestContextData()));
  EXPECT_TRUE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("extension.getURL"), nullptr,
      mojom::ContextType::kContentScript, GURL(), CheckAliasStatus::NOT_ALLOWED,
      kUnspecifiedContextId, TestContextData()));

  // "history" is entirely privileged.
  EXPECT_TRUE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("history"), nullptr,
      mojom::ContextType::kPrivilegedExtension, GURL(),
      CheckAliasStatus::NOT_ALLOWED, kUnspecifiedContextId, TestContextData()));
  EXPECT_FALSE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("history"), nullptr,
      mojom::ContextType::kUnprivilegedExtension, GURL(),
      CheckAliasStatus::NOT_ALLOWED, kUnspecifiedContextId, TestContextData()));
  EXPECT_FALSE(extension_api->IsAnyFeatureAvailableToContext(
      *api_features.GetFeature("history"), nullptr,
      mojom::ContextType::kContentScript, GURL(), CheckAliasStatus::NOT_ALLOWED,
      kUnspecifiedContextId, TestContextData()));
}

scoped_refptr<Extension> CreateHostedApp() {
  base::Value::Dict values;
  values.Set(manifest_keys::kName, "test");
  values.Set(manifest_keys::kVersion, "0.1");
  values.SetByDottedPath(manifest_keys::kWebURLs,
                         base::Value(base::Value::Type::LIST));
  values.SetByDottedPath(manifest_keys::kLaunchWebURL,
                         "http://www.example.com");
  std::string error;
  scoped_refptr<Extension> extension(
      Extension::Create(base::FilePath(), mojom::ManifestLocation::kInternal,
                        values, Extension::NO_FLAGS, &error));
  CHECK(extension.get());
  return extension;
}

scoped_refptr<Extension> CreatePackagedAppWithPermissions(
    const std::set<std::string>& permissions) {
  auto manifest =
      base::Value::Dict()
          .Set(manifest_keys::kName, "test")
          .Set(manifest_keys::kVersion, "0.1")
          .Set(manifest_keys::kApp,
               base::Value::Dict().Set(
                   "background",
                   base::Value::Dict().Set(
                       "scripts", base::Value::List().Append("test.js"))));

  {
    base::Value::List permissions_list;
    for (const auto& i : permissions) {
      permissions_list.Append(i);
    }
    manifest.Set("permissions", std::move(permissions_list));
  }

  std::string error;
  scoped_refptr<Extension> extension(
      Extension::Create(base::FilePath(), mojom::ManifestLocation::kInternal,
                        manifest, Extension::NO_FLAGS, &error));
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
                                 mojom::ContextType::kPrivilegedExtension,
                                 GURL(), CheckAliasStatus::NOT_ALLOWED,
                                 kUnspecifiedContextId, TestContextData())
                   .is_available());
  EXPECT_FALSE(extension_api
                   ->IsAvailable("runtime.id", extension.get(),
                                 mojom::ContextType::kPrivilegedExtension,
                                 GURL(), CheckAliasStatus::NOT_ALLOWED,
                                 kUnspecifiedContextId, TestContextData())
                   .is_available());
  EXPECT_FALSE(extension_api
                   ->IsAvailable("runtime.sendMessage", extension.get(),
                                 mojom::ContextType::kPrivilegedExtension,
                                 GURL(), CheckAliasStatus::NOT_ALLOWED,
                                 kUnspecifiedContextId, TestContextData())
                   .is_available());
  EXPECT_FALSE(extension_api
                   ->IsAvailable("runtime.sendNativeMessage", extension.get(),
                                 mojom::ContextType::kPrivilegedExtension,
                                 GURL(), CheckAliasStatus::NOT_ALLOWED,
                                 kUnspecifiedContextId, TestContextData())
                   .is_available());
  EXPECT_FALSE(extension_api
                   ->IsAvailable("tabs.create", extension.get(),
                                 mojom::ContextType::kPrivilegedExtension,
                                 GURL(), CheckAliasStatus::NOT_ALLOWED,
                                 kUnspecifiedContextId, TestContextData())
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
                                   mojom::ContextType::kPrivilegedExtension,
                                   GURL("http://foo.com"),
                                   CheckAliasStatus::NOT_ALLOWED,
                                   kUnspecifiedContextId, TestContextData())
                     .is_available());
    EXPECT_TRUE(extension_api
                    ->IsAvailable("app.runtime", extension.get(),
                                  mojom::ContextType::kPrivilegedExtension,
                                  GURL("http://foo.com"),
                                  CheckAliasStatus::NOT_ALLOWED,
                                  kUnspecifiedContextId, TestContextData())
                    .is_available());
    EXPECT_TRUE(extension_api
                    ->IsAvailable("app.window", extension.get(),
                                  mojom::ContextType::kPrivilegedExtension,
                                  GURL("http://foo.com"),
                                  CheckAliasStatus::NOT_ALLOWED,
                                  kUnspecifiedContextId, TestContextData())
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
                                  mojom::ContextType::kPrivilegedExtension,
                                  GURL("http://foo.com"),
                                  CheckAliasStatus::NOT_ALLOWED,
                                  kUnspecifiedContextId, TestContextData())
                    .is_available());
    EXPECT_FALSE(extension_api
                     ->IsAvailable("app.runtime", extension.get(),
                                   mojom::ContextType::kPrivilegedExtension,
                                   GURL("http://foo.com"),
                                   CheckAliasStatus::NOT_ALLOWED,
                                   kUnspecifiedContextId, TestContextData())
                     .is_available());
    EXPECT_FALSE(extension_api
                     ->IsAvailable("app.window", extension.get(),
                                   mojom::ContextType::kPrivilegedExtension,
                                   GURL("http://foo.com"),
                                   CheckAliasStatus::NOT_ALLOWED,
                                   kUnspecifiedContextId, TestContextData())
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
                                 mojom::ContextType::kPrivilegedExtension,
                                 GURL(), CheckAliasStatus::NOT_ALLOWED,
                                 kUnspecifiedContextId, TestContextData())
                    .is_available());
    EXPECT_FALSE(api->IsAvailable("tts", extension.get(),
                                  mojom::ContextType::kPrivilegedExtension,
                                  GURL(), CheckAliasStatus::NOT_ALLOWED,
                                  kUnspecifiedContextId, TestContextData())
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
                                  mojom::ContextType::kPrivilegedExtension,
                                  GURL(), CheckAliasStatus::NOT_ALLOWED,
                                  kUnspecifiedContextId, TestContextData())
                     .is_available());
    EXPECT_TRUE(api->IsAvailable("tts", extension.get(),
                                 mojom::ContextType::kPrivilegedExtension,
                                 GURL(), CheckAliasStatus::NOT_ALLOWED,
                                 kUnspecifiedContextId, TestContextData())
                    .is_available());
  }
}

bool MatchesURL(
    ExtensionAPI* api, const std::string& api_name, const std::string& url) {
  return api
      ->IsAvailable(api_name, nullptr, mojom::ContextType::kWebPage, GURL(url),
                    CheckAliasStatus::NOT_ALLOWED, kUnspecifiedContextId,
                    TestContextData())
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
  for (size_t i = 0; i < std::size(test_data); ++i) {
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
    raw_ptr<const SimpleFeature> feature;
    // TODO(aa): More stuff to test over time.
  } test_data[] = {{browser_action}, {browser_action_set_title}};

  for (size_t i = 0; i < std::size(test_data); ++i) {
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

static const base::Value::Dict* GetDictChecked(const base::Value::Dict* dict,
                                               const std::string& key) {
  const base::Value::Dict* out = dict->FindDict(key);
  CHECK(out) << key;
  return out;
}

static std::string GetStringChecked(const base::Value::Dict* dict,
                                    const std::string& key) {
  const std::string* out = dict->FindString(key);
  CHECK(out) << key;
  return *out;
}

TEST(ExtensionAPITest, TypesHaveNamespace) {
  std::unique_ptr<ExtensionAPI> api(
      ExtensionAPI::CreateWithDefaultConfiguration());

  // Returns the dictionary that has |key|: |value|.
  auto get_dict_from_list =
      [](const base::Value::List* list, const std::string& key,
         const std::string& value) -> const base::Value::Dict* {
    for (const auto& val : *list) {
      const base::Value::Dict* dict = val.GetIfDict();
      if (!dict)
        continue;
      if (const std::string* str = dict->FindString(key)) {
        if (*str == value)
          return dict;
      }
    }
    return nullptr;
  };

  const base::Value::Dict* schema = api->GetSchema("sessions");
  ASSERT_TRUE(schema);

  const base::Value::List* types = schema->FindList("types");
  ASSERT_TRUE(types);
  {
    const base::Value::Dict* session_type =
        get_dict_from_list(types, "id", "sessions.Session");
    ASSERT_TRUE(session_type);
    const base::Value::Dict* props = GetDictChecked(session_type, "properties");
    const base::Value::Dict* tab = GetDictChecked(props, "tab");
    EXPECT_EQ("tabs.Tab", GetStringChecked(tab, "$ref"));
    const base::Value::Dict* window = GetDictChecked(props, "window");
    EXPECT_EQ("windows.Window", GetStringChecked(window, "$ref"));
  }
  {
    const base::Value::Dict* device_type =
        get_dict_from_list(types, "id", "sessions.Device");
    ASSERT_TRUE(device_type);
    const base::Value::Dict* props = GetDictChecked(device_type, "properties");
    const base::Value::Dict* sessions = GetDictChecked(props, "sessions");
    const base::Value::Dict* items = GetDictChecked(sessions, "items");
    EXPECT_EQ("sessions.Session", GetStringChecked(items, "$ref"));
  }
  const base::Value::List* functions = schema->FindList("functions");
  ASSERT_TRUE(functions);
  {
    const base::Value::Dict* get_recently_closed =
        get_dict_from_list(functions, "name", "getRecentlyClosed");
    ASSERT_TRUE(get_recently_closed);
    const base::Value::List* parameters =
        get_recently_closed->FindList("parameters");
    ASSERT_TRUE(parameters);
    const base::Value::Dict* filter =
        get_dict_from_list(parameters, "name", "filter");
    ASSERT_TRUE(filter);
    EXPECT_EQ("sessions.Filter", GetStringChecked(filter, "$ref"));
  }

  schema = api->GetSchema("types");
  ASSERT_TRUE(schema);
  types = schema->FindList("types");
  ASSERT_TRUE(types);
  {
    const base::Value::Dict* chrome_setting =
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

  // TODO(https://crbug.com/40804030): Update this to use MV3.
  // Some of the APIs above are deprecated in MV3.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test").SetManifestVersion(2).Build();

  for (size_t i = 0; i < std::size(kTests); ++i) {
    EXPECT_EQ(kTests[i].expect_success,
              extension_api
                  ->IsAvailable(kTests[i].permission_name, extension.get(),
                                mojom::ContextType::kPrivilegedExtension,
                                GURL(), CheckAliasStatus::NOT_ALLOWED,
                                kUnspecifiedContextId, TestContextData())
                  .is_available())
        << "Permission being tested: " << kTests[i].permission_name;
  }
}

// Tests that permissions that require manifest keys are available when those
// keys are present.
TEST(ExtensionAPITest, ManifestKeys) {
  std::unique_ptr<ExtensionAPI> extension_api(
      ExtensionAPI::CreateWithDefaultConfiguration());

  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("Test").SetAction(ActionInfo::Type::kAction).Build();
    EXPECT_TRUE(extension_api
                    ->IsAvailable("action", extension.get(),
                                  mojom::ContextType::kPrivilegedExtension,
                                  GURL(), CheckAliasStatus::NOT_ALLOWED,
                                  kUnspecifiedContextId, TestContextData())
                    .is_available());
  }

  {
    scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
    EXPECT_FALSE(extension_api
                     ->IsAvailable("action", extension.get(),
                                   mojom::ContextType::kPrivilegedExtension,
                                   GURL(), CheckAliasStatus::NOT_ALLOWED,
                                   kUnspecifiedContextId, TestContextData())
                     .is_available());
  }
}

// (TSAN) Tests that ExtensionAPI are able to handle GetSchema from different
// threads.
TEST(ExtensionAPITest, GetSchemaFromDifferentThreads) {
  ExtensionAPI* shared_instance = ExtensionAPI::GetSharedInstance();
  ASSERT_TRUE(shared_instance);
  base::test::TaskEnvironment task_environment;

  base::Thread t("test_thread");
  ASSERT_TRUE(t.Start());

  base::RunLoop run_loop;
  const base::Value::Dict* another_thread_schema = nullptr;

  auto result_cb =
      base::BindLambdaForTesting([&](const base::Value::Dict* res) {
        another_thread_schema = res;
        run_loop.Quit();
      });
  auto task =
      base::BindOnce(&ExtensionAPI::GetSchema,
                     base::Unretained(shared_instance), "storage")
          .Then(base::BindPostTaskToCurrentDefault(std::move(result_cb)));
  t.task_runner()->PostTask(FROM_HERE, std::move(task));

  const auto* current_thread_schema = shared_instance->GetSchema("storage");
  EXPECT_TRUE(current_thread_schema);

  run_loop.Run();

  // The pointers (not only the values) must be the same.
  EXPECT_EQ(another_thread_schema, current_thread_schema);
}

}  // namespace extensions
