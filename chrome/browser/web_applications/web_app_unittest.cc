// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/model/display_override.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_paths.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_sha256_signature.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack_entry.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/liburlpattern/part.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

namespace {

constexpr std::string_view kGenerateExpectationsMessage = R"(
In order to regenerate expectations run
the following command:
  out/<dir>/unit_tests \
    --gtest_filter="WebAppTest.*" \
    --rebaseline-web-app-expectations
)";

base::Value WebAppToPlatformAgnosticDebugValue(
    std::unique_ptr<WebApp> web_app) {
  return web_app->AsDebugValueWithOnlyPlatformAgnosticFields();
}

base::FilePath GetTestDataDir() {
  base::FilePath test_data_dir;
  CHECK(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  return test_data_dir;
}

base::FilePath GetPathRelativeToTestDataDir(
    const base::FilePath absolute_path) {
  base::FilePath relative_path;
  GetTestDataDir().AppendRelativePath(absolute_path, &relative_path);
  return relative_path;
}

base::FilePath GetPathToTestFile(std::string_view filename) {
  return GetTestDataDir().AppendASCII("web_apps").AppendASCII(filename);
}

std::string GetContentsOrDie(const base::FilePath& filepath) {
  std::string contents;
  CHECK(base::ReadFileToString(filepath, &contents));
  return contents;
}

void SetContentsOrDie(const base::FilePath& filepath,
                      std::string_view contents) {
  CHECK(base::WriteFile(filepath, contents));
}

std::string SerializeValueToJsonOrDie(const base::Value& value) {
  std::string contents;
  CHECK(base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &contents));
  return contents;
}

base::Value DeserializeValueFromJsonOrDie(std::string_view json) {
  std::optional<base::Value> value =
      base::JSONReader::Read(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  CHECK(value.has_value());
  return *std::move(value);
}

bool IsRebaseline() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return command_line.HasSwitch("rebaseline-web-app-expectations");
}

void SaveExpectationsContentsOrDie(const base::FilePath path,
                                   std::string_view contents) {
  const std::string current_contents = GetContentsOrDie(path);

  const base::FilePath test_data_dir_relative_path =
      GetPathRelativeToTestDataDir(path);

  if (current_contents != contents) {
    LOG(INFO) << "New content is generated for " << test_data_dir_relative_path;
  } else {
    LOG(INFO) << "No new content is generated for "
              << test_data_dir_relative_path;
  }

  SetContentsOrDie(path, contents);
}

static constexpr char kEcdsaP256PublicKeyBase64[] =
    "AxyCBzvQfu1yaF01392k1gu2qCtT1uA2+WfIEhlyJB5S";
static constexpr char kEcdsaP256SHA256SignatureHex[] =
    "3044022007381524F538B04F99CCC62703F06C87F66EF41BDA18A22D8E57952AA23E53A6"
    "022063C7F81D3A44798CB95823FA38FC23B15E0483744657FF49E1E83AB8C06B63C2";

IsolatedWebAppIntegrityBlockData CreateIntegrityBlockData() {
  std::vector<web_package::SignedWebBundleSignatureInfo> signatures;

  // EcdsaP256SHA256:
  {
    auto public_key = *web_package::EcdsaP256PublicKey::Create(
        *base::Base64Decode(kEcdsaP256PublicKeyBase64));
    std::vector<uint8_t> data;
    CHECK(base::HexStringToBytes(kEcdsaP256SHA256SignatureHex, &data));
    auto signature = *web_package::EcdsaP256SHA256Signature::Create(data);
    signatures.push_back(
        web_package::SignedWebBundleSignatureInfoEcdsaP256SHA256(
            std::move(public_key), std::move(signature)));
  }

  return IsolatedWebAppIntegrityBlockData(std::move(signatures));
}

}  // namespace

TEST(WebAppTest, HasAnySources) {
  GURL start_url("https://example.com");
  WebApp app(GenerateManifestIdFromStartUrlOnly(start_url), start_url,
             start_url.GetWithoutFilename());

  EXPECT_FALSE(app.HasAnySources());
  for (WebAppManagement::Type source : WebAppManagementTypes::All()) {
    app.AddSource(source);
    EXPECT_TRUE(app.HasAnySources());
  }

  for (WebAppManagement::Type source : WebAppManagementTypes::All()) {
    EXPECT_TRUE(app.HasAnySources());
    app.RemoveSource(source);
  }
  EXPECT_FALSE(app.HasAnySources());
}

