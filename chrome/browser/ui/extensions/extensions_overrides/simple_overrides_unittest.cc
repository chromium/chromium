// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extensions_overrides/simple_overrides.h"

#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "chrome/common/extensions/api/chrome_url_overrides.h"
#include "chrome/common/extensions/api/omnibox.h"
#include "chrome/common/extensions/api/side_panel.h"
#include "extensions/common/api/content_scripts.h"
#include "extensions/common/api/cross_origin_isolation.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/incognito.h"
#include "extensions/common/api/oauth2.h"
#include "extensions/common/api/requirements.h"
#include "extensions/common/api/shared_module.h"
#include "extensions/common/api/web_accessible_resources.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace simple_overrides {

namespace {

// If any of the following are present, the extension is not considered a
// "simple override". The union of this set and the allowed features should
// encompass every feature. This ensures that when developers add a new
// feature, they consider whether it should be allowed for "simple override"
// extensions.
const char* kDisallowedFeatures[] = {
    // Manifest constants.
    extensions::manifest_keys::kAction,
    extensions::manifest_keys::kApp,
    extensions::manifest_keys::kAutomation,
    extensions::manifest_keys::kBackground,
    extensions::manifest_keys::kBackgroundPage,
    extensions::manifest_keys::kBackgroundPersistent,
    extensions::manifest_keys::kBackgroundScripts,
    extensions::manifest_keys::kBackgroundServiceWorkerScript,
    extensions::manifest_keys::kBluetooth,
    extensions::manifest_keys::kBrowserAction,
    extensions::manifest_keys::kCommands,
    extensions::manifest_keys::kContentCapabilities,
    extensions::manifest_keys::kContentSecurityPolicy,
    extensions::manifest_keys::kConvertedFromUserScript,
    extensions::manifest_keys::kDevToolsPage,
    extensions::manifest_keys::kDisplayInLauncher,
    extensions::manifest_keys::kDisplayInNewTabPage,
    extensions::manifest_keys::kEventRules,
    extensions::manifest_keys::kExternallyConnectable,
    extensions::manifest_keys::kFileBrowserHandlers,
    extensions::manifest_keys::kFileHandlers,
    extensions::manifest_keys::kHostPermissions,
    extensions::manifest_keys::kInputComponents,
    extensions::manifest_keys::kKiosk,
    extensions::manifest_keys::kKioskAlwaysUpdate,
    extensions::manifest_keys::kKioskEnabled,
    extensions::manifest_keys::kKioskOnly,
    extensions::manifest_keys::kKioskRequiredPlatformVersion,
    extensions::manifest_keys::kKioskSecondaryApps,
    extensions::manifest_keys::kLaunch,
    extensions::manifest_keys::kLinkedAppIcons,
    extensions::manifest_keys::kMIMETypes,
    extensions::manifest_keys::kMimeTypesHandler,
    extensions::manifest_keys::kNaClModules,
    extensions::manifest_keys::kNativelyConnectable,
    extensions::manifest_keys::kOptionalHostPermissions,
    extensions::manifest_keys::kOptionalPermissions,
    extensions::manifest_keys::kPageAction,
    extensions::manifest_keys::kPermissions,
    extensions::manifest_keys::kPlatformAppBackground,
    extensions::manifest_keys::kPlatformAppContentSecurityPolicy,
    extensions::manifest_keys::kReplacementWebApp,
    extensions::manifest_keys::kSockets,
    extensions::manifest_keys::kSystemIndicator,
    extensions::manifest_keys::kTheme,
    extensions::manifest_keys::kTrialTokens,
    extensions::manifest_keys::kTtsEngine,
    extensions::manifest_keys::kUrlHandlers,
    extensions::manifest_keys::kUsbPrinters,
    extensions::manifest_keys::kWebview,

    // Compiled manifest keys.
    extensions::api::chrome_url_overrides::ManifestKeys::kChromeUrlOverrides,
    extensions::api::content_scripts::ManifestKeys::kContentScripts,
    extensions::api::cross_origin_isolation::ManifestKeys::
        kCrossOriginEmbedderPolicy,
    extensions::api::cross_origin_isolation::ManifestKeys::
        kCrossOriginOpenerPolicy,
    extensions::api::declarative_net_request::ManifestKeys::
        kDeclarativeNetRequest,
    extensions::api::oauth2::ManifestKeys::kOauth2,
    extensions::api::omnibox::ManifestKeys::kOmnibox,
    extensions::api::requirements::ManifestKeys::kRequirements,
    extensions::api::shared_module::ManifestKeys::kExport,
    extensions::api::shared_module::ManifestKeys::kImport,
    extensions::api::side_panel::ManifestKeys::kSidePanel,
    extensions::api::web_accessible_resources::ManifestKeys::
        kWebAccessibleResources,

    // The following features are only available on ChromeOS. However, the
    // entries in manifest_features are compiled for every OS (so that we can
    // populate availability messages if an extension requests them), so we
    // need to ensure they are included in this list.
    "action_handlers",
    "file_system_provider_capabilities",

// Unlike the keys above, chromeos_system_extension *is* only defined on
// ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
    extensions::manifest_keys::kChromeOSSystemExtension,
#endif

    // The following features have no declared constant, but are present in
    // the manifest_features file (they may be used only in a single other file,
    // and thus not exposed in a .h).
    "chrome_url_overrides.activationmessage",
    "chrome_url_overrides.keyboard",
    "oauth2.auto_approve",
    "platforms",
    "sandbox",
    "storage",
};

}  // namespace

