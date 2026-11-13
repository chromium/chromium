// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/webui_origin_access.h"

#include <string>
#include <vector>

#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

namespace {

using mojom::ManifestLocation;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

url::Origin OriginFor(const char* url) {
  return url::Origin::Create(GURL(url));
}

scoped_refptr<const Extension> BuildExtension(
    ManifestLocation location,
    const std::vector<std::string>& host_permissions = {},
    const std::string& id = std::string()) {
  return ExtensionBuilder("test")
      .SetLocation(location)
      .AddHostPermissions(host_permissions)
      .SetID(id)
      .Build();
}

scoped_refptr<const Extension> BuildFromManifest(ManifestLocation location,
                                                 base::DictValue manifest) {
  return ExtensionBuilder()
      .SetManifest(std::move(manifest))
      .SetLocation(location)
      .Build();
}

constexpr WebUIOriginUse kAllUses[] = {
    WebUIOriginUse::kFrameSubresource, WebUIOriginUse::kFrameNavigation,
    WebUIOriginUse::kWorker, WebUIOriginUse::kCorsAccess};

// Returns the origins `extension` may use for `use`.
std::vector<url::Origin> Origins(const Extension& extension,
                                 WebUIOriginUse use) {
  return GetAllowedWebUIOrigins(extension, use);
}

// Origins every extension's frames may load subresources from.
std::vector<url::Origin> ExtensionSubresourceOrigins() {
  return {OriginFor("chrome://favicon/"), OriginFor("chrome://extension-icon/"),
          OriginFor("chrome://app-icon/")};
}

// Origins every extension's frames may navigate to.
std::vector<url::Origin> ExtensionNavigationOrigins() {
  return {OriginFor("chrome://favicon/"),
          OriginFor("chrome://extension-icon/")};
}

// Returns `origins` plus the origins every trusted extension's frames get
// without opt-ins.
std::vector<url::Origin> WithTrustedOrigins(std::vector<url::Origin> origins) {
  origins.push_back(OriginFor("chrome://resources/"));
  origins.push_back(OriginFor("chrome://theme/"));
  return origins;
}

std::vector<url::Origin> TrustedSubresourceOrigins() {
  return WithTrustedOrigins(ExtensionSubresourceOrigins());
}

std::vector<url::Origin> TrustedNavigationOrigins() {
  return WithTrustedOrigins(ExtensionNavigationOrigins());
}

}  // namespace

TEST(WebUIOriginAccessTest, ComponentExtensionDefaults) {
  for (ManifestLocation location :
       {ManifestLocation::kComponent, ManifestLocation::kExternalComponent}) {
    SCOPED_TRACE(static_cast<int>(location));
    auto extension = BuildExtension(location);
    EXPECT_THAT(Origins(*extension, WebUIOriginUse::kFrameSubresource),
                UnorderedElementsAreArray(TrustedSubresourceOrigins()));
    EXPECT_THAT(Origins(*extension, WebUIOriginUse::kFrameNavigation),
                UnorderedElementsAreArray(TrustedNavigationOrigins()));
    EXPECT_THAT(Origins(*extension, WebUIOriginUse::kWorker),
                UnorderedElementsAre(OriginFor("chrome://resources/")));
    EXPECT_THAT(Origins(*extension, WebUIOriginUse::kCorsAccess),
                UnorderedElementsAre(OriginFor("chrome://theme/")));
  }
}

TEST(WebUIOriginAccessTest, NonComponentExtensionDefaults) {
  for (ManifestLocation location :
       {ManifestLocation::kInternal, ManifestLocation::kUnpacked,
        ManifestLocation::kCommandLine,
        ManifestLocation::kExternalPolicyDownload}) {
    SCOPED_TRACE(static_cast<int>(location));
    auto extension = BuildExtension(location);
    EXPECT_THAT(Origins(*extension, WebUIOriginUse::kFrameSubresource),
                UnorderedElementsAreArray(ExtensionSubresourceOrigins()));
    EXPECT_THAT(Origins(*extension, WebUIOriginUse::kFrameNavigation),
                UnorderedElementsAreArray(ExtensionNavigationOrigins()));
    EXPECT_THAT(Origins(*extension, WebUIOriginUse::kWorker), IsEmpty());
    EXPECT_THAT(Origins(*extension, WebUIOriginUse::kCorsAccess), IsEmpty());
  }
}

