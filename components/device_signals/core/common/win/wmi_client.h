// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_WIN_WMI_CLIENT_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_WIN_WMI_CLIENT_H_

#include <string>
#include <vector>

#include "base/win/wmi.h"
#include "components/device_signals/core/common/win/win_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// WMI interfaces are available on Windows Vista and above, and are officially
// undocumented.
namespace device_signals {

// Errors that can occur when calling WMI, or parsing response values.
enum class WmiParsingError {
  kFailedToIterateResults = 0,
  kFailedToGetName = 1,
  kMaxValue = kFailedToGetName
};

// Response object for calls to retrieve information about installed hotfix
// updates.
struct WmiHotfixesResponse {
  WmiHotfixesResponse();
  WmiHotfixesResponse(const WmiHotfixesResponse& other);

  ~WmiHotfixesResponse();

  std::vector<InstalledHotfix> hotfixes;
  absl::optional<base::win::WmiError> query_error;
  std::vector<WmiParsingError> parsing_errors;
};

// Interface for a client instance used to get information from WMI.
class WmiClient {
 public:
  virtual ~WmiClient() = default;

  // Will retrieve information about installed hotfix updates.
  virtual WmiHotfixesResponse GetInstalledHotfixes() = 0;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_WIN_WMI_CLIENT_H_
