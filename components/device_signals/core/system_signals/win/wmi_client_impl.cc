// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/win/wmi_client_impl.h"

#include <windows.h>

#include <wbemidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <optional>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/com_init_util.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "base/win/wmi.h"

using Microsoft::WRL::ComPtr;

namespace device_signals {

namespace {

// Parses a string value from `class_object` named `property_name`.
std::optional<std::string> ParseString(
    const std::wstring& property_name,
    const ComPtr<IWbemClassObject>& class_object) {
  base::win::ScopedVariant string_variant;
  HRESULT hr = class_object->Get(property_name.c_str(), 0,
                                 string_variant.Receive(), 0, 0);

  if (FAILED(hr) || string_variant.type() != VT_BSTR) {
    return std::nullopt;
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

    std::optional<std::string> hotfix_id =
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
