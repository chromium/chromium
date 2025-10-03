// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_content_renderer_client.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/privacy_budget/scoped_privacy_budget_config.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/common/extensions/extension_test_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/manifest_constants.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
using extensions::mojom::ManifestLocation;
#endif

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)
const bool kNotHostedApp = false;
const bool kHostedApp = true;
#endif

}  // namespace

class ChromeContentRendererClientTest : public testing::Test {
 public:
  void SetUp() override {
    // Ensure that this looks like the renderer process based on the command
    // line.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kProcessType, switches::kRendererProcess);
  }
};

#if BUILDFLAG(ENABLE_EXTENSIONS)
scoped_refptr<const extensions::Extension> CreateTestExtension(
    ManifestLocation location,
    bool is_from_webstore,
    bool is_hosted_app,
    const std::string& app_url) {
  int flags = is_from_webstore ?
      extensions::Extension::FROM_WEBSTORE:
      extensions::Extension::NO_FLAGS;

  base::Value::Dict manifest;
  manifest.Set("name", "NaCl Extension");
  manifest.Set("version", "1");
  manifest.Set("manifest_version", 2);
  if (is_hosted_app) {
    base::Value::List url_list;
    url_list.Append(app_url);
    manifest.SetByDottedPath(extensions::manifest_keys::kWebURLs,
                             std::move(url_list));
    manifest.SetByDottedPath(extensions::manifest_keys::kLaunchWebURL, app_url);
  }
  std::u16string error;
  return extensions::Extension::Create(base::FilePath(), location, manifest,
                                       flags, &error);
}

scoped_refptr<const extensions::Extension> CreateExtension(
    bool is_from_webstore) {
  return CreateTestExtension(ManifestLocation::kInternal, is_from_webstore,
                             kNotHostedApp, std::string());
}

scoped_refptr<const extensions::Extension> CreateExtensionWithLocation(
    ManifestLocation location,
    bool is_from_webstore) {
  return CreateTestExtension(
      location, is_from_webstore, kNotHostedApp, std::string());
}

scoped_refptr<const extensions::Extension> CreateHostedApp(
    bool is_from_webstore, const std::string& app_url) {
  return CreateTestExtension(ManifestLocation::kInternal, is_from_webstore,
                             kHostedApp, app_url);
}

TEST_F(ChromeContentRendererClientTest, ExtensionsClientInitialized) {
  auto* extensions_client = extensions::ExtensionsClient::Get();
  ASSERT_TRUE(extensions_client);

  // Ensure that the availability map is initialized correctly.
  const auto& map =
      extensions_client->GetFeatureDelegatedAvailabilityCheckMap();
  EXPECT_TRUE(!map.empty());
  for (const auto* feature :
       extension_test_util::GetExpectedDelegatedFeaturesForTest()) {
    EXPECT_EQ(1u, map.count(feature));
  }
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