TEST(WebAppTest, HasOnlySource) {
  GURL start_url("https://example.com");
  WebApp app(GenerateManifestIdFromStartUrlOnly(start_url), start_url,
             start_url.GetWithoutFilename());

  for (WebAppManagement::Type source : WebAppManagementTypes::All()) {
    app.AddSource(source);
    EXPECT_TRUE(app.HasOnlySource(source));

    app.RemoveSource(source);
    EXPECT_FALSE(app.HasOnlySource(source));
  }

  app.AddSource(WebAppManagement::kMinValue);
  EXPECT_TRUE(app.HasOnlySource(WebAppManagement::kMinValue));

  for (WebAppManagement::Type source : WebAppManagementTypes::All()) {
    if (source == WebAppManagement::kMinValue) {
      continue;
    }
    app.AddSource(source);
    EXPECT_FALSE(app.HasOnlySource(source));
    EXPECT_FALSE(app.HasOnlySource(WebAppManagement::kMinValue));
  }

  for (WebAppManagement::Type source : WebAppManagementTypes::All()) {
    if (source == WebAppManagement::kMinValue) {
      continue;
    }
    EXPECT_FALSE(app.HasOnlySource(WebAppManagement::kMinValue));
    app.RemoveSource(source);
    EXPECT_FALSE(app.HasOnlySource(source));
  }

  EXPECT_TRUE(app.HasOnlySource(WebAppManagement::kMinValue));
  app.RemoveSource(WebAppManagement::kMinValue);
  EXPECT_FALSE(app.HasOnlySource(WebAppManagement::kMinValue));
  EXPECT_FALSE(app.HasAnySources());
}

TEST(WebAppTest, WasInstalledByUser) {
  GURL start_url("https://example.com");
  WebApp app(GenerateManifestIdFromStartUrlOnly(start_url), start_url,
             start_url.GetWithoutFilename());

  app.AddSource(WebAppManagement::kSync);
  EXPECT_TRUE(app.WasInstalledByUser());

  app.AddSource(WebAppManagement::kWebAppStore);
  EXPECT_TRUE(app.WasInstalledByUser());

  app.RemoveSource(WebAppManagement::kSync);
  EXPECT_TRUE(app.WasInstalledByUser());

  app.RemoveSource(WebAppManagement::kWebAppStore);
  EXPECT_FALSE(app.WasInstalledByUser());

  app.AddSource(WebAppManagement::kOneDriveIntegration);
  EXPECT_TRUE(app.WasInstalledByUser());

  app.RemoveSource(WebAppManagement::kOneDriveIntegration);
  EXPECT_FALSE(app.WasInstalledByUser());

  app.AddSource(WebAppManagement::kDefault);
  EXPECT_FALSE(app.WasInstalledByUser());

  app.AddSource(WebAppManagement::kSystem);
  EXPECT_FALSE(app.WasInstalledByUser());

  app.AddSource(WebAppManagement::kKiosk);
  EXPECT_FALSE(app.WasInstalledByUser());

  app.AddSource(WebAppManagement::kPolicy);
  EXPECT_FALSE(app.WasInstalledByUser());

  app.AddSource(WebAppManagement::kSubApp);
  EXPECT_FALSE(app.WasInstalledByUser());

  app.RemoveSource(WebAppManagement::kDefault);
  EXPECT_FALSE(app.WasInstalledByUser());

  app.RemoveSource(WebAppManagement::kSystem);
  EXPECT_FALSE(app.WasInstalledByUser());

  app.RemoveSource(WebAppManagement::kKiosk);
  EXPECT_FALSE(app.WasInstalledByUser());

  app.RemoveSource(WebAppManagement::kPolicy);
  EXPECT_FALSE(app.WasInstalledByUser());

  app.RemoveSource(WebAppManagement::kSubApp);
  EXPECT_FALSE(app.WasInstalledByUser());
}

TEST(WebAppTest, CanUserUninstallWebApp) {
  GURL start_url("https://example.com");
  WebApp app(GenerateManifestIdFromStartUrlOnly(start_url), start_url,
             start_url.GetWithoutFilename());

  app.AddSource(WebAppManagement::kDefault);
  EXPECT_TRUE(app.IsPreinstalledApp());
  EXPECT_TRUE(app.CanUserUninstallWebApp());

  app.AddSource(WebAppManagement::kSync);
  EXPECT_TRUE(app.CanUserUninstallWebApp());
  app.AddSource(WebAppManagement::kWebAppStore);
  EXPECT_TRUE(app.CanUserUninstallWebApp());
  app.AddSource(WebAppManagement::kSubApp);
  EXPECT_TRUE(app.CanUserUninstallWebApp());
  app.AddSource(WebAppManagement::kOneDriveIntegration);
  EXPECT_TRUE(app.CanUserUninstallWebApp());

  app.AddSource(WebAppManagement::kPolicy);
  EXPECT_FALSE(app.CanUserUninstallWebApp());
  app.AddSource(WebAppManagement::kKiosk);
  EXPECT_FALSE(app.CanUserUninstallWebApp());
  app.AddSource(WebAppManagement::kSystem);
  EXPECT_FALSE(app.CanUserUninstallWebApp());

  app.RemoveSource(WebAppManagement::kSync);
  EXPECT_FALSE(app.CanUserUninstallWebApp());
  app.RemoveSource(WebAppManagement::kWebAppStore);
  EXPECT_FALSE(app.CanUserUninstallWebApp());
  app.RemoveSource(WebAppManagement::kSubApp);
  EXPECT_FALSE(app.CanUserUninstallWebApp());

  app.RemoveSource(WebAppManagement::kSystem);
  EXPECT_FALSE(app.CanUserUninstallWebApp());

  app.RemoveSource(WebAppManagement::kKiosk);
  EXPECT_FALSE(app.CanUserUninstallWebApp());

  app.RemoveSource(WebAppManagement::kPolicy);
  EXPECT_TRUE(app.CanUserUninstallWebApp());

  app.RemoveSource(WebAppManagement::kOneDriveIntegration);
  EXPECT_TRUE(app.CanUserUninstallWebApp());

  EXPECT_TRUE(app.IsPreinstalledApp());
  app.RemoveSource(WebAppManagement::kDefault);
  EXPECT_FALSE(app.IsPreinstalledApp());
}

