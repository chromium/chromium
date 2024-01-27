// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_WIN_WSC_CLIENT_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_WIN_WSC_CLIENT_H_

#include <optional>
#include <string>
#include <vector>

#include "components/device_signals/core/common/win/win_types.h"

// WSC interfaces are available on Windows 8 and above. More information at:
// https://docs.microsoft.com/en-us/windows/win32/api/iwscapi/
namespace device_signals {

// Errors that can occur when querying WSC.
// Do not change ordering. This enum is captured as
// `DeviceSignalsWscQueryError` in enums.xml.
enum class WscQueryError {
  kFailedToCreateInstance = 0,
  kFailedToInitializeProductList = 1,
  kFailedToGetProductCount = 2,
  kMaxValue = kFailedToGetProductCount
};

// Errors that can occur when calling WSC, or parsing response values.
// Do not change ordering. This enum is captured as
// `DeviceSignalsWscParsingError` in enums.xml.
enum class WscParsingError {
  kFailedToGetItem = 0,
  kFailedToGetState = 1,
  kStateInvalid = 2,
  kFailedToGetName = 3,
  kFailedToGetId = 4,
  kMaxValue = kFailedToGetId
};

// Response object for calls to retrieve information about installed AntiVirus
// software.
struct WscAvProductsResponse {
  WscAvProductsResponse();
  ~WscAvProductsResponse();

  WscAvProductsResponse(const WscAvProductsResponse& other);

  std::vector<AvProduct> av_products;
  std::optional<WscQueryError> query_error;
  std::vector<WscParsingError> parsing_errors;
};

// Interface for a client instance used to get information from Windows Security
// Center.
class WscClient {
 public:
  virtual ~WscClient() = default;

  // Will retrieve information about installed AntiVirus software.
  virtual WscAvProductsResponse GetAntiVirusProducts() = 0;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_WIN_WSC_CLIENT_H_
