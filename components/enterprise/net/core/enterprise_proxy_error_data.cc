// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/enterprise_proxy_error_data.h"

#include <string_view>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"

namespace enterprise_net {

namespace {

std::string_view GetErrorCategoryHistogramSuffix(
    EnterpriseProxyErrorData::ErrorCategory category) {
  switch (category) {
    case EnterpriseProxyErrorData::ErrorCategory::kAuthentication:
      return "Authentication";
    case EnterpriseProxyErrorData::ErrorCategory::kAuthorization:
      return "Authorization";
    case EnterpriseProxyErrorData::ErrorCategory::kOther:
      return "Other";
  }
  NOTREACHED();
}

}  // namespace

EnterpriseProxyErrorData::EnterpriseProxyErrorData() = default;

EnterpriseProxyErrorData::EnterpriseProxyErrorData(GURL destination_url,
                                                   GURL proxy_url,
                                                   int error_code,
                                                   ErrorCategory error_category)
    : destination_url_(std::move(destination_url)),
      proxy_url_(std::move(proxy_url)),
      error_code_(error_code),
      error_category_(error_category) {}

EnterpriseProxyErrorData::EnterpriseProxyErrorData(
    const EnterpriseProxyErrorData&) = default;

EnterpriseProxyErrorData& EnterpriseProxyErrorData::operator=(
    const EnterpriseProxyErrorData&) = default;

EnterpriseProxyErrorData::EnterpriseProxyErrorData(EnterpriseProxyErrorData&&) =
    default;

EnterpriseProxyErrorData& EnterpriseProxyErrorData::operator=(
    EnterpriseProxyErrorData&&) = default;

EnterpriseProxyErrorData::~EnterpriseProxyErrorData() = default;

void RecordEnterpriseProxyErrorPageEvent(
    EnterpriseProxyErrorData::ErrorCategory category,
    SecureGatewayErrorPageEvent event) {
  static constexpr char kErrorPageEventHistogram[] =
      "Enterprise.SecureGateway.ErrorPage.Event";
  base::UmaHistogramEnumeration(kErrorPageEventHistogram, event);
  base::UmaHistogramEnumeration(
      base::StrCat({kErrorPageEventHistogram, ".",
                    GetErrorCategoryHistogramSuffix(category)}),
      event);
}

}  // namespace enterprise_net
