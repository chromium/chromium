// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chromeos/ash/experiences/extensions/common/telemetry/chromeos_system_extension_info.h"
#include "chromeos/ash/experiences/extensions/common/telemetry/chromeos_system_extensions_api_provider.h"
#include "chromeos/ash/experiences/extensions/common/telemetry/chromeos_system_extensions_manifest_constants.h"
#include "components/version_info/channel.h"
#include "components/webapps/isolated_web_apps/scheme.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_test.h"
#include "extensions/common/scoped_testing_manifest_handler_registry.h"
#include "extensions/test/test_extensions_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"

namespace chromeos {
namespace {

class ExtensionManifestChromeOSSystemExtensionTest
    : public extensions::ManifestTest {
 public:
  ExtensionManifestChromeOSSystemExtensionTest()
      : current_channel_(version_info::Channel::UNKNOWN) {
    url::AddStandardScheme(extensions::kExtensionScheme, url::SCHEME_WITH_HOST);
    url::AddStandardScheme(webapps::kIsolatedAppScheme, url::SCHEME_WITH_HOST);
  }

  // `ExtensionsClient::Set()` initializes the process-wide `PermissionsInfo`
  // singleton, which `DCHECK`s that permissions are not registered more than
  // once and cannot be reset between individual tests. Therefore, set it up
  // once per test suite rather than in `SetUp()`/`TearDown()`.
  // Bare pointers and `new`/`delete` are used for the static members to avoid
  // static initializers and exit-time destructors.
  static void SetUpTestSuite() {
    ManifestTest::SetUpTestSuite();
    scoped_manifest_handler_registry_ =
        new extensions::ScopedTestingManifestHandlerRegistry();
    extensions_client_ = new extensions::TestExtensionsClient();
    extensions_client_->AddAPIProvider(
        std::make_unique<ChromeOSSystemExtensionsAPIProvider>());
    extensions::ExtensionsClient::Set(extensions_client_);
  }

  static void TearDownTestSuite() {
    extensions::ExtensionsClient::Set(nullptr);
    delete extensions_client_;
    extensions_client_ = nullptr;
    delete scoped_manifest_handler_registry_;
    scoped_manifest_handler_registry_ = nullptr;
    ManifestTest::TearDownTestSuite();
  }

 protected:
  base::FilePath GetTestDataDir() override {
    base::FilePath path;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path);
    return path.Append(FILE_PATH_LITERAL("chromeos"))
        .Append(FILE_PATH_LITERAL("ash"))
        .Append(FILE_PATH_LITERAL("experiences"))
        .Append(FILE_PATH_LITERAL("extensions"))
        .Append(FILE_PATH_LITERAL("test"))
        .Append(FILE_PATH_LITERAL("data"));
  }

 private:
  url::ScopedSchemeRegistryForTests scoped_scheme_registry_;
  extensions::ScopedCurrentChannel current_channel_;
  static extensions::ScopedTestingManifestHandlerRegistry*
      scoped_manifest_handler_registry_;
  static extensions::TestExtensionsClient* extensions_client_;
};

extensions::ScopedTestingManifestHandlerRegistry*
    ExtensionManifestChromeOSSystemExtensionTest::
        scoped_manifest_handler_registry_ = nullptr;
extensions::TestExtensionsClient*
    ExtensionManifestChromeOSSystemExtensionTest::extensions_client_ = nullptr;

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidChromeOSSystemExtension) {
  LoadAndExpectWarning("chromeos_system_extension_invalid.json",
                       kInvalidChromeOSSystemExtensionId);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       ChromeOSSystemExtensionDevIsDisabled) {
  // This is handled by manifest handler, so it is error, not warning.
  LoadAndExpectError("chromeos_system_extension_google_dev.json",
                     kInvalidChromeOSSystemExtensionId);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       ValidChromeOSSystemExtension_Allowlisted_Google) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("chromeos_system_extension_google.json"));
  EXPECT_TRUE(extension->is_chromeos_system_extension());
  EXPECT_TRUE(extension->install_warnings().empty());
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       ValidChromeOSSystemExtension_Allowlisted_HP) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("chromeos_system_extension_hp.json"));
  EXPECT_TRUE(extension->is_chromeos_system_extension());
  EXPECT_TRUE(extension->install_warnings().empty());
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       ValidChromeOSSystemExtension_Allowlisted_ASUS) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("chromeos_system_extension_asus.json"));
  EXPECT_TRUE(extension->is_chromeos_system_extension());
  EXPECT_TRUE(extension->install_warnings().empty());
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       ValidChromeOSSystemExtension_Allowlisted_Acer) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("chromeos_system_extension_acer.json"));
  EXPECT_TRUE(extension->is_chromeos_system_extension());
  EXPECT_TRUE(extension->install_warnings().empty());
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       ValidChromeOSSystemExtension_Allowlisted_Lenovo) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("chromeos_system_extension_lenovo.json"));
  EXPECT_TRUE(extension->is_chromeos_system_extension());
  EXPECT_TRUE(extension->install_warnings().empty());
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       ValidChromeOSSystemExtension_Allowlisted_ASUS_CTP) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("chromeos_system_extension_asus_ctp.json"));
  EXPECT_TRUE(extension->is_chromeos_system_extension());
  EXPECT_TRUE(extension->install_warnings().empty());
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       ValidNonChromeOSSystemExtension) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess(ManifestData::FromJSON(R"({
        "name": "test",
        "manifest_version": 2,
        "version": "1",
        "background": {
          "page": "foo.html"
        }
      })")));
  EXPECT_FALSE(extension->is_chromeos_system_extension());
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidExternallyConnectableEmpty) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_empty.json",
      chromeos::kInvalidExternallyConnectableDeclaration);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidExternallyConnectableIds) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_ids.json",
      chromeos::kInvalidExternallyConnectableDeclaration);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidExternallyConnectableTls) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_tls.json",
      chromeos::kInvalidExternallyConnectableDeclaration);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidExternallyConnectableMatchesMoreThanOne) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_2_origins.json",
      chromeos::kInvalidExternallyConnectableDeclaration);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidExternallyConnectableMatchesEmpty) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_matches_empty."
      "json",
      chromeos::kInvalidExternallyConnectableDeclaration);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidExternallyConnectableMatchesDisallowedOrigin) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_1_origin.json",
      chromeos::kInvalidExternallyConnectableDeclaration);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidExternallyConnectableNotExist) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_not_exist.json",
      chromeos::kInvalidExternallyConnectableDeclaration);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       ValidChromeOSSystemExtension_Allowlisted_Google_IWA) {
  auto scoped_info =
      chromeos::ScopedChromeOSSystemExtensionInfo::CreateForTesting();
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kTelemetryExtensionIwaIdOverrideForTesting,
      "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic");
  scoped_info->ApplyCommandLineSwitchesForTesting();

  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("chromeos_system_extension_google_iwa.json"));
  EXPECT_TRUE(extension->is_chromeos_system_extension());
  EXPECT_TRUE(extension->install_warnings().empty());
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       ChromeOSSystemExtensionDevIsEnabled) {
  auto scoped_info =
      chromeos::ScopedChromeOSSystemExtensionInfo::CreateForTesting();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureStates({
      {ash::features::kShimlessRMA3pDiagnosticsDevMode, true},
  });
  scoped_info->ApplyCommandLineSwitchesForTesting();

  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("chromeos_system_extension_google_dev.json"));
  EXPECT_TRUE(extension->is_chromeos_system_extension());
  EXPECT_TRUE(extension->install_warnings().empty());
}

}  // namespace
}  // namespace chromeos
