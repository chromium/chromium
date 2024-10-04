// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_manifest_downloader.h"

#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/link_header_util/link_header_util.h"
#include "components/payments/core/csp_checker.h"
#include "components/payments/core/error_logger.h"
#include "components/payments/core/features.h"
#include "components/payments/core/native_error_strings.h"
#include "components/payments/core/url_util.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/url_constants.h"

namespace payments {
namespace {

static constexpr size_t kMaxManifestSize = 5 * 1024 * 1024;
static_assert(kMaxManifestSize <=
                  network::SimpleURLLoader::kMaxBoundedStringDownloadSize,
              "Max manifest size bigger than largest allowed download size");

void RespondWithHttpStatusCodeError(const GURL& final_url,
                                    net::HttpStatusCode http_status_code,
                                    const ErrorLogger& log,
                                    PaymentManifestDownloadCallback callback) {
  std::string error_message = base::ReplaceStringPlaceholders(
      errors::kPaymentManifestDownloadFailedWithHttpStatusCode,
      {final_url.spec(), base::NumberToString(http_status_code),
       net::GetHttpReasonPhrase(http_status_code)},
      nullptr);
  log.Error(error_message);
  std::move(callback).Run(final_url, std::string(), error_message);
}

// Invokes |callback| with |error_format|.
void RespondWithError(std::string_view error_format,
                      const GURL& final_url,
                      const ErrorLogger& log,
                      PaymentManifestDownloadCallback callback) {
  std::string error_message = base::ReplaceStringPlaceholders(
      error_format, {final_url.spec()}, nullptr);
  log.Error(error_message);
  std::move(callback).Run(final_url, std::string(), error_message);
}

// Invokes the |callback| with |response_body|. If |response_body| is empty,
// then invokes |callback| with |empty_error_format|.
void RespondWithContent(const std::string& response_body,
                        std::string_view empty_error_format,
                        const GURL& final_url,
                        const ErrorLogger& log,
                        PaymentManifestDownloadCallback callback) {
  if (response_body.empty()) {
    RespondWithError(empty_error_format, final_url, log, std::move(callback));
  } else {
    std::move(callback).Run(final_url, response_body, std::string());
  }
}

bool IsValidManifestUrl(const GURL& url,
                        const ErrorLogger& log,
                        std::string* out_error_message) {
  bool is_valid = UrlUtil::IsValidManifestUrl(url);
  if (!is_valid) {
    *out_error_message = base::ReplaceStringPlaceholders(
        errors::kInvalidManifestUrl, {url.spec()}, nullptr);
    log.Error(*out_error_message);
  }
  return is_valid;
}

GURL ParseRedirectUrl(const net::RedirectInfo& redirect_info,
                      const GURL& original_url,
                      const ErrorLogger& log,
                      std::string* out_error_message) {
  if (redirect_info.status_code != net::HTTP_MOVED_PERMANENTLY &&   // 301
      redirect_info.status_code != net::HTTP_FOUND &&               // 302
      redirect_info.status_code != net::HTTP_SEE_OTHER &&           // 303
      redirect_info.status_code != net::HTTP_TEMPORARY_REDIRECT &&  // 307
      redirect_info.status_code != net::HTTP_PERMANENT_REDIRECT) {  // 308
    *out_error_message = base::ReplaceStringPlaceholders(
        errors::kHttpStatusCodeNotAllowed,
        {base::NumberToString(redirect_info.status_code),
         net::GetHttpReasonPhrase(
             static_cast<net::HttpStatusCode>(redirect_info.status_code)),
         original_url.spec()},
        nullptr);
    log.Error(*out_error_message);
    return GURL();
  }

  if (!IsValidManifestUrl(redirect_info.new_url, log, out_error_message)) {
    return GURL();
  }

  return redirect_info.new_url;
}

}  // namespace

PaymentManifestDownloader::PaymentManifestDownloader(
    std::unique_ptr<ErrorLogger> log,
    base::WeakPtr<CSPChecker> csp_checker,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : log_(std::move(log)),
      csp_checker_(csp_checker),
      url_loader_factory_(std::move(url_loader_factory)) {
  DCHECK(log_);
  DCHECK(url_loader_factory_);
}

PaymentManifestDownloader::~PaymentManifestDownloader() = default;

void PaymentManifestDownloader::DownloadPaymentMethodManifest(
    const url::Origin& merchant_origin,
    const GURL& url,
    PaymentManifestDownloadCallback callback) {
  DCHECK(UrlUtil::IsValidManifestUrl(url));
  // Restrict number of redirects for efficiency and breaking circle.
  InitiateDownload(merchant_origin, url, /*url_before_redirects=*/url,
                   /*did_follow_redirect=*/false,
                   Download::Type::LINK_HEADER_WITH_FALLBACK_TO_RESPONSE_BODY,
                   /*allowed_number_of_redirects=*/3, std::move(callback));
}

void PaymentManifestDownloader::DownloadWebAppManifest(
    const url::Origin& payment_method_manifest_origin,
    const GURL& url,
    PaymentManifestDownloadCallback callback) {
  DCHECK(UrlUtil::IsValidManifestUrl(url));
  InitiateDownload(payment_method_manifest_origin, url,
                   /*url_before_redirects=*/url,
                   /*did_follow_redirect=*/false, Download::Type::RESPONSE_BODY,
                   /*allowed_number_of_redirects=*/0, std::move(callback));
}

GURL PaymentManifestDownloader::FindTestServerURL(const GURL& url) const {
  return url;
}

void PaymentManifestDownloader::SetCSPCheckerForTesting(
    base::WeakPtr<CSPChecker> csp_checker) {
  NOTREACHED_IN_MIGRATION();
}

PaymentManifestDownloader::Download::Download() = default;

PaymentManifestDownloader::Download::~Download() = default;

bool PaymentManifestDownloader::Download::IsLinkHeaderDownload() const {
  return type == Type::LINK_HEADER_WITH_FALLBACK_TO_RESPONSE_BODY;
}

bool PaymentManifestDownloader::Download::IsResponseBodyDownload() const {
  return type == Type::FALLBACK_TO_RESPONSE_BODY || type == Type::RESPONSE_BODY;
}

void PaymentManifestDownloader::OnURLLoaderRedirect(
    network::SimpleURLLoader* url_loader,
    const GURL& url_before_redirect,
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* to_be_removed_headers) {
  auto download_it = downloads_.find(url_loader);
  CHECK(download_it != downloads_.end(), base::NotFatalUntil::M130);

  std::unique_ptr<Download> download = std::move(download_it->second);
  downloads_.erase(download_it);

  // Manually follow some type of redirects.
  std::string error_message;
  if (download->allowed_number_of_redirects > 0) {
    DCHECK(download->IsLinkHeaderDownload());
    GURL redirect_url = ParseRedirectUrl(redirect_info, download->original_url,
                                         *log_, &error_message);
    if (!redirect_url.is_empty()) {
      // Do not allow cross site redirects.
      if (net::registry_controlled_domains::SameDomainOrHost(
              download->original_url, redirect_url,
              net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
        // Redirects preserve the original request initiator.
        InitiateDownload(
            download->request_initiator, redirect_url,
            /*url_before_redirects=*/download->url_before_redirects,
            /*did_follow_redirect=*/true,
            Download::Type::LINK_HEADER_WITH_FALLBACK_TO_RESPONSE_BODY,
            --download->allowed_number_of_redirects,
            std::move(download->callback));
        return;
      }
      error_message = base::ReplaceStringPlaceholders(
          errors::kPaymentManifestCrossSiteRedirectNotAllowed,
          {download->original_url.spec(), redirect_url.spec()}, nullptr);
    }
  } else {
    error_message = errors::kReachedMaximumNumberOfRedirects;
  }
  log_->Error(error_message);
  std::move(download->callback)
      .Run(download->original_url, std::string(), error_message);
}

void PaymentManifestDownloader::OnURLLoaderComplete(
    network::SimpleURLLoader* url_loader,
    std::unique_ptr<std::string> response_body) {
  scoped_refptr<net::HttpResponseHeaders> headers;
  if (url_loader->ResponseInfo()) {
    headers = url_loader->ResponseInfo()->headers;
  }

  std::string response_body_str;
  if (response_body.get()) {
    response_body_str = std::move(*response_body);
  }

  OnURLLoaderCompleteInternal(url_loader, url_loader->GetFinalURL(),
                              response_body_str, headers,
                              url_loader->NetError());
}

void PaymentManifestDownloader::OnURLLoaderCompleteInternal(
    network::SimpleURLLoader* url_loader,
    const GURL& final_url,
    const std::string& response_body,
    scoped_refptr<net::HttpResponseHeaders> headers,
    int net_error) {
  auto download_it = downloads_.find(url_loader);
  CHECK(download_it != downloads_.end(), base::NotFatalUntil::M130);

  std::unique_ptr<Download> download = std::move(download_it->second);
  downloads_.erase(download_it);

  if (net_error != net::OK &&
      net_error != net::ERR_HTTP_RESPONSE_CODE_FAILURE) {
    std::string error_message = base::ReplaceStringPlaceholders(
        errors::kPaymentManifestDownloadFailedWithNetworkError,
        {final_url.spec(), net::ErrorToShortString(net_error),
         base::NumberToString(net_error)},
        nullptr);
    log_->Error(error_message);
    std::move(download->callback).Run(final_url, std::string(), error_message);
    return;
  }

  std::string error_message;
  if (download->IsResponseBodyDownload()) {
    if (headers && headers->response_code() != net::HTTP_OK) {
      RespondWithHttpStatusCodeError(
          final_url, static_cast<net::HttpStatusCode>(headers->response_code()),
          *log_, std::move(download->callback));
    } else {
      RespondWithContent(
          response_body,
          download->type == Download::Type::FALLBACK_TO_RESPONSE_BODY
              ? errors::kNoContentAndNoLinkHeader
              : errors::kNoContentInPaymentManifest,
          final_url, *log_, std::move(download->callback));
    }
    return;
  }

  DCHECK(download->IsLinkHeaderDownload());

  if (!headers) {
    // HTTP HEAD response has no headers; possibly fallback to HTTP GET.
    TryFallbackToDownloadingResponseBody(final_url, std::move(download));
    return;
  }

  if (headers->response_code() != net::HTTP_OK &&
      headers->response_code() != net::HTTP_NO_CONTENT) {
    RespondWithHttpStatusCodeError(
        final_url, static_cast<net::HttpStatusCode>(headers->response_code()),
        *log_, std::move(download->callback));
    return;
  }

  std::string link_header;
  headers->GetNormalizedHeader("link", &link_header);
  if (link_header.empty()) {
    // HTTP HEAD response has no Link header; possibly fallback to HTTP GET.
    TryFallbackToDownloadingResponseBody(final_url, std::move(download));
    return;
  }

  for (const auto& value : link_header_util::SplitLinkHeader(link_header)) {
    std::string link_url;
    std::unordered_map<std::string, std::optional<std::string>> params;
    if (!link_header_util::ParseLinkHeaderValue(value.first, value.second,
                                                &link_url, &params)) {
      continue;
    }

    auto rel = params.find("rel");
    if (rel == params.end()) {
      continue;
    }

    std::vector<std::string> rel_parts =
        base::SplitString(rel->second.value_or(""), HTTP_LWS,
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (base::Contains(rel_parts, "payment-method-manifest")) {
      GURL payment_method_manifest_url = final_url.Resolve(link_url);

      if (!IsValidManifestUrl(payment_method_manifest_url, *log_,
                              &error_message)) {
        std::move(download->callback)
            .Run(final_url, std::string(), error_message);
        return;
      }

      if (!url::IsSameOriginWith(final_url, payment_method_manifest_url)) {
        error_message = base::ReplaceStringPlaceholders(
            errors::kCrossOriginPaymentMethodManifestNotAllowed,
            {payment_method_manifest_url.spec(), final_url.spec()}, nullptr);
        log_->Error(error_message);
        std::move(download->callback)
            .Run(final_url, std::string(), error_message);
        return;
      }

      // The request initiator for the payment method manifest is the origin of
      // the GET request with the HTTP link header.
      // https://github.com/w3c/webappsec-fetch-metadata/issues/30
      InitiateDownload(
          url::Origin::Create(final_url), payment_method_manifest_url,
          /*url_before_redirects=*/download->url_before_redirects,
          /*did_follow_redirect=*/false, Download::Type::RESPONSE_BODY,
          /*allowed_number_of_redirects=*/0, std::move(download->callback));
      return;
    }
  }

  // HTTP HEAD response has no Link header that has a
  // rel="payment-method-manifest" entry; possibly fallback to HTTP GET.
  TryFallbackToDownloadingResponseBody(final_url, std::move(download));
}

void PaymentManifestDownloader::TryFallbackToDownloadingResponseBody(
    const GURL& url_to_download,
    std::unique_ptr<Download> download_info) {
  if (base::FeatureList::IsEnabled(
          features::kPaymentHandlerRequireLinkHeader)) {
    // Not allowed to fallback, because the payment method manifest load must
    // have a Link header.
    std::string error_message = base::ReplaceStringPlaceholders(
        errors::kNoLinkHeader, {url_to_download.spec()}, nullptr);
    log_->Error(error_message);
    std::move(download_info->callback)
        .Run(url_to_download, std::string(), error_message);
  } else {
    InitiateDownload(
        /*request_initiator=*/download_info->request_initiator,
        /*url=*/url_to_download,
        /*url_before_redirects=*/download_info->url_before_redirects,
        /*did_follow_redirect=*/download_info->did_follow_redirect,
        /*download_type=*/Download::Type::FALLBACK_TO_RESPONSE_BODY,
        /*allowed_number_of_redirects=*/0,
        /*callback=*/std::move(download_info->callback));
  }
}

network::SimpleURLLoader* PaymentManifestDownloader::GetLoaderForTesting() {
  CHECK_EQ(downloads_.size(), 1u);
  return downloads_.begin()->second->loader.get();
}

GURL PaymentManifestDownloader::GetLoaderOriginalURLForTesting() {
  CHECK_EQ(downloads_.size(), 1u);
  return downloads_.begin()->second->original_url;
}

void PaymentManifestDownloader::InitiateDownload(
    const url::Origin& request_initiator,
    const GURL& url,
    const GURL& url_before_redirects,
    bool did_follow_redirect,
    Download::Type download_type,
    int allowed_number_of_redirects,
    PaymentManifestDownloadCallback callback) {
  DCHECK(UrlUtil::IsValidManifestUrl(url));

  // Only initial download of the payment method manifest (which might contain
  // an HTTP Link header) is allowed to redirect.
  DCHECK(allowed_number_of_redirects == 0 ||
         download_type ==
             Download::Type::LINK_HEADER_WITH_FALLBACK_TO_RESPONSE_BODY);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("payment_manifest_downloader", R"(
        semantics {
          sender: "Web Payments"
          description:
            "Chromium downloads manifest files for web payments API to help "
            "users make secure and convenient payments on the web."
          trigger:
            "A user that has a payment app visits a website that uses the web "
            "payments API."
          data: "None."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings. Users can uninstall/"
            "disable all payment apps to stop this feature."
          policy_exception_justification: "Not implemented."
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->request_initiator = request_initiator;
  resource_request->url = url;

  switch (download_type) {
    case Download::Type::LINK_HEADER_WITH_FALLBACK_TO_RESPONSE_BODY:
      resource_request->method = net::HttpRequestHeaders::kHeadMethod;
      break;
    case Download::Type::FALLBACK_TO_RESPONSE_BODY:
    // Intentional fall through.
    case Download::Type::RESPONSE_BODY:
      resource_request->method = net::HttpRequestHeaders::kGetMethod;
      break;
  }
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);

  auto download = std::make_unique<Download>();
  download->request_initiator = request_initiator;
  download->type = download_type;
  download->original_url = url;
  download->url_before_redirects = url_before_redirects;
  download->did_follow_redirect = did_follow_redirect;
  download->loader = std::move(loader);
  download->callback = std::move(callback);
  download->allowed_number_of_redirects = allowed_number_of_redirects;

  if (!csp_checker_) {  // Can be null when the webpage closes.
    RespondWithError(errors::kPaymentManifestDownloadFailed,
                     download->original_url, *log_,
                     std::move(download->callback));
    return;
  }

  csp_checker_->AllowConnectToSource(
      url, url_before_redirects, did_follow_redirect,
      base::BindOnce(&PaymentManifestDownloader::OnCSPCheck,
                     weak_ptr_factory_.GetWeakPtr(), std::move(download)));
}

void PaymentManifestDownloader::OnCSPCheck(std::unique_ptr<Download> download,
                                           bool csp_allowed) {
  if (!csp_allowed) {
    RespondWithError(errors::kPaymentManifestCSPDenied, download->original_url,
                     *log_, std::move(download->callback));
    return;
  }

  network::SimpleURLLoader* loader = download->loader.get();
  loader->SetOnRedirectCallback(
      base::BindRepeating(&PaymentManifestDownloader::OnURLLoaderRedirect,
                          weak_ptr_factory_.GetWeakPtr(), loader));
  loader->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&PaymentManifestDownloader::OnURLLoaderComplete,
                     weak_ptr_factory_.GetWeakPtr(), loader),
      kMaxManifestSize);

  auto insert_result =
      downloads_.insert(std::make_pair(loader, std::move(download)));
  DCHECK(insert_result.second);  // Whether the insert has succeeded.
}

}  // namespace payments
