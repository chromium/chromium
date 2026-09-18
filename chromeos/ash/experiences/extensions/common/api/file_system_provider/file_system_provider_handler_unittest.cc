// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chromeos/ash/experiences/extensions/common/chromeos_extensions_api_provider.h"
#include "components/version_info/channel.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_test.h"
#include "extensions/common/scoped_testing_manifest_handler_registry.h"
#include "extensions/test/test_extensions_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class FileSystemProviderHandlerTest : public ManifestTest {
 public:
  FileSystemProviderHandlerTest()
      : current_channel_(version_info::Channel::UNKNOWN) {}

  static void SetUpTestSuite() {
    ManifestTest::SetUpTestSuite();
    scoped_manifest_handler_registry_ =
        new ScopedTestingManifestHandlerRegistry();
    extensions_client_ = new TestExtensionsClient();
    extensions_client_->AddAPIProvider(
        std::make_unique<ash::ChromeOSExtensionsAPIProvider>());
    ExtensionsClient::Set(extensions_client_);
  }

  static void TearDownTestSuite() {
    ExtensionsClient::Set(nullptr);
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
  ScopedCurrentChannel current_channel_;
  static ScopedTestingManifestHandlerRegistry*
      scoped_manifest_handler_registry_;
  static TestExtensionsClient* extensions_client_;
};

ScopedTestingManifestHandlerRegistry*
    FileSystemProviderHandlerTest::scoped_manifest_handler_registry_ = nullptr;
TestExtensionsClient* FileSystemProviderHandlerTest::extensions_client_ =
    nullptr;

TEST_F(FileSystemProviderHandlerTest, Valid) {
  RunTestcase(Testcase("filesystemprovider_valid.json"), ExpectType::kSuccess);
}

TEST_F(FileSystemProviderHandlerTest, Invalid_MissingCapabilities) {
  RunTestcase(
      Testcase("filesystemprovider_missing_capabilities.json",
               manifest_errors::kInvalidFileSystemProviderMissingCapabilities),
      ExpectType::kError);
}

TEST_F(FileSystemProviderHandlerTest, Invalid_MissingPermission) {
  RunTestcase(
      Testcase("filesystemprovider_missing_permission.json",
               manifest_errors::kInvalidFileSystemProviderMissingPermission),
      ExpectType::kWarning);
}

}  // namespace extensions
