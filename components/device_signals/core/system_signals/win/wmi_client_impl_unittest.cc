// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/win/wmi_client_impl.h"

#include <wbemidl.h>
#include <windows.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/win/wmi.h"
#include "components/device_signals/core/common/win/win_types.h"
#include "components/device_signals/core/system_signals/win/com_fakes.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using Microsoft::WRL::ComPtr;

namespace device_signals {

namespace {

constexpr uint8_t kOffAvState = 0;
constexpr uint8_t kOnAvState = 1;
constexpr uint8_t kSnoozedAvState = 2;
constexpr uint8_t kUnknownAvState = 3;

FakeWbemClassObject CreateHotfixObject(const std::wstring& hotfix_id) {
  FakeWbemClassObject hotfix_obj;
  hotfix_obj.Set(L"HotFixId", hotfix_id);
  return hotfix_obj;
}

FakeWbemClassObject CreateAvObject(const std::wstring& display_name,
                                   const std::wstring& product_id,
                                   uint8_t state) {
  FakeWbemClassObject av_obj;
  av_obj.Set(L"displayName", display_name);
  av_obj.Set(L"instanceGuid", product_id);

  internal::PRODUCT_STATE product_state;
  product_state.security_state = state;

  LONG state_val;
  std::copy(
      reinterpret_cast<const char*>(&product_state),
      reinterpret_cast<const char*>(&product_state) + sizeof product_state,
      reinterpret_cast<char*>(&state_val));
  av_obj.Set(L"productState", state_val);

  return av_obj;
}

}  // namespace

class WmiClientImplTest : public testing::Test {
 public:
  WmiClientImplTest()
      : wmi_client_(base::BindRepeating(&WmiClientImplTest::RunQuery,
                                        base::Unretained(this))) {}

 protected:
  absl::optional<base::win::WmiError> RunQuery(
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
    return absl::nullopt;
  }

  void ExpectAvQueryRan() {
    EXPECT_EQ(captured_server_name_, base::win::kSecurityCenter2ServerName);
    EXPECT_EQ(captured_query_, L"SELECT * FROM AntiVirusProduct");
    EXPECT_EQ(nb_calls_, 1U);
  }

  void ExpectHotfixQueryRan() {
    EXPECT_EQ(captured_server_name_, base::win::kCimV2ServerName);
    EXPECT_EQ(captured_query_, L"SELECT * FROM Win32_QuickFixEngineering");
    EXPECT_EQ(nb_calls_, 1U);
  }

  FakeEnumWbemClassObject fake_enumerator_;
  absl::optional<base::win::WmiError> query_error_;

  std::wstring captured_server_name_;
  std::wstring captured_query_;
  uint32_t nb_calls_ = 0;