TEST(WebAppTest, EmptyAppAsDebugValue) {
  const base::FilePath path_to_test_file =
      GetPathToTestFile("empty_web_app.json");
  const base::Value web_app_debug_value =
      WebAppToPlatformAgnosticDebugValue(test::CreateWebApp());

  if (IsRebaseline()) {
    LOG(INFO) << "Generating expectations empty web app unit test in "
              << GetPathRelativeToTestDataDir(path_to_test_file);
    SaveExpectationsContentsOrDie(
        path_to_test_file, SerializeValueToJsonOrDie(web_app_debug_value));
    return;
  }

  EXPECT_EQ(DeserializeValueFromJsonOrDie(GetContentsOrDie(path_to_test_file)),
            web_app_debug_value)
      << "Debug value of empty web app is unexpected. "
      << kGenerateExpectationsMessage;
}

// The values of the SampleApp are randomly generated. This test is mainly
// checking that the output is formatted well and doesn't crash. Exact field
// values are unimportant.
//
// If you have made changes and this test is failing, run the test with
// `--rebaseline-web-app-expectations` to generate a new `sample_web_app.json`.
TEST(WebAppTest, SampleAppAsDebugValue) {
  const base::FilePath path_to_test_file =
      GetPathToTestFile("sample_web_app.json");
  test::CreateRandomWebAppParams params;
  params.seed = 1234;
  params.non_zero = true;
  const base::Value web_app_debug_value =
      WebAppToPlatformAgnosticDebugValue(test::CreateRandomWebApp(params));

  if (IsRebaseline()) {
    LOG(INFO) << "Generating expectations sample web app unit test in "
              << GetPathRelativeToTestDataDir(path_to_test_file);
    SaveExpectationsContentsOrDie(
        path_to_test_file, SerializeValueToJsonOrDie(web_app_debug_value));
    return;
  }

  EXPECT_EQ(DeserializeValueFromJsonOrDie(GetContentsOrDie(path_to_test_file)),
            web_app_debug_value)
      << "Debug value of sample web app is unexpected. "
      << kGenerateExpectationsMessage;
}

TEST(WebAppTest, RandomAppAsDebugValue_NoCrash) {
  for (uint32_t seed = 0; seed < 1000; ++seed) {
    test::CreateRandomWebAppParams params;
    params.seed = seed;
    const base::Value web_app_debug_value =
        test::CreateRandomWebApp(params)->AsDebugValue();

    EXPECT_TRUE(web_app_debug_value.is_dict());
    EXPECT_GT(base::ToString(web_app_debug_value).length(), 10ul);
  }
}

TEST(WebAppTest, IsolationDataStartsEmpty) {
  auto app = web_app::test::CreateWebApp(GURL("https://example.com"));

  EXPECT_FALSE(app->isolation_data().has_value());
}

TEST(WebAppTest, IsolationDataDebugValue) {
  GURL kStartUrl("isolated-app://random_name");
  WebApp app(GenerateManifestIdFromStartUrlOnly(kStartUrl),
             /*start_url=*/kStartUrl, /*scope=*/kStartUrl);
  app.SetIsolationData(
      IsolationData::Builder(
          IwaStorageOwnedBundle{"random_name", /*dev_mode=*/false},
          *IwaVersion::Create("1.0.0"))
          .Build());

  EXPECT_TRUE(app.isolation_data().has_value());

  base::Value expected_isolation_data =
      base::JSONReader::Read(R"|({
        "isolated_web_app_location": {
          "owned_bundle": {
            "dev_mode": false,
            "dir_name_ascii": "random_name"
          }
        },
        "version": "1.0.0",
        "controlled_frame_partitions (on-disk)": [],
        "opened_tabs_counter_notification_state": null,
        "pending_update_info": null,
        "integrity_block_data": null
      })|",
                             base::JSON_PARSE_CHROMIUM_EXTENSIONS)
          .value();

  base::DictValue debug_app = app.AsDebugValue().GetDict().Clone();
  base::DictValue* debug_isolation_data = debug_app.FindDict("isolation_data");
  EXPECT_TRUE(debug_isolation_data != nullptr);
  EXPECT_EQ(*debug_isolation_data, expected_isolation_data);
}

