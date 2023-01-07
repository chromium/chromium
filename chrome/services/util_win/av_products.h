// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_UTIL_WIN_AV_PRODUCTS_H_
#define CHROME_SERVICES_UTIL_WIN_AV_PRODUCTS_H_

#include <string>
#include <vector>

#include "third_party/metrics_proto/system_profile.pb.h"

using AvProduct = metrics::SystemProfileProto_AntiVirusProduct;

std::vector<AvProduct> GetAntiVirusProducts(bool report_full_names);

// Everything in the internal namespace is exposed for testing.
namespace internal {

enum class ResultCode {
  kSuccess = 0,
  kGenericFailure = 1,
  kFailedToInitializeCOM = 2,
  kFailedToCreateInstance = 3,
  kFailedToInitializeProductList = 4,
  kFailedToGetProductCount = 5,
  kFailedToGetItem = 6,
  kFailedToGetProductState = 7,
  kProductStateInvalid = 8,
  kFailedToGetProductName = 9,
  kFailedToGetRemediationPath = 10,
  kFailedToConnectToWMI = 11,
  kFailedToSetSecurityBlanket = 12,
  kFailedToExecWMIQuery = 13,
  kFailedToIterateResults = 14,
  kWSCNotAvailable = 15,
  kMaxValue = kWSCNotAvailable,
};

std::string TrimVersionOfAvProductName(const std::string& av_product);

}  // namespace internal

#endif  // CHROME_SERVICES_UTIL_WIN_AV_PRODUCTS_H_