// Ensures that all features are either specified in the allowed or disallowed
// feature lists (so that new features are guaranteed to be evaluated).
TEST(ExtensionSimpleOverridesTest,
     AllFeaturesAreAllowlistedOrInDisallowedFeatures) {
  const extensions::FeatureProvider* manifest_features =
      extensions::FeatureProvider::GetManifestFeatures();
  ASSERT_TRUE(manifest_features);

  const extensions::FeatureMap& known_features =
      manifest_features->GetAllFeatures();

  std::vector<std::string> allowlisted_features =
      simple_overrides::GetAllowlistedManifestKeysForTesting();

  std::vector<std::string> disallowed_features(std::begin(kDisallowedFeatures),
                                               std::end(kDisallowedFeatures));

  // Verify that all disallowed features are recognized and that none are in
  // both the disallowed and allowed feature sets.
  for (const auto& feature : disallowed_features) {
    EXPECT_TRUE(base::Contains(known_features, feature))
        << "Unknown feature: " << feature;
    EXPECT_FALSE(base::Contains(allowlisted_features, feature))
        << "Feature in both allowed and disallowed: " << feature;
  }

  // Verify that all allowed features are recognized and that none are in
  // both the disallowed and allowed feature sets.
  for (const auto& feature : allowlisted_features) {
    EXPECT_TRUE(base::Contains(known_features, feature))
        << "Unknown feature: " << feature;
    EXPECT_FALSE(base::Contains(disallowed_features, feature))
        << "Feature in both allowed and disallowed: " << feature;
  }

  // Verify that all known features are in either the allowed or disallowed
  // set.
  for (const auto& [key, value] : known_features) {
    // Is this expectation failing for you?
    // Review the comment in simple_overrides.cc above the allowed feature
    // list and evaluate whether your new feature belongs in the allowed or
    // disallowed features list (it should likely be disallowed).
    EXPECT_TRUE(base::Contains(disallowed_features, key) ||
                base::Contains(allowlisted_features, key))
        << "Unknown feature: " << key;
  }
}

// Verifies that an extension that contains only allowlisted features is
// considered a simple override.
TEST(ExtensionSimpleOverridesTest,
     ExtensionWithOnlyAllowlistedFeaturesIsConsideredSimple) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("alpha")
          .SetManifestKey("description", "a description")
          .SetManifestKey("author", "some author")
          .SetManifestKey("incognito", "split")
          .SetManifestKey("short_name", "a")
          .Build();
  EXPECT_TRUE(simple_overrides::IsSimpleOverrideExtension(*extension));
}

// Verifies that an extension that contains any non-allowlisted features is
// not considered a simple override.
TEST(ExtensionSimpleOverridesTest,
     ExtensionWithPermissionsIsNotConsideredSimple) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("alpha").AddAPIPermission("tabs").Build();
  EXPECT_FALSE(simple_overrides::IsSimpleOverrideExtension(*extension));
}

// Verifies that an extension that contains unrecognized manifest keys *is*
// considered a simple override.  This allows extensions to have non-chrome
// keys (like "minimum_browser_version").
TEST(ExtensionSimpleOverridesTest,
     ExtensionWithUnrecognizedKeysIsConsideredSimple) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("alpha")
          .SetManifestKey("unknown_key", "unknown_value")
          .Build();
  EXPECT_FALSE(simple_overrides::IsSimpleOverrideExtension(*extension));
}

}  // namespace simple_overrides