TEST(WebUIOriginAccessTest, ChromeUIHosts) {
  auto extension =
      BuildExtension(ManifestLocation::kComponent, /*host_permissions=*/{},
                     extension_misc::kNewTabPageExtensionId);
  EXPECT_THAT(
      GetAllowedChromeUIHosts(*extension, WebUIOriginUse::kFrameSubresource),
      UnorderedElementsAre("resources", "theme", "favicon", "extension-icon",
                           "app-icon"));
  // chrome-untrusted:// origins are excluded.
  EXPECT_THAT(
      GetAllowedChromeUIHosts(*extension, WebUIOriginUse::kFrameNavigation),
      UnorderedElementsAre("resources", "theme", "favicon", "extension-icon"));
}

TEST(WebUIOriginAccessTest, HostPermissionOptIns) {
  auto extension = BuildExtension(ManifestLocation::kComponent,
                                  {"chrome://favicon2/*", "chrome://image/*"});
  std::vector<url::Origin> expected = TrustedSubresourceOrigins();
  expected.push_back(OriginFor("chrome://favicon2/"));
  expected.push_back(OriginFor("chrome://image/"));
  EXPECT_THAT(Origins(*extension, WebUIOriginUse::kFrameSubresource),
              UnorderedElementsAreArray(expected));
  // Opt-ins are subresource-only.
  EXPECT_THAT(Origins(*extension, WebUIOriginUse::kFrameNavigation),
              UnorderedElementsAreArray(TrustedNavigationOrigins()));
  EXPECT_THAT(Origins(*extension, WebUIOriginUse::kWorker),
              UnorderedElementsAre(OriginFor("chrome://resources/")));
}

TEST(WebUIOriginAccessTest, HostPermissionOptInsRequireTrust) {
  // --extensions-on-chrome-urls lets any extension declare chrome:// host
  // permissions, but declaring them doesn't grant access without trust.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      extensions_features::kDisableExtensionsOnChromeUrlsSwitch);
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kExtensionsOnChromeURLs);
  auto extension = BuildExtension(ManifestLocation::kInternal,
                                  {"chrome://favicon2/*", "chrome://image/*"});
  ASSERT_TRUE(extension->permissions_data()->HasHostPermission(
      GURL("chrome://favicon2/")));
  ASSERT_TRUE(extension->permissions_data()->HasHostPermission(
      GURL("chrome://image/")));
  EXPECT_THAT(Origins(*extension, WebUIOriginUse::kFrameSubresource),
              UnorderedElementsAreArray(ExtensionSubresourceOrigins()));
}

TEST(WebUIOriginAccessTest, ExternalComponentCannotDeclareOptIns) {
  // kExternalComponent extensions are trusted, but can't declare chrome://
  // host permissions (see CanSpecifyHostPermission()).
  auto extension = BuildExtension(ManifestLocation::kExternalComponent,
                                  {"chrome://favicon2/*", "chrome://image/*"});
  EXPECT_THAT(Origins(*extension, WebUIOriginUse::kFrameSubresource),
              UnorderedElementsAreArray(TrustedSubresourceOrigins()));
}

