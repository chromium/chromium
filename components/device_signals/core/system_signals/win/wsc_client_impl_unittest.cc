// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/win/wsc_client_impl.h"

#include <windows.h>

#include <iwscapi.h>
#include <wrl/client.h>
#include <wscapi.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "components/device_signals/core/common/win/win_types.h"
#include "components/device_signals/core/system_signals/win/com_fakes.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using Microsoft::WRL::ComPtr;

namespace device_signals {

struct AvTestData {
  const wchar_t* name;
  const wchar_t* id;
  WSC_SECURITY_PRODUCT_STATE state;
  AvProductState expected_state;
};

class WscClientImplTest : public testing::Test {
 protected:
  WscClientImplTest()
      : wsc_client_(base::BindRepeating(&WscClientImplTest::CreateProductList,
                                        base::Unretained(this))) {}

  HRESULT CreateProductList(ComPtr<IWSCProductList>* product_list) {
    if (fail_list_creation_) {
      return E_FAIL;
    }

    *product_list = &product_list_;
    return S_OK;
  }

  void ExpectAvInitialized() {
    EXPECT_TRUE(product_list_.provider().has_value());
    EXPECT_EQ(product_list_.provider().value(),
              WSC_SECURITY_PROVIDER_ANTIVIRUS);
  }

  bool fail_list_creation_ = false;

