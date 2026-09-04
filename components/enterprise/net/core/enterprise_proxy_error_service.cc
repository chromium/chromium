// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/enterprise_proxy_error_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/enterprise/net/core/features.h"
#include "components/error_page/common/localized_error.h"

namespace enterprise_net {

const EnterpriseProxyErrorData*
EnterpriseProxyErrorService::Delegate::GetDisguisedErrorData() const {
  return nullptr;
}

void EnterpriseProxyErrorService::Delegate::AttachDisguisedErrorData(
    const EnterpriseProxyErrorData& error_data) {}

void EnterpriseProxyErrorService::Delegate::OnSignInRequired(
    const GURL& destination_url) {}

EnterpriseProxyErrorService::EnterpriseProxyErrorService(
    EnterpriseProxyService* enterprise_proxy_service)
    : enterprise_proxy_service_(enterprise_proxy_service) {
  CHECK(enterprise_proxy_service_);
}

EnterpriseProxyErrorService::~EnterpriseProxyErrorService() = default;

void EnterpriseProxyErrorService::RecordDisguisedError(
    int64_t navigation_id,
    EnterpriseProxyErrorData error_data) {
  disguised_errors_.insert_or_assign(navigation_id, std::move(error_data));
}

std::optional<EnterpriseProxyErrorData>
EnterpriseProxyErrorService::TakeDisguisedError(int64_t navigation_id) {
  auto it = disguised_errors_.find(navigation_id);
  if (it == disguised_errors_.end()) {
    return std::nullopt;
  }
  EnterpriseProxyErrorData data = std::move(it->second);
  disguised_errors_.erase(it);
  return data;
}

void EnterpriseProxyErrorService::RemoveDisguisedError(int64_t navigation_id) {
  disguised_errors_.erase(navigation_id);
}

base::DictValue EnterpriseProxyErrorService::GetErrorPageParams(
    const EnterpriseProxyErrorData& error_data) const {
  base::DictValue params;
  if (!IsEnterpriseProxyErrorHandlingEnabled()) {
    return params;
  }

  RecordErrorCodeHistogram(error_data.error_code());

  params.Set(error_page::kOverrideErrorPage, true);
  params.Set("title", "Enterprise Proxy Error");
  params.Set("destination_url", error_data.destination_url().spec());
  params.Set("proxy_url", error_data.proxy_url().spec());
  params.Set("error_code", base::NumberToString(error_data.error_code()));
  params.Set(
      "error_category",
      base::NumberToString(static_cast<int>(error_data.error_category())));
  params.Set("is_enterprise_proxy_error", true);
  return params;
}

// TODO(crbug.com/507058812): Remove Delegate and legacy overloads once
// chrome_content_browser_client.cc and http_auth_coordinator.cc switch to
// TakeDisguisedError and navigation_id.
std::string EnterpriseProxyErrorService::GetErrorPageHTML(
    Delegate* delegate) const {
  if (!IsEnterpriseProxyErrorHandlingEnabled() || !delegate) {
    return std::string();
  }
  const EnterpriseProxyErrorData* error_data =
      delegate->GetDisguisedErrorData();
  if (!error_data) {
    return std::string();
  }

  RecordErrorCodeHistogram(error_data->error_code());

  return GetErrorPageHTML(*error_data);
}

bool EnterpriseProxyErrorService::InterceptProxyAuthChallenge(
    const net::AuthChallengeInfo& auth_info,
    const GURL& destination_url,
    const scoped_refptr<net::HttpResponseHeaders>& response_headers,
    int64_t navigation_id,
    base::OnceCallback<void(const std::optional<net::AuthCredentials>&)>
        callback) {
  return InterceptProxyAuthChallenge(auth_info, destination_url,
                                     response_headers, navigation_id,
                                     /*delegate=*/nullptr, std::move(callback));
}

// TODO(crbug.com/507058812): Remove once http_auth_coordinator.cc switches to
// navigation_id.
bool EnterpriseProxyErrorService::InterceptProxyAuthChallenge(
    const net::AuthChallengeInfo& auth_info,
    const GURL& destination_url,
    const scoped_refptr<net::HttpResponseHeaders>& response_headers,
    int64_t navigation_id,
    std::unique_ptr<Delegate> delegate,
    base::OnceCallback<void(const std::optional<net::AuthCredentials>&)>
        callback) {
  bool is_handled = true;
  GURL proxy_url = auth_info.challenger.GetURL();
  int error_code = 0;
  std::optional<int> forced_error_code = GetForcedDisguisedErrorCode();
  if (forced_error_code.has_value()) {
    error_code = *forced_error_code;
  } else {
    base::StringToInt(auth_info.realm, &error_code);
  }
  auto eps_callback =
      base::BindOnce(&EnterpriseProxyErrorService::OnProxyAuthChallengeResult,
                     weak_ptr_factory_.GetWeakPtr(), &is_handled, navigation_id,
                     std::move(delegate), destination_url, std::move(proxy_url),
                     error_code, std::move(callback));

  enterprise_proxy_service_->HandleProxyAuthChallenge(
      auth_info, destination_url, response_headers, std::move(eps_callback));

  return is_handled;
}

// TODO(crbug.com/507058812): Remove once http_auth_coordinator.cc switches to
// navigation_id.
bool EnterpriseProxyErrorService::InterceptProxyAuthChallenge(
    const net::AuthChallengeInfo& auth_info,
    const GURL& destination_url,
    const scoped_refptr<net::HttpResponseHeaders>& response_headers,
    std::unique_ptr<Delegate> delegate,
    base::OnceCallback<void(const std::optional<net::AuthCredentials>&)>
        callback) {
  return InterceptProxyAuthChallenge(auth_info, destination_url,
                                     response_headers, /*navigation_id=*/0,
                                     std::move(delegate), std::move(callback));
}

void EnterpriseProxyErrorService::RecordErrorCodeHistogram(
    int error_code) const {
  base::UmaHistogramSparse("Enterprise.Proxy.DisguisedErrorPage.ErrorCode",
                           error_code);
}

// TODO(crbug.com/543015665): Replace with production error page HTML/TS
// template.
std::string EnterpriseProxyErrorService::GetErrorPageHTML(
    const EnterpriseProxyErrorData& error_data) const {
  return base::StringPrintf(
      "<!DOCTYPE html>\n"
      "<html>\n"
      "<head><title>Enterprise Proxy Error</title></head>\n"
      "<body>\n"
      "<h1>Enterprise Proxy Error</h1>\n"
      "<p>Destination URL: <span id=\"destination-url\">%s</span></p>\n"
      "<p>Proxy URL: <span id=\"proxy-url\">%s</span></p>\n"
      "<p>Disguised Error Code: <span id=\"error-code\">%d</span></p>\n"
      "</body>\n"
      "</html>\n",
      error_data.destination_url().spec().c_str(),
      error_data.proxy_url().spec().c_str(), error_data.error_code());
}

void EnterpriseProxyErrorService::MaybeRecordErrorForNavigation(
    int64_t navigation_id,
    const GURL& destination_url,
    const GURL& proxy_url,
    int error_code,
    EnterpriseProxyErrorData::ErrorCategory category) {
  if (navigation_id <= 0) {
    return;
  }
  RecordDisguisedError(navigation_id,
                       EnterpriseProxyErrorData(destination_url, proxy_url,
                                                error_code, category));
}

void EnterpriseProxyErrorService::OnProxyAuthChallengeResult(
    bool* handled_flag,
    int64_t navigation_id,
    std::unique_ptr<Delegate> delegate,
    const GURL& destination_url,
    const GURL& proxy_url,
    int error_code,
    base::OnceCallback<void(const std::optional<net::AuthCredentials>&)>
        coord_callback,
    EnterpriseProxyService::ProxyAuthChallengeResult result,
    const std::optional<net::AuthCredentials>& credentials) {
  switch (result) {
    case EnterpriseProxyService::ProxyAuthChallengeResult::kNotApplicable:
      *handled_flag = false;
      return;
    case EnterpriseProxyService::ProxyAuthChallengeResult::kDisguisedError: {
      EnterpriseProxyErrorData::ErrorCategory category =
          (error_code == 403)
              ? EnterpriseProxyErrorData::ErrorCategory::kAuthorization
              : EnterpriseProxyErrorData::ErrorCategory::kOther;
      MaybeRecordErrorForNavigation(navigation_id, destination_url, proxy_url,
                                    error_code, category);
      if (delegate) {
        delegate->AttachDisguisedErrorData(EnterpriseProxyErrorData(
            destination_url, proxy_url, error_code, category));
      }
      std::move(coord_callback).Run(std::nullopt);
      return;
    }
    case EnterpriseProxyService::ProxyAuthChallengeResult::kSignInRequired:
      MaybeRecordErrorForNavigation(
          navigation_id, destination_url, proxy_url, error_code,
          EnterpriseProxyErrorData::ErrorCategory::kAuthentication);
      if (delegate) {
        delegate->OnSignInRequired(destination_url);
      }
      std::move(coord_callback).Run(std::nullopt);
      return;
    case EnterpriseProxyService::ProxyAuthChallengeResult::
        kCredentialFetchFailure:
      MaybeRecordErrorForNavigation(
          navigation_id, destination_url, proxy_url, error_code,
          EnterpriseProxyErrorData::ErrorCategory::kOther);
      std::move(coord_callback).Run(std::nullopt);
      return;
    case EnterpriseProxyService::ProxyAuthChallengeResult::kNoCredentialsNeeded:
      std::move(coord_callback).Run(std::nullopt);
      return;
    case EnterpriseProxyService::ProxyAuthChallengeResult::
        kCredentialFetchSuccess:
      std::move(coord_callback).Run(credentials);
      return;
  }
}

}  // namespace enterprise_net