TEST(WebAppTest, IsolationDataPendingUpdateInfoDebugValue) {
  GURL kStartUrl("isolated-app://random_name");
  WebApp app(GenerateManifestIdFromStartUrlOnly(kStartUrl),
             /*start_url=*/kStartUrl, /*scope=*/kStartUrl);

  static constexpr std::string_view kUpdateManifestUrl =
      "https://update-manifest.com";
  static const UpdateChannel kUpdateChannel = UpdateChannel::default_channel();

  auto integrity_block_data = CreateIntegrityBlockData();
  app.SetIsolationData(
      IsolationData::Builder(
          IwaStorageOwnedBundle{"random_name", /*dev_mode=*/true},
          *IwaVersion::Create("1.0.0"))
          .SetPendingUpdateInfo(IsolationData::PendingUpdateInfo(
              IwaStorageUnownedBundle{
                  base::FilePath(FILE_PATH_LITERAL("random_folder"))},
              *IwaVersion::Create("2.0.0"), integrity_block_data))
          .SetIntegrityBlockData(integrity_block_data)
          .SetUpdateManifestUrl(GURL(kUpdateManifestUrl))
          .SetUpdateChannel(kUpdateChannel)
          .Build());

  EXPECT_TRUE(app.isolation_data().has_value());

  auto ib_data_serialized = *base::WriteJson(base::DictValue().Set(
      "signatures", base::ListValue().Append(base::DictValue().Set(
                        "ecdsa_p256_sha256",
                        base::DictValue()
                            .Set("public_key", kEcdsaP256PublicKeyBase64)
                            .Set("signature", kEcdsaP256SHA256SignatureHex)))));

  static constexpr std::string_view kExpectedIsolationDataFormat =
      R"|({
        "isolated_web_app_location": {
          "owned_bundle": {
            "dev_mode": true,
            "dir_name_ascii": "random_name"
          }
        },
        "version": "1.0.0",
        "controlled_frame_partitions (on-disk)": [],
        "opened_tabs_counter_notification_state": null,
        "pending_update_info": {
          "isolated_web_app_location": {
            "unowned_bundle": {
              "path": "random_folder"
            }
          },
          "version": "2.0.0",
          "integrity_block_data": $1
        },
        "integrity_block_data": $2,
        "update_manifest_url": "$3",
        "update_channel": "$4"
      })|";

  base::Value expected_isolation_data = *base::JSONReader::Read(
      base::ReplaceStringPlaceholders(
          kExpectedIsolationDataFormat,
          {ib_data_serialized, ib_data_serialized,
           GURL(kUpdateManifestUrl).spec(), kUpdateChannel.ToString()},
          /*offsets=*/nullptr),
      base::JSON_PARSE_CHROMIUM_EXTENSIONS);

  base::DictValue debug_app = app.AsDebugValue().GetDict().Clone();
  base::DictValue* debug_isolation_data = debug_app.FindDict("isolation_data");
  EXPECT_TRUE(debug_isolation_data != nullptr);
  EXPECT_EQ(*debug_isolation_data, expected_isolation_data);
}

TEST(WebAppTest, DisplayOverrideDebugValue) {
  GURL start_url("https://example.com");
  WebApp app(GenerateManifestIdFromStartUrlOnly(start_url), start_url,
             start_url.GetWithoutFilename());

  blink::SafeUrlPattern pattern;
  pattern.pathname = {liburlpattern::Part(
      liburlpattern::PartType::kFixed, "/foo", liburlpattern::Modifier::kNone)};

  app.SetDisplayModeOverride({DisplayOverride::Create(DisplayMode::kStandalone),
                              DisplayOverride::CreateUnframed({pattern})});

  base::Value debug_app = app.AsDebugValue();

  base::ListValue* debug_display_override =
      debug_app.GetDict().FindList("display_override");
  ASSERT_THAT(debug_display_override, testing::NotNull());
  EXPECT_THAT(*debug_display_override,
              testing::ElementsAre("standalone", base::test::IsJson(R"({
                "display": "unframed",
                "url_patterns": [{ "pathname": "/foo" }]
              })")));
}

}  // namespace web_app
