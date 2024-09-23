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
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/privacy_budget/scoped_privacy_budget_config.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/webplugininfo.h"
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

#if BUILDFLAG(ENABLE_NACL)
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#endif

#if BUILDFLAG(ENABLE_NACL)
using blink::WebPluginParams;
using blink::WebString;
using blink::WebVector;
#endif

using content::WebPluginInfo;
using content::WebPluginMimeType;

#if BUILDFLAG(ENABLE_EXTENSIONS)
using extensions::mojom::ManifestLocation;
#endif

namespace {

#if BUILDFLAG(ENABLE_NACL)
const bool kNaClRestricted = false;
const bool kNaClUnrestricted = true;
const bool kExtensionNotFromWebStore = false;
const bool kExtensionFromWebStore = true;
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
const bool kNotHostedApp = false;
const bool kHostedApp = true;
#endif

#if BUILDFLAG(ENABLE_NACL)
const char kExtensionUrl[] = "chrome-extension://extension_id/background.html";

#endif

void AddContentTypeHandler(content::WebPluginInfo* info,
                           const char* mime_type,
                           const char* manifest_url) {
  content::WebPluginMimeType mime_type_info;
  mime_type_info.mime_type = mime_type;
  mime_type_info.additional_params.emplace_back(
      u"nacl", base::UTF8ToUTF16(manifest_url));
  info->mime_types.push_back(mime_type_info);
}

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
  std::string error;
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
  EXPECT_EQ(7u, map.size());
  for (const auto* feature :
       extension_test_util::GetExpectedDelegatedFeaturesForTest()) {
    EXPECT_EQ(1u, map.count(feature));
  }
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

TEST_F(ChromeContentRendererClientTest, NaClRestriction) {
  // Unknown content types have no NaCl module.
  {
    WebPluginInfo info;
    EXPECT_EQ(GURL(),
              ChromeContentRendererClient::GetNaClContentHandlerURL(
                  "application/x-foo", info));
  }
  // Known content types have a NaCl module.
  {
    WebPluginInfo info;
    AddContentTypeHandler(&info, "application/x-foo", "www.foo.com");
    EXPECT_EQ(GURL("www.foo.com"),
              ChromeContentRendererClient::GetNaClContentHandlerURL(
                  "application/x-foo", info));
  }
#if BUILDFLAG(ENABLE_NACL)
  // --enable-nacl allows all NaCl apps.
  {
    EXPECT_TRUE(ChromeContentRendererClient::IsNativeNaClAllowed(
        GURL(), kNaClUnrestricted,
        CreateExtension(kExtensionNotFromWebStore).get()));
  }
  // Unpacked extensions are allowed without --enable-nacl.
  {
    EXPECT_TRUE(ChromeContentRendererClient::IsNativeNaClAllowed(
        GURL(kExtensionUrl), kNaClRestricted,
        CreateExtensionWithLocation(ManifestLocation::kUnpacked,
                                    kExtensionNotFromWebStore)
            .get()));
  }
  // Component extensions are allowed without --enable-nacl.
  {
    EXPECT_TRUE(ChromeContentRendererClient::IsNativeNaClAllowed(
        GURL(kExtensionUrl), kNaClRestricted,
        CreateExtensionWithLocation(ManifestLocation::kComponent,
                                    kExtensionNotFromWebStore)
            .get()));
  }
  {
    EXPECT_TRUE(ChromeContentRendererClient::IsNativeNaClAllowed(
        GURL(kExtensionUrl), kNaClRestricted,
        CreateExtensionWithLocation(ManifestLocation::kExternalComponent,
                                    kExtensionNotFromWebStore)
            .get()));
  }
  // Extensions that are force installed by policy are allowed without
  // --enable-nacl.
  {
    EXPECT_TRUE(ChromeContentRendererClient::IsNativeNaClAllowed(
        GURL(kExtensionUrl), kNaClRestricted,
        CreateExtensionWithLocation(ManifestLocation::kExternalPolicy,
                                    kExtensionNotFromWebStore)
            .get()));
    EXPECT_TRUE(ChromeContentRendererClient::IsNativeNaClAllowed(
        GURL(kExtensionUrl), kNaClRestricted,
        CreateExtensionWithLocation(ManifestLocation::kExternalPolicyDownload,
                                    kExtensionNotFromWebStore)
            .get()));
  }
  // CWS extensions are allowed without --enable-nacl if called from an
  // extension url.
  {
    EXPECT_TRUE(ChromeContentRendererClient::IsNativeNaClAllowed(
        GURL(kExtensionUrl), kNaClRestricted,
        CreateExtension(kExtensionFromWebStore).get()));
  }
  // Other URLs (including previously-whitelisted URLs) are blocked
  // without --enable-nacl.
  {
    EXPECT_FALSE(ChromeContentRendererClient::IsNativeNaClAllowed(
        GURL("https://plus.google.com.evil.com/foo1"), kNaClRestricted,
        nullptr));
    EXPECT_FALSE(ChromeContentRendererClient::IsNativeNaClAllowed(
        GURL("https://talkgadget.google.com/hangouts/foo1"), kNaClRestricted,
        nullptr));
  }
  // Non chrome-extension:// URLs belonging to hosted apps are allowed for
  // webstore installed hosted apps.
  {
    EXPECT_TRUE(ChromeContentRendererClient::IsNativeNaClAllowed(
        GURL("http://example.com/test.html"), kNaClRestricted,
        CreateHostedApp(kExtensionFromWebStore, "http://example.com/").get()));
    EXPECT_FALSE(ChromeContentRendererClient::IsNativeNaClAllowed(
        GURL("http://example.com/test.html"), kNaClRestricted,
        CreateHostedApp(kExtensionNotFromWebStore, "http://example.com/")
            .get()));
    EXPECT_FALSE(ChromeContentRendererClient::IsNativeNaClAllowed(
        GURL("http://example.evil.com/test.html"), kNaClRestricted,
        CreateHostedApp(kExtensionNotFromWebStore, "http://example.com/")
            .get()));
  }
#endif  // BUILDFLAG(ENABLE_NACL)
}