TEST(WebUIOriginAccessTest, UntrustedOriginsOnlyForNewTabPageExtension) {
  const url::Origin ntp = OriginFor("chrome-untrusted://new-tab-page/");
  const url::Origin ms_auth =
      OriginFor("chrome-untrusted://ntp-microsoft-auth/");

  auto ntp_extension =
      BuildExtension(ManifestLocation::kComponent, /*host_permissions=*/{},
                     extension_misc::kNewTabPageExtensionId);
  std::vector<url::Origin> expected = TrustedNavigationOrigins();
  expected.push_back(ntp);
  expected.push_back(ms_auth);
  EXPECT_THAT(Origins(*ntp_extension, WebUIOriginUse::kFrameNavigation),
              UnorderedElementsAreArray(expected));
  // They are navigation-only.
  for (WebUIOriginUse use : kAllUses) {
    if (use == WebUIOriginUse::kFrameNavigation) {
      continue;
    }
    EXPECT_THAT(Origins(*ntp_extension, use), Not(Contains(ntp)));
    EXPECT_THAT(Origins(*ntp_extension, use), Not(Contains(ms_auth)));
  }

  // Other trusted extensions don't get them.
  auto other_component = BuildExtension(ManifestLocation::kComponent);
  EXPECT_THAT(Origins(*other_component, WebUIOriginUse::kFrameNavigation),
              UnorderedElementsAreArray(TrustedNavigationOrigins()));

  // Untrusted extensions claiming the New Tab Page extension's ID (e.g. via
  // the manifest "key" of an unpacked extension) don't get them.
  for (ManifestLocation location :
       {ManifestLocation::kUnpacked, ManifestLocation::kCommandLine,
        ManifestLocation::kInternal}) {
    SCOPED_TRACE(static_cast<int>(location));
    auto spoofed = BuildExtension(location, /*host_permissions=*/{},
                                  extension_misc::kNewTabPageExtensionId);
    EXPECT_THAT(Origins(*spoofed, WebUIOriginUse::kFrameNavigation),
                UnorderedElementsAreArray(ExtensionNavigationOrigins()));
  }
}

TEST(WebUIOriginAccessTest, WebUITestRequiresSwitch) {
  const url::Origin webui_test = OriginFor("chrome://webui-test/");
  auto extension = BuildExtension(ManifestLocation::kComponent);
  EXPECT_THAT(Origins(*extension, WebUIOriginUse::kFrameSubresource),
              Not(Contains(webui_test)));

  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kAllowWebUITestInComponentExtensions);

  std::vector<url::Origin> expected = TrustedSubresourceOrigins();
  expected.push_back(webui_test);
  EXPECT_THAT(Origins(*extension, WebUIOriginUse::kFrameSubresource),
              UnorderedElementsAreArray(expected));
  EXPECT_THAT(Origins(*extension, WebUIOriginUse::kFrameNavigation),
              Not(Contains(webui_test)));
  EXPECT_THAT(
      Origins(*extension, WebUIOriginUse::kCorsAccess),
      UnorderedElementsAre(OriginFor("chrome://theme/"),
                           OriginFor("chrome://resources/"), webui_test));

  // The switch only applies to kComponent extensions.
  for (ManifestLocation location :
       {ManifestLocation::kExternalComponent, ManifestLocation::kUnpacked}) {
    SCOPED_TRACE(static_cast<int>(location));
    auto other = BuildExtension(location);
    EXPECT_THAT(Origins(*other, WebUIOriginUse::kFrameSubresource),
                Not(Contains(webui_test)));
    EXPECT_THAT(Origins(*other, WebUIOriginUse::kCorsAccess),
                Not(Contains(webui_test)));
  }
}

