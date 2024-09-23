// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gem_device_details_manager.h"

#include <windows.h>

#include "base/base_paths_win.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
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

class GemDeviceDetailsBaseTest : public GlsRunnerTestBase {};

// Tests upload device details by ESA service.
// Params:
// string : The specified device resource ID.
// bool : Whether a valid user sid is present.
// bool : Whether the username and domain lookup fails.
// string : The specified DM token.
class GemDeviceDetailsExtensionTest
    : public GemDeviceDetailsBaseTest,
      public ::testing::WithParamInterface<
          std::tuple<const wchar_t*, bool, bool, const wchar_t*>> {
 public:
  GemDeviceDetailsExtensionTest();

 protected:
  extension::TaskCreator fetch_policy_task_creator_;
};

GemDeviceDetailsExtensionTest::GemDeviceDetailsExtensionTest() {
  fetch_policy_task_creator_ =
      GemDeviceDetailsManager::UploadDeviceDetailsTaskCreator();
}

TEST_P(GemDeviceDetailsExtensionTest, WithUserDeviceContext) {
  const std::wstring device_resource_id(std::get<0>(GetParam()));
  bool has_valid_sid = std::get<1>(GetParam());
  bool fail_sid_lookup = std::get<2>(GetParam());
  const std::wstring dm_token(std::get<3>(GetParam()));

  std::wstring user_sid = L"invalid-user-sid";
  std::wstring domain_name = L"company.com";
  if (has_valid_sid) {
    // Create a fake user associated to a gaia id.
    CComBSTR sid_str;
    ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                        kDefaultUsername, L"password", L"Full Name", L"comment",
                        base::UTF8ToWide(kDefaultGaiaId), L"user@company.com",
                        domain_name, &sid_str));
    user_sid = OLE2W(sid_str);
  }

  if (fail_sid_lookup) {
    fake_os_user_manager()->FailFindUserBySID(user_sid.c_str(), 1);
  }

  auto expected_response_value = base::Value::Dict().Set(
      "deviceResourceId", base::WideToUTF8(device_resource_id));
  std::string expected_response;
  base::JSONWriter::Write(expected_response_value, &expected_response);

  GURL upload_device_details_url =
      GemDeviceDetailsManager::Get()->GetGemServiceUploadDeviceDetailsUrl();
  ASSERT_TRUE(upload_device_details_url.is_valid());

  // Set upload device details server response.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      upload_device_details_url, FakeWinHttpUrlFetcher::Headers(),
      expected_response);
  fake_http_url_fetcher_factory()->SetCollectRequestData(true);

  extension::UserDeviceContext context(device_resource_id, L"", L"", user_sid,
                                       dm_token);

  auto task(fetch_policy_task_creator_.Run());
  ASSERT_TRUE(task);

  ASSERT_TRUE(SUCCEEDED(task->SetContext({context})));
  HRESULT status = task->Execute();

  if (!has_valid_sid) {
    ASSERT_TRUE(FAILED(status));
  } else {
    ASSERT_TRUE(SUCCEEDED(status));

    ASSERT_EQ(1UL, fake_http_url_fetcher_factory()->requests_created());
    FakeWinHttpUrlFetcherFactory::RequestData request_data =
        fake_http_url_fetcher_factory()->GetRequestData(0);
    base::Value::Dict body_dict =
        base::JSONReader::ReadDict(request_data.body).value();

    std::string uploaded_dm_token = GetDictStringUTF8(body_dict, "dm_token");
    ASSERT_EQ(uploaded_dm_token, base::WideToUTF8(dm_token));

    std::string uploaded_username =
        GetDictStringUTF8(body_dict, "account_username");
    std::string uploaded_domain = GetDictStringUTF8(body_dict, "device_domain");
    if (!fail_sid_lookup) {
      ASSERT_EQ(uploaded_username, base::WideToUTF8(kDefaultUsername));
      ASSERT_EQ(uploaded_domain, base::WideToUTF8(domain_name));
    } else {
      ASSERT_EQ(uploaded_username, "");
      ASSERT_EQ(uploaded_domain, "");
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GemDeviceDetailsExtensionTest,
    ::testing::Combine(::testing::Values(L"valid-device-resource-id"),
                       ::testing::Bool(),
                       ::testing::Bool(),
                       ::testing::Values(L"valid-dm-token")));

}  // namespace testing
}  // namespace credential_provider
