// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_manifest_downloader.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/link_header_util/link_header_util.h"
#include "components/payments/core/csp_checker.h"
#include "components/payments/core/error_logger.h"
#include "components/payments/core/error_message_util.h"
#include "components/payments/core/features.h"
#include "components/payments/core/native_error_strings.h"
#include "components/payments/core/url_util.h"
#include "content/public/browser/connection_allowlist_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "mojo/public/cpp/bindings/remote.h"
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
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/url_constants.h"

namespace payments {
namespace {

static constexpr size_t kMaxManifestSize = 5 * 1024 * 1024;
static_assert(kMaxManifestSize <=
                  network::SimpleURLLoader::kMaxBoundedStringDownloadSize,
              "Max manifest size bigger than largest allowed download size");

// Returns a generic client-facing error message for manifest download failures
// using |url_before_redirects|.
std::string CreateManifestDownloadFailureMessage(
    const GURL& url_before_redirects) {
  return base::ReplaceStringPlaceholders(errors::kPaymentManifestDownloadFailed,
                                         {url_before_redirects.spec()},
                                         nullptr);
}

// Invokes |callback| with a generic failure message using
// |url_before_redirects| and logs |devtools_error_message| to |log|.
void RespondWithError(const std::string& devtools_error_message,
                      const GURL& url_before_redirects,
                      const GURL& final_url,
                      const ErrorLogger& log,
                      PaymentManifestDownloadCallback callback) {
  log.Error(devtools_error_message);
  std::move(callback).Run(
      final_url, /*contents=*/std::string(),
      CreateManifestDownloadFailureMessage(url_before_redirects));
}

// Invokes |callback| with a generic failure message using
// |url_before_redirects| and logs |devtools_error_format| formatted with
// |final_url| to |log|.
void RespondWithTemplateError(std::string_view devtools_error_format,
                              const GURL& url_before_redirects,
                              const GURL& final_url,
                              const ErrorLogger& log,
                              PaymentManifestDownloadCallback callback) {
  RespondWithError(base::ReplaceStringPlaceholders(devtools_error_format,
                                                   {final_url.spec()}, nullptr),
                   url_before_redirects, final_url, log, std::move(callback));
}

// Invokes |callback| with a generic failure message using
// |url_before_redirects| and logs an HTTP status code error with |final_url| to
// |log|.
void RespondWithHttpStatusCodeError(const GURL& url_before_redirects,
                                    const GURL& final_url,
                                    int response_code,
                                    const ErrorLogger& log,
                                    PaymentManifestDownloadCallback callback) {
  RespondWithError(GenerateHttpStatusCodeError(final_url, response_code),
                   url_before_redirects, final_url, log, std::move(callback));
}

// Invokes |callback| with a generic failure message using
// |url_before_redirects| and logs a network error message with |final_url| to
// |log|.
void RespondWithNetworkError(const GURL& url_before_redirects,
                             const GURL& final_url,
                             int net_error,
                             const ErrorLogger& log,
                             PaymentManifestDownloadCallback callback) {
  RespondWithError(GenerateNetworkErrorMessage(final_url, net_error),
                   url_before_redirects, final_url, log, std::move(callback));
}

// Invokes |callback| with |response_body|. If |response_body| is empty, invokes
// |RespondWithTemplateError| with |empty_error_format|.
void RespondWithContent(const std::string& response_body,
                        std::string_view empty_error_format,
                        const GURL& url_before_redirects,
                        const GURL& final_url,
                        const ErrorLogger& log,
                        PaymentManifestDownloadCallback callback) {
  if (response_body.empty()) {
    RespondWithTemplateError(empty_error_format, url_before_redirects,
                             final_url, log, std::move(callback));
  } else {
    std::move(callback).Run(final_url, response_body,
                            /*error_message=*/std::string());
  }
}

bool IsValidManifestUrl(const GURL& url, const ErrorLogger& log) {
  bool is_valid = UrlUtil::IsValidManifestUrl(url);
  if (!is_valid) {
    log.Error(base::ReplaceStringPlaceholders(errors::kInvalidManifestUrl,
                                              {url.spec()}, nullptr));
  }
  return is_valid;
}

GURL ParseRedirectUrl(const net::RedirectInfo& redirect_info,
                      const GURL& original_url,
                      const ErrorLogger& log) {
  if (redirect_info.status_code != net::HTTP_MOVED_PERMANENTLY &&   // 301
      redirect_info.status_code != net::HTTP_FOUND &&               // 302
      redirect_info.status_code != net::HTTP_SEE_OTHER &&           // 303
      redirect_info.status_code != net::HTTP_TEMPORARY_REDIRECT &&  // 307
      redirect_info.status_code != net::HTTP_PERMANENT_REDIRECT) {  // 308
    log.Error(base::ReplaceStringPlaceholders(
        errors::kHttpStatusCodeNotAllowed,
        {base::NumberToString(redirect_info.status_code),
         std::string(net::GetHttpReasonPhrase(redirect_info.status_code)),
         original_url.spec()},
        nullptr));
    return GURL();
  }

  if (!IsValidManifestUrl(redirect_info.new_url, log)) {
    return GURL();
  }

  return redirect_info.new_url;
}

}  // namespace

PaymentManifestDownloader::PaymentManifestDownloader(
    std::unique_ptr<ErrorLogger> log,
    base::WeakPtr<CSPChecker> csp_checker,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_rfh,
    content::WeakDocumentPtr initiator_document)
    : log_(std::move(log)),
      csp_checker_(csp_checker),
      initiator_document_(std::move(initiator_document)),
      url_loader_factory_(std::move(url_loader_factory)),
      url_loader_factory_rfh_(std::move(url_loader_factory_rfh)) {
  CHECK(log_);
  CHECK(url_loader_factory_);
  CHECK(url_loader_factory_rfh_.is_bound());
}

PaymentManifestDownloader::~PaymentManifestDownloader() = default;

void PaymentManifestDownloader::DownloadPaymentMethodManifest(
    const url::Origin& merchant_origin,
    const GURL& url,
    PaymentManifestDownloadCallback callback) {
  DCHECK(UrlUtil::IsValidManifestUrl(url));
  // Restrict number of redirects for efficiency and breaking circle.
  InitiateDownload(merchant_origin, url, /*url_before_redirects=*/url,
                   /*did_follow_redirect=*/false, Download::Type::LINK_HEADER,
                   /*allowed_number_of_redirects=*/3,
                   /*use_url_loader_factory_rfh=*/true, std::move(callback));
}

void PaymentManifestDownloader::DownloadWebAppManifest(
    const url::Origin& payment_method_manifest_origin,
    const GURL& url,
    PaymentManifestDownloadCallback callback) {
  DCHECK(UrlUtil::IsValidManifestUrl(url));
  InitiateDownload(payment_method_manifest_origin, url,
                   /*url_before_redirects=*/url,
                   /*did_follow_redirect=*/false, Download::Type::RESPONSE_BODY,
                   /*allowed_number_of_redirects=*/0,
                   /*use_url_loader_factory_rfh=*/false, std::move(callback));
}

GURL PaymentManifestDownloader::FindTestServerURL(const GURL& url) const {
  return url;
}

void PaymentManifestDownloader::SetCSPCheckerForTesting(
    base::WeakPtr<CSPChecker> csp_checker) {
  NOTREACHED();
}

PaymentManifestDownloader::Download::Download() = default;

PaymentManifestDownloader::Download::~Download() = default;

bool PaymentManifestDownloader::Download::IsLinkHeaderDownload() const {
  return type == Type::LINK_HEADER;
}

bool PaymentManifestDownloader::Download::IsResponseBodyDownload() const {
  return type == Type::RESPONSE_BODY;
}

void PaymentManifestDownloader::OnURLLoaderRedirect(
    network::SimpleURLLoader* url_loader,
    const GURL& url_before_redirect,
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* to_be_removed_headers) {
  auto download_it = downloads_.find(url_loader);
  CHECK(download_it != downloads_.end());

  std::unique_ptr<Download> download = std::move(download_it->second);
  downloads_.erase(download_it);

  // Manually follow some type of redirects.
  if (download->allowed_number_of_redirects > 0) {
    DCHECK(download->IsLinkHeaderDownload());
    GURL redirect_url =
        ParseRedirectUrl(redirect_info, download->original_url, *log_);
    if (!redirect_url.is_empty()) {
      // Do not allow cross site redirects.
      if (net::registry_controlled_domains::SameDomainOrHost(
              download->original_url, redirect_url,
              net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
        // Redirects preserve the original request initiator.
        InitiateDownload(
            download->request_initiator, redirect_url,
            /*url_before_redirects=*/download->url_before_redirects,
            /*did_follow_redirect=*/true, Download::Type::LINK_HEADER,
            --download->allowed_number_of_redirects,
            // Use the SharedURLLoaderFactory for redirects as the RFH one
            // strips the headers. See crbug.com/520035382 for details.
            /*use_url_loader_factory_rfh=*/false,
            std::move(download->callback));
        return;
      }
      log_->Error(base::ReplaceStringPlaceholders(
          errors::kPaymentManifestCrossSiteRedirectNotAllowed,
          {download->original_url.spec(), redirect_url.spec()}, nullptr));
    }
  } else {
    log_->Error(errors::kReachedMaximumNumberOfRedirects);
  }
  std::move(download->callback)
      .Run(
          download->original_url, /*contents=*/std::string(),
          CreateManifestDownloadFailureMessage(download->url_before_redirects));
}

void PaymentManifestDownloader::OnURLLoaderComplete(
    network::SimpleURLLoader* url_loader,
    std::optional<std::string> response_body) {
  scoped_refptr<net::HttpResponseHeaders> headers;
  if (url_loader->ResponseInfo()) {
    headers = url_loader->ResponseInfo()->headers;
  }

  if (!response_body.has_value()) {
    response_body.emplace();
  }

  OnURLLoaderCompleteInternal(url_loader, url_loader->GetFinalURL(),
                              *response_body, headers, url_loader->NetError());
}

void PaymentManifestDownloader::OnURLLoaderCompleteInternal(
    network::SimpleURLLoader* url_loader,
    const GURL& final_url,
    const std::string& response_body,
    scoped_refptr<net::HttpResponseHeaders> headers,
    int net_error) {
  auto download_it = downloads_.find(url_loader);
  CHECK(download_it != downloads_.end());

  std::unique_ptr<Download> download = std::move(download_it->second);
  downloads_.erase(download_it);

  if (net_error != net::OK &&
      net_error != net::ERR_HTTP_RESPONSE_CODE_FAILURE) {
    RespondWithNetworkError(download->url_before_redirects, final_url,
                            net_error, *log_, std::move(download->callback));
    return;
  }

  if (download->IsResponseBodyDownload()) {
    if (headers && headers->response_code() != net::HTTP_OK) {
      RespondWithHttpStatusCodeError(download->url_before_redirects, final_url,
                                     headers->response_code(), *log_,
                                     std::move(download->callback));
    } else {
      RespondWithContent(response_body, errors::kNoContentInPaymentManifest,
                         download->url_before_redirects, final_url, *log_,
                         std::move(download->callback));
    }
    return;
  }

  DCHECK(download->IsLinkHeaderDownload());

  if (!headers) {
    RespondWithTemplateError(errors::kNoLinkHeader,
                             download->url_before_redirects, final_url, *log_,
                             std::move(download->callback));
    return;
  }

  if (headers->response_code() != net::HTTP_OK &&
      headers->response_code() != net::HTTP_NO_CONTENT) {
    RespondWithHttpStatusCodeError(download->url_before_redirects, final_url,
                                   headers->response_code(), *log_,
                                   std::move(download->callback));
    return;
  }

  std::string link_header =
      headers->GetNormalizedHeader("link").value_or(std::string());
  if (link_header.empty()) {
    RespondWithTemplateError(errors::kNoLinkHeader,
                             download->url_before_redirects, final_url, *log_,
                             std::move(download->callback));
    return;
  }

  for (const auto& value : link_header_util::SplitLinkHeader(link_header)) {
    std::unordered_map<std::string, std::optional<std::string>> params;
    std::optional<std::string> link_url =
        link_header_util::ParseLinkHeaderValue(value, params);
    if (!link_url) {
      continue;
    }

    auto rel = params.find("rel");
    if (rel == params.end()) {
      continue;
    }

    // Link relation types are case-insensitive (RFC 8288 section 2.1).
    std::vector<std::string> rel_parts = base::SplitString(
        base::ToLowerASCII(rel->second.value_or("")), HTTP_LWS,
        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (std::ranges::contains(rel_parts, "payment-method-manifest")) {
      GURL payment_method_manifest_url = final_url.Resolve(*link_url);

      if (!IsValidManifestUrl(payment_method_manifest_url, *log_)) {
        std::move(download->callback)
            .Run(final_url, /*contents=*/std::string(),
                 CreateManifestDownloadFailureMessage(
                     download->url_before_redirects));
        return;
      }

      if (!url::IsSameOriginWith(final_url, payment_method_manifest_url)) {
        RespondWithError(
            base::ReplaceStringPlaceholders(
                errors::kCrossOriginPaymentMethodManifestNotAllowed,
                {payment_method_manifest_url.spec(), final_url.spec()},
                nullptr),
            download->url_before_redirects, final_url, *log_,
            std::move(download->callback));
        return;
      }

      // The request initiator for the payment method manifest is the origin of
      // the GET request with the HTTP link header.
      // https://github.com/w3c/webappsec-fetch-metadata/issues/30
      InitiateDownload(
          url::Origin::Create(final_url), payment_method_manifest_url,
          /*url_before_redirects=*/download->url_before_redirects,
          /*did_follow_redirect=*/false, Download::Type::RESPONSE_BODY,
          /*allowed_number_of_redirects=*/0,
          /*use_url_loader_factory_rfh=*/false, std::move(download->callback));
      return;
    }
  }

  // HTTP HEAD response has no Link header that has a
  // rel="payment-method-manifest" entry.
  RespondWithTemplateError(errors::kNoLinkHeader,
                           download->url_before_redirects, final_url, *log_,
                           std::move(download->callback));
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
    bool use_url_loader_factory_rfh,
    PaymentManifestDownloadCallback callback) {
  DCHECK(UrlUtil::IsValidManifestUrl(url));

  // Only initial download of the payment method manifest (which might contain
  // an HTTP Link header) is allowed to redirect.
  DCHECK(allowed_number_of_redirects == 0 ||
         download_type == Download::Type::LINK_HEADER);

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
          internal {
            contacts {
              email: "chrome-payments-eng@google.com"
            }
            contacts {
              email: "darwinyang@chromium.org"
            }
          }
          user_data: {
            type: NONE
          }
          last_reviewed: "2026-02-09"
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
    case Download::Type::LINK_HEADER:
      resource_request->method = net::HttpRequestHeaders::kHeadMethod;
      break;
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

  content::RenderFrameHost* initiator_frame =
      initiator_document_.AsRenderFrameHostIfValid();
  if (!initiator_frame) {
    // The initiator frame is gone. No request should be made.
    RespondWithTemplateError(
        errors::kPaymentManifestDownloadFailed, download->url_before_redirects,
        download->original_url, *log_, std::move(download->callback));
    return;
  }

  if (!(use_url_loader_factory_rfh &&
        base::FeatureList::IsEnabled(
            features::kPaymentRequestUseRendererUrlLoader)) &&
      !FrameConnectionAllowlistAllowsRequestAndReportIfNeeded(
          initiator_frame, download->original_url, did_follow_redirect)) {
    // The download request is going to use the url loader factory for the
    // browser process, which does not have the network restriction id of the
    // initiator frame. The url loader factory does not check the connection
    // allowlist. The request URL has to be checked here.
    RespondWithNetworkError(
        download->url_before_redirects, download->original_url,
        did_follow_redirect ? net::ERR_UNSAFE_REDIRECT
                            : net::ERR_NETWORK_ACCESS_REVOKED,
        *log_, std::move(download->callback));
    return;
  }

  if (!csp_checker_) {  // Can be null when the webpage closes.
    RespondWithTemplateError(
        errors::kPaymentManifestDownloadFailed, download->url_before_redirects,
        download->original_url, *log_, std::move(download->callback));
    return;
  }

  csp_checker_->AllowConnectToSource(
      url, url_before_redirects, did_follow_redirect,
      base::BindOnce(&PaymentManifestDownloader::OnCSPCheck,
                     weak_ptr_factory_.GetWeakPtr(), std::move(download),
                     use_url_loader_factory_rfh));
}

void PaymentManifestDownloader::OnCSPCheck(std::unique_ptr<Download> download,
                                           bool use_url_loader_factory_rfh,
                                           bool csp_allowed) {
  if (!csp_allowed) {
    RespondWithTemplateError(
        errors::kPaymentManifestCSPDenied, download->url_before_redirects,
        download->original_url, *log_, std::move(download->callback));
    return;
  }

  network::SimpleURLLoader* loader = download->loader.get();
  loader->SetOnRedirectCallback(
      base::BindRepeating(&PaymentManifestDownloader::OnURLLoaderRedirect,
                          weak_ptr_factory_.GetWeakPtr(), loader));

  if (use_url_loader_factory_rfh &&
      base::FeatureList::IsEnabled(
          features::kPaymentRequestUseRendererUrlLoader)) {
    loader->DownloadToString(
        url_loader_factory_rfh_.get(),
        base::BindOnce(&PaymentManifestDownloader::OnURLLoaderComplete,
                       weak_ptr_factory_.GetWeakPtr(), loader),
        kMaxManifestSize);
  } else {
    loader->DownloadToString(
        url_loader_factory_.get(),
        base::BindOnce(&PaymentManifestDownloader::OnURLLoaderComplete,
                       weak_ptr_factory_.GetWeakPtr(), loader),
        kMaxManifestSize);
  }

  auto insert_result =
      downloads_.insert(std::make_pair(loader, std::move(download)));
  DCHECK(insert_result.second);  // Whether the insert has succeeded.
}

}  // namespace payments