TEST(WebUIOriginAccessTest, HostedApps) {
  auto make_manifest = [] {
    return base::DictValue()
        .Set("name", "hosted app")
        .Set("version", "1")
        .Set("manifest_version", 2)
        .Set("app",
             base::DictValue()
                 .Set("urls", base::ListValue().Append("https://example.com/"))
                 .Set("launch", base::DictValue().Set("web_url",
                                                      "https://example.com/")));
  };
  auto hosted = BuildFromManifest(ManifestLocation::kInternal, make_manifest());
  ASSERT_TRUE(hosted->is_hosted_app());
  for (WebUIOriginUse use : kAllUses) {
    EXPECT_THAT(Origins(*hosted, use), IsEmpty());
  }

  // Component hosted apps only get CORS access.
  auto component_hosted =
      BuildFromManifest(ManifestLocation::kComponent, make_manifest());
  EXPECT_THAT(Origins(*component_hosted, WebUIOriginUse::kFrameSubresource),
              IsEmpty());
  EXPECT_THAT(Origins(*component_hosted, WebUIOriginUse::kFrameNavigation),
              IsEmpty());
  EXPECT_THAT(Origins(*component_hosted, WebUIOriginUse::kWorker), IsEmpty());
  EXPECT_THAT(Origins(*component_hosted, WebUIOriginUse::kCorsAccess),
              UnorderedElementsAre(OriginFor("chrome://theme/")));
}

TEST(WebUIOriginAccessTest, LegacyPackagedApps) {
  auto make_manifest = [] {
    return base::DictValue()
        .Set("name", "legacy packaged app")
        .Set("version", "1")
        .Set("manifest_version", 2)
        .Set("app",
             base::DictValue().Set(
                 "launch", base::DictValue().Set("local_path", "main.html")));
  };
  for (ManifestLocation location :
       {ManifestLocation::kInternal, ManifestLocation::kComponent}) {
    SCOPED_TRACE(static_cast<int>(location));
    auto app = BuildFromManifest(location, make_manifest());
    ASSERT_TRUE(app->is_legacy_packaged_app());
    // Legacy packaged apps get the icons, but no trusted WebUI origins in
    // frames or workers, even as component extensions.
    EXPECT_THAT(Origins(*app, WebUIOriginUse::kFrameSubresource),
                UnorderedElementsAreArray(ExtensionSubresourceOrigins()));
    EXPECT_THAT(Origins(*app, WebUIOriginUse::kFrameNavigation),
                UnorderedElementsAreArray(ExtensionNavigationOrigins()));
    EXPECT_THAT(Origins(*app, WebUIOriginUse::kWorker), IsEmpty());
    // CORS access applies to trusted extensions of any type.
    if (location == ManifestLocation::kComponent) {
      EXPECT_THAT(Origins(*app, WebUIOriginUse::kCorsAccess),
                  UnorderedElementsAre(OriginFor("chrome://theme/")));
    } else {
      EXPECT_THAT(Origins(*app, WebUIOriginUse::kCorsAccess), IsEmpty());
    }
  }
}

#if BUILDFLAG(ENABLE_PLATFORM_APPS)
TEST(WebUIOriginAccessTest, PlatformApps) {
  auto component_app =
      ExtensionBuilder("app", ExtensionBuilder::Type::PLATFORM_APP)
          .SetLocation(ManifestLocation::kComponent)
          .Build();
  ASSERT_TRUE(component_app->is_platform_app());
  EXPECT_THAT(Origins(*component_app, WebUIOriginUse::kFrameSubresource),
              UnorderedElementsAreArray(TrustedSubresourceOrigins()));
  EXPECT_THAT(Origins(*component_app, WebUIOriginUse::kFrameNavigation),
              UnorderedElementsAreArray(TrustedNavigationOrigins()));
  // Workers of platform apps don't get chrome://resources.
  EXPECT_THAT(Origins(*component_app, WebUIOriginUse::kWorker), IsEmpty());
  EXPECT_THAT(Origins(*component_app, WebUIOriginUse::kCorsAccess),
              UnorderedElementsAre(OriginFor("chrome://theme/")));

  // Non-component platform apps get nothing.
  auto app = ExtensionBuilder("app", ExtensionBuilder::Type::PLATFORM_APP)
                 .SetLocation(ManifestLocation::kInternal)
                 .Build();
  for (WebUIOriginUse use : kAllUses) {
    EXPECT_THAT(Origins(*app, use), IsEmpty());
  }
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_APPS)

}  // namespace extensions
