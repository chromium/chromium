// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/enterprise_proxy_error_data.h"

#include <utility>

namespace enterprise_net {

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

}  // namespace enterprise_net
