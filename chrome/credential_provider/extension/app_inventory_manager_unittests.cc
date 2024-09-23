// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/extension/app_inventory_manager.h"

#include <windows.h>

#include <memory>

#include "base/base_paths_win.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "chrome/credential_provider/extension/user_device_context.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/test/gls_runner_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

namespace testing {

class AppInventoryManagerBaseTest : public GlsRunnerTestBase {
 protected:
  void SetUp() override;
  std::wstring CreateUser();
};

void AppInventoryManagerBaseTest::SetUp() {
  GlsRunnerTestBase::SetUp();

  FakesForTesting fakes;
  fakes.fake_win_http_url_fetcher_creator =
      fake_http_url_fetcher_factory()->GetCreatorCallback();
  AppInventoryManager::Get()->SetFakesForTesting(&fakes);
}

std::wstring AppInventoryManagerBaseTest::CreateUser() {
  // Create a fake user associated to a gaia id.
  CComBSTR sid_str;
  EXPECT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      kDefaultUsername, L"password", L"Full Name", L"comment",
                      base::UTF8ToWide(kDefaultGaiaId), L"user@company.com",
                      &sid_str));
  return OLE2W(sid_str);
}

// Tests user policy fetch by ESA service.
// Params:
// string : The specified device resource ID.
// bool : Whether a valid user sid is present.
// bool : Whether app data is present or not.
// string : The specified DM token.
class AppInventoryManagerTest
    : public AppInventoryManagerBaseTest,
      public ::testing::WithParamInterface<
          std::tuple<const wchar_t*, bool, bool, const wchar_t*>> {
 public:
  AppInventoryManagerTest();

 protected:
  extension::TaskCreator app_inventory_task_creator_;
};

AppInventoryManagerTest::AppInventoryManagerTest() {
  app_inventory_task_creator_ =
      AppInventoryManager::UploadAppInventoryTaskCreator();
}

