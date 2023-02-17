// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_piece_forward.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_declaration.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

namespace {

constexpr base::StringPiece kGenerateExpectationsMessage = R"(
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

base::FilePath GetPathToTestFile(base::StringPiece filename) {
  return GetTestDataDir().AppendASCII("web_apps").AppendASCII(filename);
}

std::string GetContentsOrDie(const base::FilePath& filepath) {
  std::string contents;
  CHECK(base::ReadFileToString(filepath, &contents));
  return contents;
}

void SetContentsOrDie(const base::FilePath& filepath,
                      base::StringPiece contents) {
  CHECK(base::WriteFile(filepath, contents));
}

std::string SerializeValueToJsonOrDie(const base::Value& value) {
  std::string contents;
  CHECK(base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::Options::OPTIONS_PRETTY_PRINT, &contents));
  return contents;
}

base::Value DeserializeValueFromJsonOrDie(base::StringPiece json) {
  absl::optional<base::Value> value = base::JSONReader::Read(json);
  CHECK(value.has_value());
  return *std::move(value);
}

bool IsRebaseline() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return command_line.HasSwitch("rebaseline-web-app-expectations");
}

void SaveExpectationsContentsOrDie(const base::FilePath path,
                                   base::StringPiece contents) {
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

}  // namespace

TEST(WebAppTest, HasAnySources) {
  WebApp app{GenerateAppId(/*manifest_id=*/absl::nullopt,
                           GURL("https://example.com"))};

  EXPECT_FALSE(app.HasAnySources());
  for (int i = WebAppManagement::kMinValue; i <= WebAppManagement::kMaxValue;
       ++i) {
    app.AddSource(static_cast<WebAppManagement::Type>(i));
    EXPECT_TRUE(app.HasAnySources());
  }

  for (int i = WebAppManagement::kMinValue; i <= WebAppManagement::kMaxValue;
       ++i) {
    EXPECT_TRUE(app.HasAnySources());
    app.RemoveSource(static_cast<WebAppManagement::Type>(i));
  }
  EXPECT_FALSE(app.HasAnySources());
}

TEST(WebAppTest, HasOnlySource) {
  WebApp app{GenerateAppId(/*manifest_id=*/absl::nullopt,
                           GURL("https://example.com"))};

  for (int i = WebAppManagement::kMinValue; i <= WebAppManagement::kMaxValue;
       ++i) {
    auto source = static_cast<WebAppManagement::Type>(i);

    app.AddSource(source);
    EXPECT_TRUE(app.HasOnlySource(source));

    app.RemoveSource(source);
    EXPECT_FALSE(app.HasOnlySource(source));
  }

  app.AddSource(WebAppManagement::kMinValue);
  EXPECT_TRUE(app.HasOnlySource(WebAppManagement::kMinValue));

  for (int i = WebAppManagement::kMinValue + 1;
       i <= WebAppManagement::kMaxValue; ++i) {
    auto source = static_cast<WebAppManagement::Type>(i);
    app.AddSource(source);
    EXPECT_FALSE(app.HasOnlySource(source));
    EXPECT_FALSE(app.HasOnlySource(WebAppManagement::kMinValue));
  }

  for (int i = WebAppManagement::kMinValue + 1;
       i <= WebAppManagement::kMaxValue; ++i) {
    auto source = static_cast<WebAppManagement::Type>(i);
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
  WebApp app{GenerateAppId(/*manifest_id=*/absl::nullopt,
                           GURL("https://example.com"))};

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
  WebApp app{GenerateAppId(/*manifest_id=*/absl::nullopt,
                           GURL("https://example.com"))};

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
      WebAppToPlatformAgnosticDebugValue(std::make_unique<WebApp>("empty_app"));

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
  const base::Value web_app_debug_value = WebAppToPlatformAgnosticDebugValue(
      test::CreateRandomWebApp(GURL("https://example.com/"),
                               /*seed=*/1234));

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

TEST(WebAppTest, IsolationDataStartsEmpty) {
  WebApp app{GenerateAppId(/*manifest_id=*/absl::nullopt,
                           GURL("https://example.com"))};

  EXPECT_FALSE(app.isolation_data().has_value());
}

TEST(WebAppTest, IsolationDataDebugValue) {
  WebApp app{GenerateAppId(/*manifest_id=*/absl::nullopt,
                           GURL("https://example.com"))};
  app.SetIsolationData(WebApp::IsolationData(InstalledBundle{
      .path = base::FilePath(FILE_PATH_LITERAL("random_path"))}));

  EXPECT_TRUE(app.isolation_data().has_value());

  base::Value expected_isolation_data = base::JSONReader::Read(R"({
        "isolated_web_app_location": {
          "installed_bundle": {
            "path": "random_path"
          }
        }
      })")
                                            .value();

  base::Value::Dict debug_app = app.AsDebugValue().GetDict().Clone();
  base::Value::Dict* debug_isolation_data =
      debug_app.FindDict("isolation_data");
  EXPECT_TRUE(debug_isolation_data != nullptr);
  EXPECT_EQ(*debug_isolation_data, expected_isolation_data);
}

TEST(WebAppTest, PermissionsPolicyDebugValue) {
  WebApp app{GenerateAppId(/*manifest_id=*/absl::nullopt,
                           GURL("https://example.com"))};
  app.SetPermissionsPolicy({
      {blink::mojom::PermissionsPolicyFeature::kGyroscope,
       {},
       /*matches_all_origins=*/false,
       /*matches_opaque_src=*/true},
      {blink::mojom::PermissionsPolicyFeature::kGeolocation,
       {},
       /*matches_all_origins=*/true,
       /*matches_opaque_src=*/false},
      {blink::mojom::PermissionsPolicyFeature::kGamepad,
       {{url::Origin::Create(GURL("https://example.com")),
         /*has_subdomain_wildcard=*/false},
        {url::Origin::Create(GURL("https://example.net")),
         /*has_subdomain_wildcard=*/true},
        {url::Origin::Create(GURL("https://*.example.net")),
         /*has_subdomain_wildcard=*/false}},
       /*matches_all_origins=*/false,
       /*matches_opaque_src=*/false},
  });

  EXPECT_TRUE(!app.permissions_policy().empty());

  base::Value expected_permissions_policy = base::JSONReader::Read(R"([
        {
          "allowed_origins": [  ],
          "feature": "gyroscope",
          "matches_all_origins": false,
          "matches_opaque_src": true
        }
        , {
          "allowed_origins": [  ],
          "feature": "geolocation",
          "matches_all_origins": true,
          "matches_opaque_src": false
        }
        , {
          "allowed_origins": [ "https://example.com", "https://*.example.net", "https://%2A.example.net" ],
          "feature": "gamepad",
          "matches_all_origins": false,
          "matches_opaque_src": false
        }
      ])")
                                                .value();

  base::Value::Dict debug_app = app.AsDebugValue().GetDict().Clone();
  base::Value::List* debug_permissions_policy =
      debug_app.FindList("permissions_policy");
  EXPECT_TRUE(debug_permissions_policy != nullptr);
  EXPECT_EQ(*debug_permissions_policy, expected_permissions_policy);
}
}  // namespace web_app
