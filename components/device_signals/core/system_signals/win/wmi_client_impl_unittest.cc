// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/win/wmi_client_impl.h"

#include <windows.h>

#include <wbemidl.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/win/wmi.h"
#include "components/device_signals/core/common/win/win_types.h"
#include "components/device_signals/core/system_signals/win/com_fakes.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using Microsoft::WRL::ComPtr;

namespace device_signals {

namespace {

FakeWbemClassObject CreateHotfixObject(const std::wstring& hotfix_id) {
  FakeWbemClassObject hotfix_obj;
  hotfix_obj.Set(L"HotFixId", hotfix_id);
  return hotfix_obj;
}

}  // namespace

class WmiClientImplTest : public testing::Test {
 public:
  WmiClientImplTest()
      : wmi_client_(base::BindRepeating(&WmiClientImplTest::RunQuery,
                                        base::Unretained(this))) {}

 protected:
  std::optional<base::win::WmiError> RunQuery(
      const std::wstring& server_name,
      const std::wstring& query,
      ComPtr<IEnumWbemClassObject>* enumerator) {
    ++nb_calls_;
    captured_server_name_ = server_name;
    captured_query_ = query;

    if (query_error_.has_value()) {
      return query_error_.value();
    }
    *enumerator = &fake_enumerator_;
    return std::nullopt;
  }

  void ExpectHotfixQueryRan() {
    EXPECT_EQ(captured_server_name_, base::win::kCimV2ServerName);
    EXPECT_EQ(captured_query_, L"SELECT * FROM Win32_QuickFixEngineering");
    EXPECT_EQ(nb_calls_, 1U);
  }

  FakeEnumWbemClassObject fake_enumerator_;
  std::optional<base::win::WmiError> query_error_;

  std::wstring captured_server_name_;
  std::wstring captured_query_;
  uint32_t nb_calls_ = 0;

  WmiClientImpl wmi_client_;
};

// Tests how the client behaves when the WMI query fails when querying for
// installed hotfixes.
TEST_F(WmiClientImplTest, GetInstalledHotfixes_FailedRunQuery) {
  query_error_ = base::win::WmiError::kFailedToConnectToWMI;

  auto hotfix_response = wmi_client_.GetInstalledHotfixes();

  ExpectHotfixQueryRan();
  EXPECT_EQ(hotfix_response.hotfixes.size(), 0U);
  EXPECT_EQ(hotfix_response.parsing_errors.size(), 0U);
  EXPECT_EQ(hotfix_response.query_error,
            base::win::WmiError::kFailedToConnectToWMI);
}

// Tests how the client behaves when parsing hotfix objects, one of which is
// valid, and another which is missing an ID.
TEST_F(WmiClientImplTest, GetInstalledHotfixes_ParsingItems) {
  std::wstring hotfix_id1 = L"some_hotfix_id";
  FakeWbemClassObject hotfix_obj1 = CreateHotfixObject(hotfix_id1);

  FakeWbemClassObject hotfix_obj2 = CreateHotfixObject(L"some_other_id");

  hotfix_obj2.Delete(L"HotFixId");

  fake_enumerator_.Add(&hotfix_obj1);
  fake_enumerator_.Add(&hotfix_obj2);

  auto hotfix_response = wmi_client_.GetInstalledHotfixes();

  ExpectHotfixQueryRan();
  EXPECT_EQ(hotfix_response.query_error, std::nullopt);

  // Success item.
  ASSERT_EQ(hotfix_response.hotfixes.size(), 1U);
  EXPECT_EQ(hotfix_response.hotfixes[0].hotfix_id,
            base::SysWideToUTF8(hotfix_id1));

  // Failed item.
  ASSERT_EQ(hotfix_response.parsing_errors.size(), 1U);
  EXPECT_EQ(hotfix_response.parsing_errors[0],
            WmiParsingError::kFailedToGetName);
}

}  // namespace device_signals