TEST_P(AppInventoryManagerTest, uploadAppInventory) {
  const std::wstring device_resource_id(std::get<0>(GetParam()));
  bool has_valid_sid = std::get<1>(GetParam());
  bool has_app_data = std::get<2>(GetParam());
  const std::wstring dm_token(std::get<3>(GetParam()));

  const char kAppDisplayName[] = "name";
  const char kAppDisplayVersion[] = "version";
  const char kAppPublisher[] = "publisher";
  const char kAppType[] = "app_type";

  const wchar_t kApp1[] = L"app1";
  const wchar_t kAppDisplayName1[] = L"appName1";
  const wchar_t kAppDisplayVersion1[] = L"appVersion1";
  const wchar_t kAppPublisher1[] = L"appPublisher1";

  const wchar_t kApp2[] = L"app2";
  const wchar_t kAppDisplayName2[] = L"appName2";
  const wchar_t kAppDisplayVersion2[] = L"appVersion2";

  const wchar_t kApp3[] = L"app3";
  const wchar_t kAppDisplayVersion3[] = L"appVersion3";

  const wchar_t kInstalledWin32AppsRegistryPath[] =
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
  const wchar_t kInstalledWin32AppsRegistryPathWOW6432[] =
      L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
  const wchar_t kDelimiter[] = L"\\";
  const wchar_t kAppDisplayNameRegistryKey[] = L"DisplayName";
  const wchar_t kAppDisplayVersionRegistryKey[] = L"DisplayVersion";
  const wchar_t kAppPublisherRegistryKey[] = L"Publisher";

  std::wstring user_sid = L"invalid-user-sid";
  if (has_valid_sid) {
    // Create a fake user associated to a gaia id.
    CComBSTR sid_str;
    ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                        kDefaultUsername, L"password", L"Full Name", L"comment",
                        base::UTF8ToWide(kDefaultGaiaId), L"user@company.com",
                        &sid_str));
    user_sid = OLE2W(sid_str);
  }

  if (has_app_data) {
    // Set valid app data at
    // SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall.
    std::wstring app_path_1 = std::wstring(kInstalledWin32AppsRegistryPath)
                                  .append(std::wstring(kDelimiter))
                                  .append(kApp1);
    SetMachineRegString(app_path_1, kAppDisplayNameRegistryKey,
                        kAppDisplayName1);
    SetMachineRegString(app_path_1, kAppDisplayVersionRegistryKey,
                        kAppDisplayVersion1);
    SetMachineRegString(app_path_1, kAppPublisherRegistryKey, kAppPublisher1);

    // Set valid app data at
    // SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall.
    std::wstring app_path_2 =
        std::wstring(kInstalledWin32AppsRegistryPathWOW6432)
            .append(std::wstring(kDelimiter))
            .append(kApp2);
    SetMachineRegString(app_path_2, kAppDisplayNameRegistryKey,
                        kAppDisplayName2);
    SetMachineRegString(app_path_2, kAppDisplayVersionRegistryKey,
                        kAppDisplayVersion2);

    // Set app data without display name.
    std::wstring app_path_3 = std::wstring(kInstalledWin32AppsRegistryPath)
                                  .append(std::wstring(kDelimiter))
                                  .append(kApp3);
    SetMachineRegString(app_path_3, kAppDisplayVersionRegistryKey,
                        kAppDisplayVersion3);
  }

  GURL app_inventory_url =
      AppInventoryManager::Get()->GetGemServiceUploadAppInventoryUrl();
  ASSERT_TRUE(app_inventory_url.is_valid());

  auto expected_response_value = base::Value::Dict().Set(
      "deviceResourceId", base::WideToUTF8(device_resource_id));
  std::string expected_response;
  base::JSONWriter::Write(expected_response_value, &expected_response);

  fake_http_url_fetcher_factory()->SetCollectRequestData(true);
  // Set upload device details server response.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      app_inventory_url, FakeWinHttpUrlFetcher::Headers(), expected_response);

  extension::UserDeviceContext context(device_resource_id, L"", L"", user_sid,
                                       dm_token);

  auto task(app_inventory_task_creator_.Run());
  ASSERT_TRUE(task);

  ASSERT_TRUE(SUCCEEDED(task->SetContext({context})));
  HRESULT status = task->Execute();

  if (!has_valid_sid || device_resource_id.empty() || dm_token.empty()) {
    ASSERT_TRUE(FAILED(status));
    ASSERT_EQ(fake_http_url_fetcher_factory()->requests_created(), 0uLL);
  } else {
    ASSERT_TRUE(SUCCEEDED(status));
    ASSERT_EQ(fake_http_url_fetcher_factory()->requests_created(), 1uLL);
    FakeWinHttpUrlFetcherFactory::RequestData request_data =
        fake_http_url_fetcher_factory()->GetRequestData(0);

    std::optional<base::Value> body_value =
        base::JSONReader::Read(request_data.body);

    base::Value::Dict request;

    request.Set("device_resource_id", "valid-device-resource-id");
    request.Set("dm_token", "valid-dm-token");
    request.Set("obfuscated_gaia_id", "test-gaia-id");
    request.Set("user_sid", "S-1-4-2");
    base::Value::List app_info_value_list;

    if (has_app_data) {
      base::Value::Dict request_dict_1;
      request_dict_1.Set(kAppDisplayName, base::WideToUTF8(kAppDisplayName1));
      request_dict_1.Set(kAppDisplayVersion,
                         base::WideToUTF8(kAppDisplayVersion1));
      request_dict_1.Set(kAppPublisher, base::WideToUTF8(kAppPublisher1));
      // WIN_32
      request_dict_1.Set(kAppType, 1);
      app_info_value_list.Append(std::move(request_dict_1));

      base::Value::Dict request_dict_2;
      request_dict_2.Set(kAppDisplayName, base::WideToUTF8(kAppDisplayName2));
      request_dict_2.Set(kAppDisplayVersion,
                         base::WideToUTF8(kAppDisplayVersion2));
      request_dict_2.Set(kAppType, 1);
      app_info_value_list.Append(std::move(request_dict_2));
    }

    request.Set("windows_gpcw_app_info", std::move(app_info_value_list));
    ASSERT_EQ(body_value.value(), request);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AppInventoryManagerTest,
    ::testing::Combine(::testing::Values(L"", L"valid-device-resource-id"),
                       ::testing::Bool(),
                       ::testing::Bool(),
                       ::testing::Values(L"", L"valid-dm-token")));

}  // namespace testing
}  // namespace credential_provider
