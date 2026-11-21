// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/enterprise_proxy_error_service.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/enterprise/net/core/features.h"
#include "components/enterprise/net/core/utils.h"
#include "components/error_page/common/localized_error.h"
#include "components/strings/grit/components_strings.h"
#include "net/http/http_auth_scheme.h"
#include "net/http/http_status_code.h"
#include "net/log/net_log_event_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/url_constants.h"

namespace enterprise_net {

namespace {

constexpr std::string_view kUrlAuthParam = "url";

// Validates a static error page URL extracted from a disguised 407 challenge.
// `GURL` is safe to construct directly in the browser process under Chrome's
// Rule of 2 (`//docs/security/rule-of-2.md#safe-types-and-abstractions`),
// provided the parsed URL's semantics are validated:
// - Bounds input to `url::kMaxURLChars` and requires printable non-whitespace
//   ASCII so WHATWG canonicalization does not silently strip tabs/newlines or
//   percent-encode raw spaces.
// - Restricts to valid `https://` URLs without embedded credentials so a proxy
//   challenge can never redirect a tab to privileged schemes (`chrome://`,
//   `javascript:`, `data:`, `file:`, `chrome-extension:`), cleartext `http://`
//   pages, or `user:pass@` URLs.
std::optional<GURL> ValidateStaticErrorRedirectUrl(std::string_view raw_url) {
  if (raw_url.empty() || raw_url.size() > url::kMaxURLChars ||
      !std::ranges::all_of(raw_url, [](char c) {
        return base::IsAsciiPrintable(c) && !base::IsAsciiWhitespace(c);
      })) {
    return std::nullopt;
  }

  GURL url(raw_url);
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme) ||
      url.has_username() || url.has_password()) {
    return std::nullopt;
  }
  return url;
}

std::optional<GURL> ExtractDisguisedErrorRedirectUrl(
    const net::AuthChallengeInfo& auth_info,
    const scoped_refptr<net::HttpResponseHeaders>& response_headers) {
  if (!IsStaticErrorPageRedirectEnabled()) {
    return std::nullopt;
  }
  const bool has_raw_challenge =
      response_headers != nullptr || !auth_info.challenge.empty();
  if (auth_info.challenger.scheme() != url::kHttpsScheme) {
    if (has_raw_challenge) {
      RecordStaticErrorPageRedirectOutcome(
          SecureGatewayStaticErrorPageRedirectOutcome::kRejectedInvalidUrl);
    }
    return std::nullopt;
  }
  if (!has_raw_challenge) {
    return std::nullopt;
  }
  std::optional<BasicAuthChallengeParams> params =
      ExtractMatchingBasicAuthChallengeParams(auth_info,
                                              response_headers.get());
  if (!params.has_value()) {
    if (base::EqualsCaseInsensitiveASCII(auth_info.scheme,
                                         net::kBasicAuthScheme)) {
      RecordStaticErrorPageRedirectOutcome(
          SecureGatewayStaticErrorPageRedirectOutcome::
              kRejectedInvalidChallenge);
    }
    return std::nullopt;
  }
  auto url_it = params->find(kUrlAuthParam);
  if (url_it == params->end()) {
    return std::nullopt;
  }
  std::optional<GURL> validated_url =
      ValidateStaticErrorRedirectUrl(url_it->second);
  if (!validated_url.has_value()) {
    RecordStaticErrorPageRedirectOutcome(
        SecureGatewayStaticErrorPageRedirectOutcome::kRejectedInvalidUrl);
  }
  return validated_url;
}

}  // namespace

EnterpriseProxyErrorService::PendingInterception::PendingInterception(
    PendingInterception&&) = default;
EnterpriseProxyErrorService::PendingInterception&
EnterpriseProxyErrorService::PendingInterception::operator=(
    PendingInterception&&) = default;
EnterpriseProxyErrorService::PendingInterception::~PendingInterception() =
    default;