  WmiClientImpl wmi_client_;
};

// Tests how the client behaves when the WMI query fails when querying for AV
// products.
TEST_F(WmiClientImplTest, GetAntiVirusProducts_FailedRunQuery) {
  query_error_ = base::win::WmiError::kFailedToConnectToWMI;

  auto av_response = wmi_client_.GetAntiVirusProducts();

  ExpectAvQueryRan();
  EXPECT_EQ(av_response.av_products.size(), 0U);
  EXPECT_EQ(av_response.parsing_errors.size(), 0U);
  EXPECT_EQ(av_response.query_error,
            base::win::WmiError::kFailedToConnectToWMI);
}

// Tests how the client behaves when iterating through objects with all known
// AV product states.
TEST_F(WmiClientImplTest, GetAntiVirusProducts_AllProductStates) {
  std::wstring display_name1 = L"Av Display Name 1";
  std::wstring product_id1 = L"product ID 1";
  FakeWbemClassObject av_obj1 =
      CreateAvObject(display_name1, product_id1, kOffAvState);

  std::wstring display_name2 = L"Av Display Name 2";
  std::wstring product_id2 = L"product ID 2";
  FakeWbemClassObject av_obj2 =
      CreateAvObject(display_name2, product_id2, kOnAvState);

  std::wstring display_name3 = L"Av Display Name 3";
  std::wstring product_id3 = L"product ID 3";
  FakeWbemClassObject av_obj3 =
      CreateAvObject(display_name3, product_id3, kSnoozedAvState);

  std::wstring display_name4 = L"Av Display Name 4";
  std::wstring product_id4 = L"product ID 4";
  FakeWbemClassObject av_obj4 =
      CreateAvObject(display_name3, product_id4, kUnknownAvState);

  fake_enumerator_.Add(&av_obj1);
  fake_enumerator_.Add(&av_obj2);
  fake_enumerator_.Add(&av_obj3);
  fake_enumerator_.Add(&av_obj4);

  auto av_response = wmi_client_.GetAntiVirusProducts();

  ExpectAvQueryRan();
  EXPECT_EQ(av_response.query_error, absl::nullopt);

  // Known states were parsed successfully.
  EXPECT_EQ(av_response.av_products.size(), 3U);
  EXPECT_EQ(av_response.av_products[0].display_name,
            base::SysWideToUTF8(display_name1));
  EXPECT_EQ(av_response.av_products[0].product_id,
            base::SysWideToUTF8(product_id1));
  EXPECT_EQ(av_response.av_products[0].state, AvProductState::kOff);
  EXPECT_EQ(av_response.av_products[1].display_name,
            base::SysWideToUTF8(display_name2));
  EXPECT_EQ(av_response.av_products[1].product_id,
            base::SysWideToUTF8(product_id2));
  EXPECT_EQ(av_response.av_products[1].state, AvProductState::kOn);
  EXPECT_EQ(av_response.av_products[2].display_name,
            base::SysWideToUTF8(display_name3));
  EXPECT_EQ(av_response.av_products[2].product_id,
            base::SysWideToUTF8(product_id3));
  EXPECT_EQ(av_response.av_products[2].state, AvProductState::kSnoozed);

  // Unknown state is returned as parsing error.
  ASSERT_EQ(av_response.parsing_errors.size(), 1U);
  EXPECT_EQ(av_response.parsing_errors[0], WmiParsingError::kStateInvalid);
}

// Tests how the client behaves when parsing an AV object which is missing the
// displayName value.
TEST_F(WmiClientImplTest, GetAntiVirusProducts_MissingDisplayName) {
  FakeWbemClassObject av_obj1 =
      CreateAvObject(L"Av Display Name", L"product ID", kOffAvState);

  av_obj1.Delete(L"displayName");

  fake_enumerator_.Add(&av_obj1);

  auto av_response = wmi_client_.GetAntiVirusProducts();

  ExpectAvQueryRan();
  EXPECT_EQ(av_response.query_error, absl::nullopt);
  EXPECT_EQ(av_response.av_products.size(), 0U);

  // Missing name property is returned as parsing error.
  ASSERT_EQ(av_response.parsing_errors.size(), 1U);
  EXPECT_EQ(av_response.parsing_errors[0], WmiParsingError::kFailedToGetName);
}

// Tests how the client behaves when parsing an AV object which is missing the
// displayName value.
TEST_F(WmiClientImplTest, GetAntiVirusProducts_MissingProductId) {
  FakeWbemClassObject av_obj1 =
      CreateAvObject(L"Av Display Name", L"product ID", kOffAvState);

  av_obj1.Delete(L"instanceGuid");

  fake_enumerator_.Add(&av_obj1);

  auto av_response = wmi_client_.GetAntiVirusProducts();

  ExpectAvQueryRan();
  EXPECT_EQ(av_response.query_error, absl::nullopt);
  EXPECT_EQ(av_response.av_products.size(), 0U);

  // Missing ID property is returned as parsing error.
  ASSERT_EQ(av_response.parsing_errors.size(), 1U);
  EXPECT_EQ(av_response.parsing_errors[0], WmiParsingError::kFailedToGetId);
}

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
  EXPECT_EQ(hotfix_response.query_error, absl::nullopt);

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
