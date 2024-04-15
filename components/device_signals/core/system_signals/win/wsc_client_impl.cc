// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/win/wsc_client_impl.h"

#include <windows.h>

#include <iwscapi.h>
#include <wrl/client.h>
#include <wscapi.h>

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

using Microsoft::WRL::ComPtr;

namespace device_signals {

namespace {

AvProductState ConvertState(WSC_SECURITY_PRODUCT_STATE state) {
  switch (state) {
    case WSC_SECURITY_PRODUCT_STATE_ON:
      return AvProductState::kOn;
    case WSC_SECURITY_PRODUCT_STATE_OFF:
      return AvProductState::kOff;
    case WSC_SECURITY_PRODUCT_STATE_SNOOZED:
      return AvProductState::kSnoozed;
    case WSC_SECURITY_PRODUCT_STATE_EXPIRED:
      return AvProductState::kExpired;
  }
}

HRESULT CreateProductList(ComPtr<IWSCProductList>* out_product_list) {
  ComPtr<IWSCProductList> product_list;
  HRESULT hr =
      ::CoCreateInstance(__uuidof(WSCProductList), nullptr,
                         CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&product_list));

  if (!FAILED(hr)) {
    *out_product_list = product_list;
  }

  return hr;
}

}  // namespace

WscClientImpl::WscClientImpl()
    : create_callback_(base::BindRepeating(CreateProductList)) {}

WscClientImpl::WscClientImpl(CreateProductListCallback create_callback)
    : create_callback_(std::move(create_callback)) {}

WscClientImpl::~WscClientImpl() = default;

WscAvProductsResponse WscClientImpl::GetAntiVirusProducts() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  WscAvProductsResponse response;

  ComPtr<IWSCProductList> product_list;
  HRESULT hr = create_callback_.Run(&product_list);
  if (FAILED(hr)) {
    response.query_error = WscQueryError::kFailedToCreateInstance;
    return response;
  }

  hr = product_list->Initialize(WSC_SECURITY_PROVIDER_ANTIVIRUS);
  if (FAILED(hr)) {
    response.query_error = WscQueryError::kFailedToInitializeProductList;
    return response;
  }

  LONG product_count;
  hr = product_list->get_Count(&product_count);
  if (FAILED(hr)) {
    response.query_error = WscQueryError::kFailedToGetProductCount;
    return response;
  }

  for (LONG i = 0; i < product_count; i++) {
    ComPtr<IWscProduct> product;
    hr = product_list->get_Item(i, &product);
    if (FAILED(hr)) {
      response.parsing_errors.push_back(WscParsingError::kFailedToGetItem);
      continue;
    }

    AvProduct av_product;
    WSC_SECURITY_PRODUCT_STATE product_state;
    hr = product->get_ProductState(&product_state);
    if (FAILED(hr)) {
      response.parsing_errors.push_back(WscParsingError::kFailedToGetState);
      continue;
    }

    av_product.state = ConvertState(product_state);

    base::win::ScopedBstr product_name;
    hr = product->get_ProductName(product_name.Receive());
    if (FAILED(hr)) {
      response.parsing_errors.push_back(WscParsingError::kFailedToGetName);
      continue;
    }
    av_product.display_name = base::SysWideToUTF8(
        std::wstring(product_name.Get(), product_name.Length()));

    base::win::ScopedBstr product_id;
    hr = product->get_ProductGuid(product_id.Receive());
    if (FAILED(hr)) {
      response.parsing_errors.push_back(WscParsingError::kFailedToGetId);
      continue;
    }
    av_product.product_id = base::SysWideToUTF8(
        std::wstring(product_id.Get(), product_id.Length()));

    response.av_products.push_back(std::move(av_product));
  }

  return response;
}

}  // namespace device_signals
