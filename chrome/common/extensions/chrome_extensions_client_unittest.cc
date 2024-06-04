// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/chrome_extensions_client.h"

#include <memory>
#include <set>
#include <string>

#include "base/functional/bind.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/manifest_handler_registry.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/test/test_context_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class ChromeExtensionsClientTest : public testing::Test {
 public:
  void SetUp() override {
    extensions_client_ = std::make_unique<ChromeExtensionsClient>();
    ExtensionsClient::Set(extensions_client_.get());
  }

 private:
  std::unique_ptr<ChromeExtensionsClient> extensions_client_;
};

base::span<const char* const> GetFeatureList() {
  constexpr const char* feature_list[] = {"AllowedFeature",
                                          "DisallowedFeature"};
  return base::make_span(feature_list);
}

bool FeatureDelegatedCheck(const std::string& api_full_name,
                           const Extension* extension,
                           mojom::ContextType context,
                           const GURL& url,
                           Feature::Platform platform,
                           int context_id,
                           bool check_developer_mode,
                           const ContextData& context_data) {
  return api_full_name == "AllowedFeature";
}

Feature::FeatureDelegatedAvailabilityCheckMap
CreateFeatureDelegatedAvailabilityCheckMap() {
  Feature::FeatureDelegatedAvailabilityCheckMap map;
  auto feature_list = GetFeatureList();
  for (const auto* item : feature_list) {
    map.emplace(item, base::BindRepeating(&FeatureDelegatedCheck));
  }
  return map;
}

TEST_F(ChromeExtensionsClientTest, FeatureDelegatedAvailabilityCheckMap) {
  auto* client = ExtensionsClient::Get();
  client->SetFeatureDelegatedAvailabilityCheckMap(
      CreateFeatureDelegatedAvailabilityCheckMap());
  {
    const auto& map = client->GetFeatureDelegatedAvailabilityCheckMap();
    EXPECT_EQ(2u, map.size());

    ASSERT_EQ(1u, map.count("AllowedFeature"));
    bool allowed_result =
        map.at("AllowedFeature")
            .Run("AllowedFeature", /*extension=*/nullptr,
                 mojom::ContextType::kUnspecified, GURL(),
                 Feature::Platform::UNSPECIFIED_PLATFORM, /*context_id*/ 0,
                 /*check_developer_mode=*/false, TestContextData());
    EXPECT_TRUE(allowed_result);

    ASSERT_EQ(1u, map.count("DisallowedFeature"));
    bool disallowed_result =
        map.at("DisallowedFeature")
            .Run("DisallowedFeature", /*extension=*/nullptr,
                 mojom::ContextType::kUnspecified, GURL(),
                 Feature::Platform::UNSPECIFIED_PLATFORM, /*context_id*/ 0,
                 /*check_developer_mode=*/false, TestContextData());
    EXPECT_FALSE(disallowed_result);
  }
}

// Test that a browser action extension returns a path to an icon.
TEST_F(ChromeExtensionsClientTest, GetBrowserImagePaths) {
  base::FilePath install_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &install_dir));
  install_dir = install_dir.AppendASCII("extensions")
                    .AppendASCII("api_test")
                    .AppendASCII("browser_action")
                    .AppendASCII("basics");

  std::string error;
  scoped_refptr<Extension> extension(
      file_util::LoadExtension(install_dir, mojom::ManifestLocation::kUnpacked,
                               Extension::NO_FLAGS, &error));
  ASSERT_TRUE(extension.get());

  // The extension contains one icon.
  std::set<base::FilePath> paths =
      ExtensionsClient::Get()->GetBrowserImagePaths(extension.get());
  ASSERT_EQ(1u, paths.size());
  EXPECT_EQ("icon.png", paths.begin()->BaseName().AsUTF8Unsafe());
}

// Test that extensions with zero-length action icons will not load.
TEST_F(ChromeExtensionsClientTest, CheckZeroLengthActionIconFiles) {
  base::FilePath install_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &install_dir));

  // Try to install an extension with a zero-length browser action icon file.
  base::FilePath ext_dir = install_dir.AppendASCII("extensions")
                               .AppendASCII("bad")
                               .AppendASCII("Extensions")
                               .AppendASCII("gggggggggggggggggggggggggggggggg");

  std::string error;
  scoped_refptr<Extension> extension2(
      file_util::LoadExtension(ext_dir, mojom::ManifestLocation::kUnpacked,
                               Extension::NO_FLAGS, &error));
  EXPECT_FALSE(extension2.get());
  EXPECT_EQ("Could not load icon 'icon.png' specified in 'browser_action'.",
            error);

  // Try to install an extension with a zero-length page action icon file.
  ext_dir = install_dir.AppendASCII("extensions")
                .AppendASCII("bad")
                .AppendASCII("Extensions")
                .AppendASCII("hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh");

  scoped_refptr<Extension> extension3(
      file_util::LoadExtension(ext_dir, mojom::ManifestLocation::kUnpacked,
                               Extension::NO_FLAGS, &error));
  EXPECT_FALSE(extension3.get());
  EXPECT_EQ("Could not load icon 'icon.png' specified in 'page_action'.",
            error);
}

// Test that the ManifestHandlerRegistry handler map hasn't overflowed.
// If this test fails, increase ManifestHandlerRegistry::kHandlerMax.
TEST_F(ChromeExtensionsClientTest, CheckManifestHandlerRegistryForOverflow) {
  ManifestHandlerRegistry* registry = ManifestHandlerRegistry::Get();
  ASSERT_TRUE(registry);
  ASSERT_LT(0u, registry->handlers_.size());
  EXPECT_LE(registry->handlers_.size(), ManifestHandlerRegistry::kHandlerMax);
}

}  // namespace extensions
