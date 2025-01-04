// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/win/com_classes_util.h"

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected_macros.h"
#include "base/version.h"
#include "base/win/windows_types.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/win_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater::test {

TEST(ComClassesUtil, ValidateStringEmptyNotOk) {
  ASSERT_FALSE(ValidateStringEmptyNotOk(nullptr, 10));
  ASSERT_FALSE(ValidateStringEmptyNotOk(L"", 10));
  ASSERT_FALSE(ValidateStringEmptyNotOk(L"morethan10characters", 10));
  ASSERT_EQ(ValidateStringEmptyNotOk(L"ninechars", 10).value(), "ninechars");
}

TEST(ComClassesUtil, ValidateStringEmptyOk) {
  ASSERT_EQ(ValidateStringEmptyOk(nullptr, 10).value(), "");
  ASSERT_EQ(ValidateStringEmptyOk(L"", 10).value(), "");
  ASSERT_FALSE(ValidateStringEmptyOk(L"morethan10characters", 10));
  ASSERT_EQ(ValidateStringEmptyOk(L"ninechars", 10).value(), "ninechars");
}

TEST(ComClassesUtil, ValidateAppId) {
  ASSERT_FALSE(ValidateAppId(nullptr));
  ASSERT_FALSE(ValidateAppId(L""));
  ASSERT_FALSE(ValidateAppId(std::wstring(0x4001, 'a').c_str()));
  ASSERT_EQ(ValidateAppId(L"appidisvalid").value(), "appidisvalid");
}

TEST(ComClassesUtil, ValidateCommandId) {
  ASSERT_FALSE(ValidateCommandId(nullptr));
  ASSERT_FALSE(ValidateCommandId(L""));
  ASSERT_FALSE(ValidateCommandId(std::wstring(0x4001, 'a').c_str()));
  ASSERT_EQ(ValidateCommandId(L"commandidisvalid").value(), "commandidisvalid");
}

TEST(ComClassesUtil, ValidateBrandCode) {
  ASSERT_EQ(ValidateBrandCode(nullptr).value(), "");
  ASSERT_EQ(ValidateBrandCode(L"").value(), "");
  ASSERT_FALSE(ValidateBrandCode(std::wstring(0x4001, 'a').c_str()));
  ASSERT_EQ(ValidateBrandCode(L"brandcodeisvalid").value(), "brandcodeisvalid");
}

TEST(ComClassesUtil, ValidateBrandPath) {
  ASSERT_TRUE(ValidateBrandPath(nullptr).value().empty());
  ASSERT_TRUE(ValidateBrandPath(L"").value().empty());
  ASSERT_FALSE(ValidateBrandPath(std::wstring(0x4001, 'a').c_str()));
  ASSERT_EQ(ValidateBrandPath(L"brandpathisvalid").value().value(),
            L"brandpathisvalid");
}

TEST(ComClassesUtil, ValidateAP) {
  ASSERT_EQ(ValidateAP(nullptr).value(), "");
  ASSERT_EQ(ValidateAP(L"").value(), "");
  ASSERT_FALSE(ValidateAP(std::wstring(0x4001, 'a').c_str()));
  ASSERT_EQ(ValidateAP(L"apisvalid").value(), "apisvalid");
}

TEST(ComClassesUtil, ValidateVersion) {
  ASSERT_FALSE(ValidateVersion(nullptr));
  ASSERT_FALSE(ValidateVersion(L""));
  ASSERT_FALSE(ValidateVersion(std::wstring(0x4001, 'a').c_str()));
  ASSERT_FALSE(ValidateVersion(L"invalidversion"));
  ASSERT_EQ(ValidateVersion(L"1.2.3.4").value(), base::Version("1.2.3.4"));
}

TEST(ComClassesUtil, ValidateExistenceCheckerPath) {
  ASSERT_TRUE(ValidateExistenceCheckerPath(nullptr).value().empty());
  ASSERT_TRUE(ValidateExistenceCheckerPath(L"").value().empty());
  ASSERT_FALSE(ValidateExistenceCheckerPath(std::wstring(0x4001, 'a').c_str()));
  ASSERT_EQ(
      ValidateExistenceCheckerPath(L"existencecheckerisvalid").value().value(),
      L"existencecheckerisvalid");
}

TEST(ComClassesUtil, ValidateInstallerPath) {
  ASSERT_FALSE(ValidateInstallerPath(nullptr));
  ASSERT_FALSE(ValidateInstallerPath(L""));
  ASSERT_FALSE(ValidateInstallerPath(std::wstring(0x4001, 'a').c_str()));
  ASSERT_EQ(ValidateInstallerPath(L"installerpathisvalid").value().value(),
            L"installerpathisvalid");
}