EnterpriseProxyErrorService::PendingInterception::PendingInterception(
    EnterpriseProxyService::ProxyAuthChallengeMatch match,
    int64_t navigation_id,
    GURL destination_url,
    GURL proxy_url,
    int error_code,
    std::optional<GURL> redirect_url)
    : match_(std::move(match)),
      navigation_id_(navigation_id),
      destination_url_(std::move(destination_url)),
      proxy_url_(std::move(proxy_url)),
      error_code_(error_code),
      redirect_url_(std::move(redirect_url)) {}

EnterpriseProxyErrorService::EnterpriseProxyErrorService(
    EnterpriseProxyService* enterprise_proxy_service)
    : enterprise_proxy_service_(enterprise_proxy_service) {
  CHECK(enterprise_proxy_service_);
}

EnterpriseProxyErrorService::~EnterpriseProxyErrorService() = default;

void EnterpriseProxyErrorService::Shutdown() {
  for (Observer& observer : observers_) {
    observer.OnEnterpriseProxyErrorServiceShuttingDown();
  }
  observers_.Clear();
  disguised_errors_.clear();
}

void EnterpriseProxyErrorService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void EnterpriseProxyErrorService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void EnterpriseProxyErrorService::RecordDisguisedError(
    int64_t navigation_id,
    EnterpriseProxyErrorData error_data,
    const net::NetLogWithSource& net_log) {
  if (navigation_id != 0) {
    net_log.AddEvent(
        net::NetLogEventType::ENTERPRISE_PROXY_DISGUISED_ERROR_SAVED, [&] {
          base::DictValue dict =
              base::DictValue()
                  .Set("navigation_id", base::NumberToString(navigation_id))
                  .Set("destination_url",
                       error_data.destination_url().possibly_invalid_spec())
                  .Set("proxy_url",
                       error_data.proxy_url().possibly_invalid_spec())
                  .Set("error_code", error_data.error_code());
          if (error_data.redirect_url().has_value()) {
            dict.Set("redirect_url",
                     error_data.redirect_url()->possibly_invalid_spec());
          }
          return dict;
        });
  }
  const EnterpriseProxyErrorData& stored_error =
      disguised_errors_.insert_or_assign(navigation_id, std::move(error_data))
          .first->second;
  if (navigation_id != 0 && !observers_.empty()) {
    // Copy before notifying, as observers may synchronously call
    // `RemoveDisguisedError(navigation_id)` when initiating a redirect.
    const EnterpriseProxyErrorData error_copy = stored_error;
    for (Observer& observer : observers_) {
      observer.OnDisguisedErrorRecorded(navigation_id, error_copy);
    }
  }
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
  params.Set("title", l10n_util::GetStringUTF16(
                          IDS_ENTERPRISE_PROXY_OTHER_ERROR_HEADING));
  params.Set("destination_url", error_data.destination_url().spec());
  params.Set("proxy_url", error_data.proxy_url().spec());
  params.Set("error_code", base::NumberToString(error_data.error_code()));
  params.Set(
      "error_category",
      base::NumberToString(static_cast<int>(error_data.error_category())));
  params.Set("is_enterprise_proxy_error", true);

  params.Set("heading", l10n_util::GetStringUTF16(
                            IDS_ENTERPRISE_PROXY_OTHER_ERROR_HEADING));
  params.Set("primary_paragraph",
             l10n_util::GetStringUTF16(
                 IDS_ENTERPRISE_PROXY_OTHER_ERROR_PRIMARY_PARAGRAPH));
  params.Set("button_text",
             l10n_util::GetStringUTF16(IDS_ENTERPRISE_BLOCK_GO_BACK));

  switch (error_data.error_category()) {
    case EnterpriseProxyErrorData::ErrorCategory::kAuthentication:
      params.Set("title", l10n_util::GetStringUTF16(
                              IDS_ENTERPRISE_PROXY_AUTHN_ERROR_HEADING));
      params.Set("heading", l10n_util::GetStringUTF16(
                                IDS_ENTERPRISE_PROXY_AUTHN_ERROR_HEADING));
      params.Set("primary_paragraph",
                 l10n_util::GetStringUTF16(
                     IDS_ENTERPRISE_PROXY_AUTHN_ERROR_PRIMARY_PARAGRAPH));
      params.Set("button_text", l10n_util::GetStringUTF16(IDS_CONTINUE));
      break;
    case EnterpriseProxyErrorData::ErrorCategory::kAuthorization:
      params.Set("title", l10n_util::GetStringUTF16(
                              IDS_ENTERPRISE_PROXY_AUTHZ_ERROR_HEADING));
      params.Set("heading", l10n_util::GetStringUTF16(
                                IDS_ENTERPRISE_PROXY_AUTHZ_ERROR_HEADING));
      params.Set("primary_paragraph",
                 l10n_util::GetStringFUTF16(
                     IDS_ENTERPRISE_PROXY_AUTHZ_ERROR_PRIMARY_PARAGRAPH,
                     base::EscapeForHTML(base::UTF8ToUTF16(
                         error_data.destination_url().spec()))));
      params.Set("button_text",
                 l10n_util::GetStringUTF16(IDS_ENTERPRISE_BLOCK_GO_BACK));
      break;
    default:
      // Corresponds to the "other" error cases. Strings are already populated
      // with defaults above.
      break;
  }

  return params;
}

