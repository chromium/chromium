// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "chrome/renderer/searchbox/search_bouncer.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/webplugininfo.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#endif

#if BUILDFLAG(ENABLE_NACL)
#include "third_party/blink/public/platform/web_string.h"
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
      base::UTF8ToUTF16("nacl"), base::UTF8ToUTF16(manifest_url));
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
    extensions::Manifest::Location location, bool is_from_webstore,
    bool is_hosted_app, const std::string& app_url) {
  int flags = is_from_webstore ?
      extensions::Extension::FROM_WEBSTORE:
      extensions::Extension::NO_FLAGS;

  base::DictionaryValue manifest;
  manifest.SetString("name", "NaCl Extension");
  manifest.SetString("version", "1");
  manifest.SetInteger("manifest_version", 2);
  if (is_hosted_app) {
    auto url_list = std::make_unique<base::ListValue>();
    url_list->AppendString(app_url);
    manifest.Set(extensions::manifest_keys::kWebURLs, std::move(url_list));
    manifest.SetString(extensions::manifest_keys::kLaunchWebURL, app_url);
  }
  std::string error;
  return extensions::Extension::Create(base::FilePath(), location, manifest,
                                       flags, &error);
}

scoped_refptr<const extensions::Extension> CreateExtension(
    bool is_from_webstore) {
  return CreateTestExtension(
      extensions::Manifest::INTERNAL, is_from_webstore, kNotHostedApp,
      std::string());
}

scoped_refptr<const extensions::Extension> CreateExtensionWithLocation(
    extensions::Manifest::Location location, bool is_from_webstore) {
  return CreateTestExtension(
      location, is_from_webstore, kNotHostedApp, std::string());
}

scoped_refptr<const extensions::Extension> CreateHostedApp(
    bool is_from_webstore, const std::string& app_url) {
  return CreateTestExtension(extensions::Manifest::INTERNAL,
                             is_from_webstore,
                             kHostedApp,
                             app_url);
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
        CreateExtensionWithLocation(extensions::Manifest::UNPACKED,
                                    kExtensionNotFromWebStore)
            .get()));
  }
  // Component extensions are allowed without --enable-nacl.
  {
    EXPECT_TRUE(ChromeContentRendererClient::IsNativeNaClAllowed(
        GURL(kExtensionUrl), kNaClRestricted,
        CreateExtensionWithLocation(extensions::Manifest::COMPONENT,
                                    kExtensionNotFromWebStore)
            .get()));
  }
  {
    EXPECT_TRUE(ChromeContentRendererClient::IsNativeNaClAllowed(
        GURL(kExtensionUrl), kNaClRestricted,
        CreateExtensionWithLocation(extensions::Manifest::EXTERNAL_COMPONENT,
                                    kExtensionNotFromWebStore)
            .get()));
  }
  // Extensions that are force installed by policy are allowed without
  // --enable-nacl.
  {
    EXPECT_TRUE(ChromeContentRendererClient::IsNativeNaClAllowed(
        GURL(kExtensionUrl), kNaClRestricted,
        CreateExtensionWithLocation(extensions::Manifest::EXTERNAL_POLICY,
                                    kExtensionNotFromWebStore)
            .get()));
    EXPECT_TRUE(ChromeContentRendererClient::IsNativeNaClAllowed(
        GURL(kExtensionUrl), kNaClRestricted,
        CreateExtensionWithLocation(
            extensions::Manifest::EXTERNAL_POLICY_DOWNLOAD,
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

// SearchBouncer doesn't exist on Android.
#if !defined(OS_ANDROID)
TEST_F(ChromeContentRendererClientTest, ShouldSuppressErrorPage) {
  ChromeContentRendererClient client;
  SearchBouncer::GetInstance()->SetNewTabPageURL(GURL("http://example.com/n"));
  EXPECT_FALSE(client.ShouldSuppressErrorPage(nullptr,
                                              GURL("http://example.com")));
  EXPECT_TRUE(client.ShouldSuppressErrorPage(nullptr,
                                             GURL("http://example.com/n")));
  SearchBouncer::GetInstance()->SetNewTabPageURL(GURL::EmptyGURL());
}

TEST_F(ChromeContentRendererClientTest, ShouldTrackUseCounter) {
  ChromeContentRendererClient client;
  SearchBouncer::GetInstance()->SetNewTabPageURL(GURL("http://example.com/n"));
  EXPECT_TRUE(client.ShouldTrackUseCounter(GURL("http://example.com")));
  EXPECT_FALSE(client.ShouldTrackUseCounter(GURL("http://example.com/n")));
  SearchBouncer::GetInstance()->SetNewTabPageURL(GURL::EmptyGURL());
}
#endif