  FakeWSCProductList product_list_;
  WscClientImpl wsc_client_;
};

// Tests how the client handles all different product states when parsing
// products received from the list.
TEST_F(WscClientImplTest, GetAntiVirusProducts_AllStates) {
  std::vector<AvTestData> test_data;
  test_data.push_back({L"first name", L"first product id",
                       WSC_SECURITY_PRODUCT_STATE_ON, AvProductState::kOn});
  test_data.push_back({L"second name", L"second product id",
                       WSC_SECURITY_PRODUCT_STATE_OFF, AvProductState::kOff});
  test_data.push_back({L"third name", L"third product id",
                       WSC_SECURITY_PRODUCT_STATE_SNOOZED,
                       AvProductState::kSnoozed});
  test_data.push_back({L"fourth name", L"fourth product id",
                       WSC_SECURITY_PRODUCT_STATE_EXPIRED,
                       AvProductState::kExpired});

  // Used to keep products from going out of scope.
  std::vector<FakeWscProduct> products;

  for (const auto& data : test_data) {
    products.emplace_back(data.name, data.id, data.state);
  }

  for (auto& product : products) {
    product_list_.Add(&product);
  }

  auto response = wsc_client_.GetAntiVirusProducts();

  ExpectAvInitialized();
  EXPECT_EQ(response.query_error, std::nullopt);

  EXPECT_EQ(response.parsing_errors.size(), 0U);

  ASSERT_EQ(response.av_products.size(), test_data.size());
  ASSERT_GT(response.av_products.size(), 0U);

  for (size_t i = 0; i < test_data.size(); i++) {
    EXPECT_EQ(response.av_products[i].display_name,
              base::SysWideToUTF8(test_data[i].name));
    EXPECT_EQ(response.av_products[i].product_id,
              base::SysWideToUTF8(test_data[i].id));
    EXPECT_EQ(response.av_products[i].state, test_data[i].expected_state);
  }
}

// Tests how the client reacts to a failure occurring when trying to create a
// product list instance.
TEST_F(WscClientImplTest, GetAntiVirusProducts_FailedCreate) {
  fail_list_creation_ = true;

  auto response = wsc_client_.GetAntiVirusProducts();

  EXPECT_EQ(response.av_products.size(), 0U);
  EXPECT_EQ(response.parsing_errors.size(), 0U);

  ASSERT_TRUE(response.query_error.has_value());
  EXPECT_EQ(response.query_error.value(),
            WscQueryError::kFailedToCreateInstance);
}

// Tests how the client reacts to a failure occurring when trying to initialize
// a product list instance.
TEST_F(WscClientImplTest, GetAntiVirusProducts_FailedInitialize) {
  product_list_.set_failed_step(FakeWSCProductList::FailureStep::kInitialize);

  auto response = wsc_client_.GetAntiVirusProducts();

  EXPECT_EQ(response.av_products.size(), 0U);
  EXPECT_EQ(response.parsing_errors.size(), 0U);

  ASSERT_TRUE(response.query_error.has_value());
  EXPECT_EQ(response.query_error.value(),
            WscQueryError::kFailedToInitializeProductList);
}

// Tests how the client reacts to a failure occurring when trying to get a
// product count from a product list.
TEST_F(WscClientImplTest, GetAntiVirusProducts_FailedGetProductCount) {
  product_list_.set_failed_step(FakeWSCProductList::FailureStep::kGetCount);

  auto response = wsc_client_.GetAntiVirusProducts();

  ExpectAvInitialized();
  EXPECT_EQ(response.av_products.size(), 0U);
  EXPECT_EQ(response.parsing_errors.size(), 0U);

  ASSERT_TRUE(response.query_error.has_value());
  EXPECT_EQ(response.query_error.value(),
            WscQueryError::kFailedToGetProductCount);
}

// Tests how the client reacts to a failure occurring when trying to get an item
// from a product list.
TEST_F(WscClientImplTest, GetAntiVirusProducts_FailedGetItem) {
  product_list_.set_failed_step(FakeWSCProductList::FailureStep::kGetItem);

  auto* name1 = L"first name";
  auto* id1 = L"first product id";
  FakeWscProduct on_product(name1, id1, WSC_SECURITY_PRODUCT_STATE_ON);
  product_list_.Add(&on_product);

  auto response = wsc_client_.GetAntiVirusProducts();

  ExpectAvInitialized();
  EXPECT_EQ(response.av_products.size(), 0U);
  EXPECT_FALSE(response.query_error.has_value());

  ASSERT_EQ(response.parsing_errors.size(), 1U);
  EXPECT_EQ(response.parsing_errors[0], WscParsingError::kFailedToGetItem);
}

// Tests how the client reacts to a failure occurring when facing product
// parsing errors.
TEST_F(WscClientImplTest, GetAntiVirusProducts_ProductErrors) {
  // Valid product.
  auto* name1 = L"first name";
  auto* id1 = L"first product id";
  FakeWscProduct on_product(name1, id1, WSC_SECURITY_PRODUCT_STATE_ON);

  // Product missing a state.
  auto* name2 = L"second name";
  auto* id2 = L"second product id";
  FakeWscProduct stateless_product(name2, id2, WSC_SECURITY_PRODUCT_STATE_OFF);
  stateless_product.set_failed_step(FakeWscProduct::FailureStep::kProductState);

  // Product missing a name.
  auto* name3 = L"third name";
  auto* id3 = L"third product id";
  FakeWscProduct nameless_product(name3, id3,
                                  WSC_SECURITY_PRODUCT_STATE_SNOOZED);
  nameless_product.set_failed_step(FakeWscProduct::FailureStep::kProductName);

  // Product missing a name.
  auto* name4 = L"fourth name";
  auto* id4 = L"fourth product id";
  FakeWscProduct id_less_product(name4, id4,
                                 WSC_SECURITY_PRODUCT_STATE_SNOOZED);
  id_less_product.set_failed_step(FakeWscProduct::FailureStep::kProductId);

  product_list_.Add(&on_product);
  product_list_.Add(&stateless_product);
  product_list_.Add(&nameless_product);
  product_list_.Add(&id_less_product);

  auto response = wsc_client_.GetAntiVirusProducts();

  ExpectAvInitialized();
  EXPECT_FALSE(response.query_error.has_value());

  ASSERT_EQ(response.av_products.size(), 1U);
  EXPECT_EQ(response.av_products[0].display_name, base::SysWideToUTF8(name1));
  EXPECT_EQ(response.av_products[0].product_id, base::SysWideToUTF8(id1));
  EXPECT_EQ(response.av_products[0].state, AvProductState::kOn);

  ASSERT_EQ(response.parsing_errors.size(), 3U);
  EXPECT_EQ(response.parsing_errors[0], WscParsingError::kFailedToGetState);
  EXPECT_EQ(response.parsing_errors[1], WscParsingError::kFailedToGetName);
  EXPECT_EQ(response.parsing_errors[2], WscParsingError::kFailedToGetId);
}

// Smoke/sanity test to verify that Defender's instance GUID does not change
// over time. This test actually calls WSC.
TEST(RealWscClientImplTest, SmokeWsc_GetAntiVirusProducts) {
  base::win::ScopedCOMInitializer scoped_com_initializer;

  // That part of the display name is not translated when getting it from WSC,
  // so it can be used quite simply.
  constexpr char kPartialDefenderName[] = "Microsoft Defender";

  constexpr char kDefenderProductGuid[] =
      "{D68DDC3A-831F-4fae-9E44-DA132C1ACF46}";

  // WSC is only supported on Win8+ (and not server).
  base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  if (os_info->version_type() == base::win::SUITE_SERVER ||
      os_info->version() < base::win::Version::WIN8) {
    return;
  }

  WscClientImpl wsc_client;
  auto response = wsc_client.GetAntiVirusProducts();

  for (const auto& av_product : response.av_products) {
    if (av_product.display_name.find(kPartialDefenderName) !=
        std::string::npos) {
      EXPECT_EQ(av_product.product_id, kDefenderProductGuid);
    }
  }
}

}  // namespace device_signals