std::optional<EnterpriseProxyErrorService::PendingInterception>
EnterpriseProxyErrorService::EvaluateProxyAuthChallenge(
    const net::AuthChallengeInfo& auth_info,
    const GURL& destination_url,
    const scoped_refptr<net::HttpResponseHeaders>& response_headers,
    int64_t navigation_id) {
  using Decision = EnterpriseProxyService::ProxyAuthChallengeMatch::Decision;

  EnterpriseProxyService::ProxyAuthChallengeMatch match =
      enterprise_proxy_service_->ClassifyProxyAuthChallenge(
          auth_info, destination_url, response_headers);

  if (match.decision == Decision::kNotApplicable) {
    return std::nullopt;
  }

  int error_code = 0;
  std::optional<GURL> redirect_url;
  std::optional<int> forced_error_code = GetForcedDisguisedErrorCode();
  if (forced_error_code.has_value()) {
    error_code = *forced_error_code;
  } else {
    base::StringToInt(auth_info.realm, &error_code);
    if (match.decision == Decision::kDisguisedError && navigation_id > 0) {
      redirect_url =
          ExtractDisguisedErrorRedirectUrl(auth_info, response_headers);
    }
  }

  return PendingInterception(std::move(match), navigation_id, destination_url,
                             auth_info.challenger.GetURL(), error_code,
                             std::move(redirect_url));
}