TEST(ComClassesUtil, ValidateInstallArgs) {
  ASSERT_EQ(ValidateInstallArgs(nullptr).value(), "");
  ASSERT_EQ(ValidateInstallArgs(L"").value(), "");
  ASSERT_FALSE(ValidateInstallArgs(std::wstring(0x4001, 'a').c_str()));
  ASSERT_EQ(ValidateInstallArgs(L"installargsisvalid").value(),
            "installargsisvalid");
}

TEST(ComClassesUtil, ValidateInstallSettings) {
  ASSERT_EQ(ValidateInstallSettings(nullptr).value(), "");
  ASSERT_EQ(ValidateInstallSettings(L"").value(), "");
  ASSERT_FALSE(ValidateInstallSettings(std::wstring(0x4001, 'a').c_str()));
  ASSERT_EQ(ValidateInstallSettings(L"installsettingsisvalid").value(),
            "installsettingsisvalid");
}

TEST(ComClassesUtil, ValidateClientInstallData) {
  ASSERT_EQ(ValidateClientInstallData(nullptr).value(), "");
  ASSERT_EQ(ValidateClientInstallData(L"").value(), "");
  ASSERT_FALSE(ValidateClientInstallData(std::wstring(0x4001, 'a').c_str()));
  ASSERT_EQ(ValidateClientInstallData(L"clientinstalldataisvalid").value(),
            "clientinstalldataisvalid");
}

TEST(ComClassesUtil, ValidateInstallDataIndex) {
  ASSERT_EQ(ValidateInstallDataIndex(nullptr).value(), "");
  ASSERT_EQ(ValidateInstallDataIndex(L"").value(), "");
  ASSERT_FALSE(ValidateInstallDataIndex(std::wstring(0x4001, 'a').c_str()));
  ASSERT_EQ(ValidateInstallDataIndex(L"installdataindexisvalid").value(),
            "installdataindexisvalid");
}

TEST(ComClassesUtil, ValidateInstallId) {
  ASSERT_EQ(ValidateInstallId(nullptr).value(), "");
  ASSERT_EQ(ValidateInstallId(L"").value(), "");
  ASSERT_FALSE(ValidateInstallId(std::wstring(0x4001, 'a').c_str()));
  ASSERT_EQ(ValidateInstallId(L"installidisvalid").value(), "installidisvalid");
}

TEST(ComClassesUtil, ValidateLanguage) {
  ASSERT_EQ(ValidateLanguage(nullptr).value(), "");
  ASSERT_EQ(ValidateLanguage(L"").value(), "");
  ASSERT_FALSE(ValidateLanguage(std::wstring(11, 'a').c_str()));
  ASSERT_EQ(ValidateLanguage(L"langvalid").value(), "langvalid");
}

TEST(ComClassesUtil, ValidateRegistrationRequest) {
  ASSERT_FALSE(ValidateRegistrationRequest(nullptr, nullptr, nullptr, nullptr,
                                           nullptr, nullptr, nullptr));
  ASSERT_FALSE(ValidateRegistrationRequest(L"", L"", L"", L"", L"", L"", L""));
  ASSERT_FALSE(ValidateRegistrationRequest(std::wstring(0x4001, 'a').c_str(),
                                           L"", L"", L"", L"", L"", L""));
  ASSERT_FALSE(ValidateRegistrationRequest(
      L"app_id", L"brand_code", L"brand_path", L"ap", L"invalidversion",
      L"existence_checker_path", L"install_id"));

  std::optional<RegistrationRequest> request = ValidateRegistrationRequest(
      L"app_id", L"brand_code", L"brand_path", L"ap", L"1.2.3.4",
      L"existence_checker_path", L"install_id");
  ASSERT_TRUE(request);

  RegistrationRequest expected_request;
  expected_request.app_id = "app_id";
  expected_request.brand_code = "brand_code";
  expected_request.brand_path = base::FilePath(L"brand_path");
  expected_request.ap = "ap";
  expected_request.version = base::Version("1.2.3.4");
  expected_request.existence_checker_path =
      base::FilePath(L"existence_checker_path");
  expected_request.install_id = "install_id";

  ASSERT_EQ(request->app_id, expected_request.app_id);
  ASSERT_EQ(request->brand_code, expected_request.brand_code);
  ASSERT_EQ(request->brand_path, expected_request.brand_path);
  ASSERT_EQ(request->ap, expected_request.ap);
  ASSERT_EQ(request->ap_path, expected_request.ap_path);
  ASSERT_EQ(request->ap_key, expected_request.ap_key);
  ASSERT_EQ(request->version, expected_request.version);
  ASSERT_EQ(request->version_path, expected_request.version_path);
  ASSERT_EQ(request->version_key, expected_request.version_key);
  ASSERT_EQ(request->existence_checker_path,
            expected_request.existence_checker_path);
}

}  // namespace updater::test
