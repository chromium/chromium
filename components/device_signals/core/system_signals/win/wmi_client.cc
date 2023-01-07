// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/win/wmi_client.h"

namespace device_signals {

WmiAvProductsResponse::WmiAvProductsResponse() = default;

WmiAvProductsResponse::WmiAvProductsResponse(
    const WmiAvProductsResponse& other) = default;

WmiAvProductsResponse::~WmiAvProductsResponse() = default;

WmiHotfixesResponse::WmiHotfixesResponse() = default;

WmiHotfixesResponse::WmiHotfixesResponse(const WmiHotfixesResponse& other) =
    default;

WmiHotfixesResponse::~WmiHotfixesResponse() = default;

}  // namespace device_signals
