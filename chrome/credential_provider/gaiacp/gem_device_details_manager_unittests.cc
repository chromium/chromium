// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/base_paths_win.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "chrome/credential_provider/extension/user_device_context.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/gem_device_details_manager.h"
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
// bool : Whether the feature to upload device details via ESA is enabled.
// string : The specified DM token.
class GemDeviceDetailsExtensionTest
    : public GemDeviceDetailsBaseTest,
      public ::testing::WithParamInterface<
          std::tuple<const wchar_t*, bool, const wchar_t*>> {
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
  const base::string16 device_resource_id(std::get<0>(GetParam()));
  bool has_valid_sid = std::get<1>(GetParam());
  const base::string16 dm_token(std::get<2>(GetParam()));

  base::string16 user_sid = L"invalid-user-sid";
  if (has_valid_sid) {
    // Create a fake user associated to a gaia id.
    CComBSTR sid_str;
    ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                        kDefaultUsername, L"password", L"Full Name", L"comment",
                        base::UTF8ToUTF16(kDefaultGaiaId), L"user@company.com",
                        &sid_str));
    user_sid = OLE2W(sid_str);
  }

  base::Value expected_response_value(base::Value::Type::DICTIONARY);
  expected_response_value.SetStringKey("deviceResourceId", device_resource_id);
  std::string expected_response;
  base::JSONWriter::Write(expected_response_value, &expected_response);

  GURL upload_device_details_url =
      GemDeviceDetailsManager::Get()->GetGemServiceUploadDeviceDetailsUrl();
  ASSERT_TRUE(upload_device_details_url.is_valid());

  // Set upload device details server response.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      upload_device_details_url, FakeWinHttpUrlFetcher::Headers(),
      expected_response);

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
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GemDeviceDetailsExtensionTest,
    ::testing::Combine(::testing::Values(L"valid-device-resource-id"),
                       ::testing::Bool(),
                       ::testing::Values(L"valid-dm-token")));

}  // namespace testing
}  // namespace credential_provider
