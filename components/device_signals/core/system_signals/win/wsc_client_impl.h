// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_WIN_WSC_CLIENT_IMPL_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_WIN_WSC_CLIENT_IMPL_H_

#include <iwscapi.h>

#include "base/functional/callback.h"
#include "components/device_signals/core/system_signals/win/wsc_client.h"

namespace device_signals {

class WscClientImpl : public WscClient {
 public:
  using CreateProductListCallback = base::RepeatingCallback<HRESULT(
      Microsoft::WRL::ComPtr<IWSCProductList>*)>;

  WscClientImpl();

  ~WscClientImpl() override;

  // WscClient:
  WscAvProductsResponse GetAntiVirusProducts() override;

 private:
  friend class WscClientImplTest;

  // Constructor taking in a `create_callback` which can be used to mock
  // creating the product list COM object.
  explicit WscClientImpl(CreateProductListCallback create_callback);

  CreateProductListCallback create_callback_;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_WIN_WSC_CLIENT_IMPL_H_
