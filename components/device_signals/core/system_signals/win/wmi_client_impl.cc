// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/win/wmi_client_impl.h"

#include <wbemidl.h>
#include <windows.h>
#include <wrl/client.h>

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/com_init_util.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "base/win/wmi.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using Microsoft::WRL::ComPtr;

namespace device_signals {

namespace {

// Parses a string value from `class_object` named `property_name`.
absl::optional<std::string> ParseString(
    const std::wstring& property_name,
    const ComPtr<IWbemClassObject>& class_object) {
  base::win::ScopedVariant string_variant;
  HRESULT hr = class_object->Get(property_name.c_str(), 0,
                                 string_variant.Receive(), 0, 0);

  if (FAILED(hr) || string_variant.type() != VT_BSTR) {
    return absl::nullopt;
  }

  // Owned by ScopedVariant.
  BSTR temp_bstr = V_BSTR(string_variant.ptr());
  return base::SysWideToUTF8(
      std::wstring(temp_bstr, ::SysStringLen(temp_bstr)));
}

}  // namespace

WmiClientImpl::WmiClientImpl()
    : run_query_callback_(base::BindRepeating(base::win::RunWmiQuery)) {}

WmiClientImpl::WmiClientImpl(RunWmiQueryCallback run_query_callback)
    : run_query_callback_(std::move(run_query_callback)) {}

WmiClientImpl::~WmiClientImpl() = default;

WmiAvProductsResponse WmiClientImpl::GetAntiVirusProducts() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  ComPtr<IEnumWbemClassObject> enumerator;
  auto error_code =
      run_query_callback_.Run(base::win::kSecurityCenter2ServerName,
                              L"SELECT * FROM AntiVirusProduct", &enumerator);

  WmiAvProductsResponse response;
  if (error_code.has_value()) {
    response.query_error = error_code.value();
    return response;
  }

  // Iterate over the results of the WMI query. Each result will be an
  // AntiVirusProduct instance.
  HRESULT hr;
  while (true) {
    ComPtr<IWbemClassObject> class_object;
    ULONG items_returned = 0U;
    hr = enumerator->Next(WBEM_INFINITE, 1, &class_object, &items_returned);

    if (hr == WBEM_S_FALSE || items_returned == 0) {
      // Reached the end of the enumerator.
      break;
    }

    // Something went wrong and it wasn't the end of the enumerator.
    if (FAILED(hr)) {
      response.parsing_errors.push_back(
          WmiParsingError::kFailedToIterateResults);
      continue;
    }

    base::win::ScopedVariant product_state;
    hr = class_object->Get(L"productState", 0, product_state.Receive(), 0, 0);

    if (FAILED(hr) || product_state.type() != VT_I4) {
      response.parsing_errors.push_back(WmiParsingError::kFailedToGetState);
      continue;
    }

    LONG state_val = V_I4(product_state.ptr());
    internal::PRODUCT_STATE product_state_struct;
    std::copy(reinterpret_cast<const char*>(&state_val),
              reinterpret_cast<const char*>(&state_val) + sizeof state_val,
              reinterpret_cast<char*>(&product_state_struct));
    // Map the values from product_state_struct to the av struct values.
    AvProduct av_product;
    switch (product_state_struct.security_state) {
      case 0:
        av_product.state = AvProductState::kOff;
        break;
      case 1:
        av_product.state = AvProductState::kOn;
        break;
      case 2:
        av_product.state = AvProductState::kSnoozed;
        break;
      default:
        // Unknown state.
        response.parsing_errors.push_back(WmiParsingError::kStateInvalid);
        continue;
    }

    absl::optional<std::string> display_name =
        ParseString(L"displayName", class_object);
    if (!display_name.has_value()) {
      response.parsing_errors.push_back(WmiParsingError::kFailedToGetName);
      continue;
    }

    av_product.display_name = display_name.value();

    absl::optional<std::string> product_id =
        ParseString(L"instanceGuid", class_object);
    if (!product_id.has_value()) {
      response.parsing_errors.push_back(WmiParsingError::kFailedToGetId);
      continue;
    }

    av_product.product_id = product_id.value();

    // If all values were parsed properly, add `av_product` into the response
    // vector. If any value could not be parsed properly, the item was
    // discarded.
    response.av_products.push_back(av_product);
  }

  return response;
}

WmiHotfixesResponse WmiClientImpl::GetInstalledHotfixes() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  ComPtr<IEnumWbemClassObject> enumerator;
  auto error_code = run_query_callback_.Run(
      base::win::kCimV2ServerName, L"SELECT * FROM Win32_QuickFixEngineering",
      &enumerator);

  WmiHotfixesResponse response;
  if (error_code.has_value()) {
    response.query_error = error_code.value();
    return response;
  }

  HRESULT hr;
  while (true) {
    ComPtr<IWbemClassObject> class_object;
    ULONG items_returned = 0U;
    hr = enumerator->Next(WBEM_INFINITE, 1, &class_object, &items_returned);

    if (hr == WBEM_S_FALSE || items_returned == 0U) {
      // Reached the end of the enumerator.
      break;
    }

    // Something went wrong, and it wasn't the end of the enumerator.
    if (FAILED(hr)) {
      response.parsing_errors.push_back(
          WmiParsingError::kFailedToIterateResults);
      continue;
    }

    absl::optional<std::string> hotfix_id =
        ParseString(L"HotFixId", class_object);
    if (!hotfix_id.has_value()) {
      response.parsing_errors.push_back(WmiParsingError::kFailedToGetName);
      continue;
    }

    InstalledHotfix hotfix;
    hotfix.hotfix_id = hotfix_id.value();

    // If all values were parsed properly, add `hotfix` into the response
    // vector. If any value could not be parsed properly, the item was
    // discarded.
    response.hotfixes.push_back(hotfix);
  }

  return response;
}

}  // namespace device_signals
