// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/enterprise_proxy_error_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "components/enterprise/net/core/enterprise_proxy_error_data.h"
#include "components/enterprise/net/core/enterprise_proxy_service.h"

namespace enterprise_net {

EnterpriseProxyErrorService::EnterpriseProxyErrorService(
    EnterpriseProxyService* enterprise_proxy_service)
    : enterprise_proxy_service_(enterprise_proxy_service) {
  CHECK(enterprise_proxy_service_);
}

EnterpriseProxyErrorService::~EnterpriseProxyErrorService() = default;

bool EnterpriseProxyErrorService::InterceptProxyAuthChallenge(
    const net::AuthChallengeInfo& auth_info,
    const GURL& destination_url,
    const scoped_refptr<net::HttpResponseHeaders>& response_headers,
    std::unique_ptr<Delegate> delegate,
    base::OnceCallback<void(const std::optional<net::AuthCredentials>&)>
        callback) {
  bool is_handled = true;
  GURL proxy_url = auth_info.challenger.GetURL();
  int error_code = 0;
  base::StringToInt(auth_info.realm, &error_code);
  auto eps_callback = base::BindOnce(
      &EnterpriseProxyErrorService::OnProxyAuthChallengeResult,
      weak_ptr_factory_.GetWeakPtr(), &is_handled, std::move(delegate),
      destination_url, std::move(proxy_url), error_code, std::move(callback));

  enterprise_proxy_service_->HandleProxyAuthChallenge(
      auth_info, destination_url, response_headers, std::move(eps_callback));

  return is_handled;
}

void EnterpriseProxyErrorService::OnProxyAuthChallengeResult(
    bool* handled_flag,
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
    case EnterpriseProxyService::ProxyAuthChallengeResult::kDisguisedError:
      if (delegate) {
        delegate->AttachDisguisedErrorData(
            EnterpriseProxyErrorData(destination_url, proxy_url, error_code));
      }
      std::move(coord_callback).Run(std::nullopt);
      return;
    case EnterpriseProxyService::ProxyAuthChallengeResult::kNoCredentialsNeeded:
    case EnterpriseProxyService::ProxyAuthChallengeResult::
        kCredentialFetchFailure:
      std::move(coord_callback).Run(std::nullopt);
      return;
    case EnterpriseProxyService::ProxyAuthChallengeResult::
        kCredentialFetchSuccess:
      std::move(coord_callback).Run(credentials);
      return;
  }
}

}  // namespace enterprise_net
