// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_OAUTH2_UPGRADE_TOKEN_FLOW_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_OAUTH2_UPGRADE_TOKEN_FLOW_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/signin/public/base/signin_switches.h"
#include "google_apis/gaia/oauth2_api_call_flow.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

class GURL;

namespace signin {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(OAuth2UpgradeTokenFlowResult)
enum class OAuth2UpgradeTokenFlowResult {
  kSuccess = 0,
  kNetworkError = 1,
  kServerError = 2,
  kTokenGenerationFailure = 3,
  kFailedToSaveBindingKey = 4,
  kMaxValue = kFailedToSaveBindingKey
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:OAuth2UpgradeTokenFlowResult)

class OAuth2UpgradeTokenFlow : public OAuth2ApiCallFlow {
 public:
  OAuth2UpgradeTokenFlow(
      std::string refresh_token,
      switches::RefreshTokenBindingUpgradeType upgrade_type,
      std::string device_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::OnceClosure callback);

  OAuth2UpgradeTokenFlow(const OAuth2UpgradeTokenFlow&) = delete;
  OAuth2UpgradeTokenFlow& operator=(const OAuth2UpgradeTokenFlow&) = delete;

  ~OAuth2UpgradeTokenFlow() override;

  const std::string& refresh_token() const { return refresh_token_; }

  void StartWithRegistrationToken(std::string binding_registration_token);
  void AbortWithError(OAuth2UpgradeTokenFlowResult result);

 protected:
  // OAuth2ApiCallFlow:
  GURL CreateApiCallUrl() override;
  std::string CreateApiCallBody() override;
  std::string CreateApiCallBodyContentType() override;
  void ProcessApiCallSuccess(const network::mojom::URLResponseHead* head,
                             std::optional<std::string> body) override;
  void ProcessApiCallFailure(int net_error,
                             const network::mojom::URLResponseHead* head,
                             std::optional<std::string> body) override;
  net::PartialNetworkTrafficAnnotationTag GetNetworkTrafficAnnotationTag()
      override;
  std::string CreateAuthorizationHeaderValue(
      const std::string& access_token) override;

 private:
  void ProcessResult(OAuth2UpgradeTokenFlowResult result,
                     std::optional<int> net_error_or_http_code);

  const std::string refresh_token_;
  const switches::RefreshTokenBindingUpgradeType upgrade_type_;
  const std::string device_id_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  const base::TimeTicks start_time_{base::TimeTicks::Now()};

  std::string binding_registration_token_;

  base::OnceClosure callback_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_OAUTH2_UPGRADE_TOKEN_FLOW_H_
