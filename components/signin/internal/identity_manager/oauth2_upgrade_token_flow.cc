// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/oauth2_upgrade_token_flow.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace signin {

namespace {

std::string UpgradeTypeToString(
    switches::RefreshTokenBindingUpgradeType upgrade_type) {
  switch (upgrade_type) {
    case switches::RefreshTokenBindingUpgradeType::kDarkLaunch:
      return "DARK_LAUNCH_BIND_TO_KEY";
    case switches::RefreshTokenBindingUpgradeType::kLiveLaunch:
      return "BIND_TO_KEY";
  }
  NOTREACHED();
}

}  // namespace

OAuth2UpgradeTokenFlow::OAuth2UpgradeTokenFlow(
    std::string refresh_token,
    switches::RefreshTokenBindingUpgradeType upgrade_type,
    std::string device_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::OnceClosure callback)
    : refresh_token_(std::move(refresh_token)),
      upgrade_type_(upgrade_type),
      device_id_(std::move(device_id)),
      url_loader_factory_(std::move(url_loader_factory)),
      callback_(std::move(callback)) {
  CHECK(!refresh_token_.empty());
  CHECK(url_loader_factory_);
  CHECK(callback_);
}

OAuth2UpgradeTokenFlow::~OAuth2UpgradeTokenFlow() = default;

void OAuth2UpgradeTokenFlow::StartWithRegistrationToken(
    std::string binding_registration_token) {
  binding_registration_token_ = std::move(binding_registration_token);
  Start(url_loader_factory_, refresh_token_);
}

void OAuth2UpgradeTokenFlow::AbortWithError(
    OAuth2UpgradeTokenFlowResult result) {
  CHECK_NE(result, OAuth2UpgradeTokenFlowResult::kSuccess);
  ProcessResult(result, std::nullopt);
}

GURL OAuth2UpgradeTokenFlow::CreateApiCallUrl() {
  return GaiaUrls::GetInstance()->oauth2_upgrade_token_url();
}

std::string OAuth2UpgradeTokenFlow::CreateApiCallBody() {
  auto dict =
      base::DictValue()
          .Set("token", refresh_token_)
          .Set("upgradeType", UpgradeTypeToString(upgrade_type_))
          .Set("tokenBindingRegistrationJwt", binding_registration_token_);
  if (!device_id_.empty()) {
    dict.Set("deviceId", device_id_);
  }

  return base::WriteJson(dict).value_or("");
}

std::string OAuth2UpgradeTokenFlow::CreateApiCallBodyContentType() {
  return "application/json";
}

void OAuth2UpgradeTokenFlow::ProcessApiCallSuccess(
    const network::mojom::URLResponseHead* head,
    std::optional<std::string> body) {
  ProcessResult(OAuth2UpgradeTokenFlowResult::kSuccess,
                (head && head->headers) ? head->headers->response_code() : 0);
}

void OAuth2UpgradeTokenFlow::ProcessApiCallFailure(
    int net_error,
    const network::mojom::URLResponseHead* head,
    std::optional<std::string> body) {
  if (net_error != net::OK) {
    ProcessResult(OAuth2UpgradeTokenFlowResult::kNetworkError, net_error);
  } else {
    ProcessResult(OAuth2UpgradeTokenFlowResult::kServerError,
                  (head && head->headers) ? head->headers->response_code() : 0);
  }
}

void OAuth2UpgradeTokenFlow::ProcessResult(
    OAuth2UpgradeTokenFlowResult result,
    std::optional<int> net_error_or_http_code) {
  if (net_error_or_http_code.has_value()) {
    base::UmaHistogramSparse("Signin.TokenBinding.UpgradeHttpResult",
                             net_error_or_http_code.value());
  }
  base::UmaHistogramEnumeration("Signin.TokenBinding.UpgradeResult", result);
  base::UmaHistogramMediumTimes("Signin.TokenBinding.UpgradeDuration",
                                base::TimeTicks::Now() - start_time_);

  if (callback_) {
    std::move(callback_).Run();
    // `this` may be destroyed.
  }
}

net::PartialNetworkTrafficAnnotationTag
OAuth2UpgradeTokenFlow::GetNetworkTrafficAnnotationTag() {
  return net::DefinePartialNetworkTrafficAnnotation("oauth2_upgrade_token_flow",
                                                    "oauth2_api_call_flow", R"(
      semantics {
        sender: "Chrome Identity API"
        description:
          "Upgrades an unbound Google OAuth2 refresh token to a device-bound "
          "one. Device-bound token cannot be used outside of this device"
          "which helps protect against credential theft."
        trigger:
          "Server challenge in OAuth2 access token mint flow. Happens whenever "
          "Chrome needs to mint a new access token from an unbound refresh "
          "token to provide authenticated access to Google services at a "
          "browser level."
        data:
          "User's refresh token, device id and binding key registration token."
        destination: GOOGLE_OWNED_SERVICE
        user_data {
          type: ACCESS_TOKEN
          type: DEVICE_ID
        }
        last_reviewed: "2026-05-24"
        internal {
          contacts {
            email: "alexilin@chromium.org"
          }
          contacts {
            email: "ahijazi@chromium.org"
          }
          contacts {
            email: "chrome-signin-team@google.com"
          }
        }
      }
      policy {
        setting:
          "This feature cannot be disabled by settings, however the request is "
          "made only for signed-in users."
        chrome_policy {
          BrowserSignin {
            policy_options {mode: MANDATORY}
            BrowserSignin: 0
          }
        }
      })");
}

std::string OAuth2UpgradeTokenFlow::CreateAuthorizationHeaderValue(
    const std::string& access_token) {
  // This flow doesn't use the Authorization header.
  return "";
}

}  // namespace signin