void EnterpriseProxyErrorService::ResolveProxyAuthChallenge(
    PendingInterception interception,
    ProxyAuthCredentialsCallback callback) {
  using Decision = EnterpriseProxyService::ProxyAuthChallengeMatch::Decision;

  // With no callback nobody is waiting on this challenge, so neither the
  // credential fetch nor the error-page bookkeeping would have a consumer.
  // Drop the interception rather than doing work for no one.
  if (!callback) {
    return;
  }

  switch (interception.match_.decision) {
    case Decision::kNotApplicable:
      // EvaluateProxyAuthChallenge() never mints an interception for this.
      NOTREACHED();

    case Decision::kNoCredentialsNeeded:
      // A PvD route matched, so this proxy is ours, but that route is not
      // configured to authenticate against it -- and the challenge is not a
      // disguised error either. Either the proxy should not have challenged, or
      // the policy is misconfigured; both are error cases.
      //
      // Fail closed with a generic error page. Silently cancelling auth would
      // surface the proxy's raw 407 body, and falling through to a login prompt
      // would ask the user for credentials that cannot possibly satisfy an
      // enterprise proxy. `error_code` is the challenge's own status rather
      // than a disguised one, since there is no disguised error to report.
      MaybeRecordErrorForNavigation(
          interception.navigation_id_, interception.destination_url_,
          interception.proxy_url_, net::HTTP_PROXY_AUTHENTICATION_REQUIRED,
          EnterpriseProxyErrorData::ErrorCategory::kOther,
          /*redirect_url=*/std::nullopt, interception.match_.net_log);
      std::move(callback).Run(std::nullopt);
      return;

    case Decision::kDisguisedError:
      MaybeRecordErrorForNavigation(
          interception.navigation_id_, interception.destination_url_,
          interception.proxy_url_, interception.error_code_,
          (interception.error_code_ == 403)
              ? EnterpriseProxyErrorData::ErrorCategory::kAuthorization
              : EnterpriseProxyErrorData::ErrorCategory::kOther,
          std::move(interception.redirect_url_), interception.match_.net_log);
      std::move(callback).Run(std::nullopt);
      return;

    case Decision::kNeedsCredentials:
      // FetchProxyAuthCredentials() guarantees its callback runs exactly once.
      // The continuation is weakly bound to `this`, so `callback` is dropped if
      // this service dies first -- see OnProxyAuthCredentialsFetched() for why
      // KeyedService teardown makes that unreachable in production.
      enterprise_proxy_service_->FetchProxyAuthCredentials(
          std::move(interception.match_),
          base::BindOnce(
              &EnterpriseProxyErrorService::OnProxyAuthCredentialsFetched,
              weak_ptr_factory_.GetWeakPtr(), interception.navigation_id_,
              interception.destination_url_, interception.proxy_url_,
              interception.error_code_, std::move(callback)));
      return;
  }
}

void EnterpriseProxyErrorService::RecordErrorCodeHistogram(
    int error_code) const {
  base::UmaHistogramSparse("Enterprise.Proxy.DisguisedErrorPage.ErrorCode",
                           error_code);
}

void EnterpriseProxyErrorService::MaybeRecordErrorForNavigation(
    int64_t navigation_id,
    const GURL& destination_url,
    const GURL& proxy_url,
    int error_code,
    EnterpriseProxyErrorData::ErrorCategory category,
    std::optional<GURL> redirect_url,
    const net::NetLogWithSource& net_log) {
  if (navigation_id <= 0) {
    return;
  }
  RecordDisguisedError(
      navigation_id,
      EnterpriseProxyErrorData(destination_url, proxy_url, error_code, category,
                               std::move(redirect_url)),
      net_log);
}

// Weakly bound, so `callback` is dropped if this service is destroyed while a
// fetch is still in flight. That window is unreachable in production:
// KeyedService teardown runs every Shutdown() before any destructor, and
// EnterpriseProxyService::Shutdown() flushes its pending requests, which runs
// this continuation while this service is still alive. Tests that destroy the
// services directly, without that Shutdown(), can still observe the drop.
void EnterpriseProxyErrorService::OnProxyAuthCredentialsFetched(
    int64_t navigation_id,
    const GURL& destination_url,
    const GURL& proxy_url,
    int error_code,
    ProxyAuthCredentialsCallback callback,
    EnterpriseProxyService::CredentialFetchOutcome outcome,
    const std::optional<net::AuthCredentials>& credentials,
    const net::NetLogWithSource& net_log) {
  switch (outcome) {
    case EnterpriseProxyService::CredentialFetchOutcome::kSuccess:
      std::move(callback).Run(credentials);
      return;

    case EnterpriseProxyService::CredentialFetchOutcome::kSignInRequired:
      MaybeRecordErrorForNavigation(
          navigation_id, destination_url, proxy_url, error_code,
          EnterpriseProxyErrorData::ErrorCategory::kAuthentication,
          /*redirect_url=*/std::nullopt, net_log);
      std::move(callback).Run(std::nullopt);
      return;

    case EnterpriseProxyService::CredentialFetchOutcome::kFailure:
      MaybeRecordErrorForNavigation(
          navigation_id, destination_url, proxy_url, error_code,
          EnterpriseProxyErrorData::ErrorCategory::kOther,
          /*redirect_url=*/std::nullopt, net_log);
      std::move(callback).Run(std::nullopt);
      return;
  }
}

}  // namespace enterprise_net
