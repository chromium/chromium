// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/tutorial_fetcher.h"

#include <utility>

#include "base/lazy_instance.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace video_tutorials {
namespace {

// An override server URL for testing.
base::LazyInstance<GURL>::Leaky g_override_url_for_testing;

const char kRequestContentType[] = "application/x-protobuf";

constexpr net::NetworkTrafficAnnotationTag
    kVideoTutorialFetcherTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("video_tutorial_fetcher", R"(
              semantics {
                sender: "Video Tutorial Fetcher"
                description:
                  "Fetches metadata for showing video tutorials on Android."
                trigger:
                  "Chrome startup. Only triggered if the cache is older "
                  "than two weeks."
                data: "No user data."
                destination: GOOGLE_OWNED_SERVICE
              }
              policy {
                cookies_allowed: NO
                setting:
                    "No user setting to disable this feature. The "
                    "feature is only enabled in certain countries."
                policy_exception_justification:
                    "Not implemented. The fetch frequency is really low."
              }
    )");

class TutorialFetcherImpl : public TutorialFetcher {
 public:
  TutorialFetcherImpl(
      const GURL& url,
      const std::string& country_code,
      const std::string& accept_languages,
      const std::string& api_key,
      const std::string& experiment_tag,
      const std::string& client_version,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : url_loader_factory_(url_loader_factory),
        url_(url),
        country_code_(country_code),
        accept_languages_(accept_languages),
        api_key_(api_key),
        experiment_tag_(experiment_tag),
        client_version_(client_version) {}

 private:
  // TutorialFetcher implementation.
  void StartFetchForTutorials(FinishedCallback callback) override {
    auto resource_request = BuildGetRequest();
    if (!resource_request)
      return;
    url_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request), kVideoTutorialFetcherTrafficAnnotation);
    url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory_.get(),
        base::BindOnce(&TutorialFetcherImpl::OnDownloadComplete,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnAcceptLanguagesChanged(const std::string& accept_languages) override {
    accept_languages_ = accept_languages;
  }

  // Build the request to get tutorial info.
  std::unique_ptr<network::ResourceRequest> BuildGetRequest() {
    if (url_.is_empty() && g_override_url_for_testing.Get().is_empty())
      return nullptr;
    auto request = std::make_unique<network::ResourceRequest>();
    request->method = net::HttpRequestHeaders::kGetMethod;
    request->headers.SetHeader("x-goog-api-key", api_key_);
    request->headers.SetHeader("X-Client-Version", client_version_);
    request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                               kRequestContentType);
    request->url =
        net::AppendOrReplaceQueryParameter(url_, "country_code", country_code_);
    if (!experiment_tag_.empty()) {
      request->url = net::AppendOrReplaceQueryParameter(
          request->url, "experiment_tag", experiment_tag_);
    }
    if (!accept_languages_.empty()) {
      request->headers.SetHeader(net::HttpRequestHeaders::kAcceptLanguage,
                                 accept_languages_);
    }

    if (!g_override_url_for_testing.Get().is_empty())
      request->url = g_override_url_for_testing.Get();

    return request;
  }

  // Called after receiving HTTP response. Processes the response code and net
  // error.
  void OnDownloadComplete(FinishedCallback callback,
                          std::unique_ptr<std::string> response_body) {
    int response_code = -1;
    if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers)
      response_code = url_loader_->ResponseInfo()->headers->response_code();

    bool success =
        (response_code >= 200 && response_code < 300 && response_body);
    // TODO(shaktisahu): Collect metrics on response code.
    std::move(callback).Run(success, std::move(response_body));
    url_loader_.reset();
  }

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Simple URL loader to fetch proto from network.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Params of resource request.
  GURL url_;
  std::string country_code_;
  std::string accept_languages_;
  std::string api_key_;
  std::string experiment_tag_;
  std::string client_version_;

  base::WeakPtrFactory<TutorialFetcherImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
std::unique_ptr<TutorialFetcher> TutorialFetcher::Create(
    const GURL& url,
    const std::string& country_code,
    const std::string& accept_languages,
    const std::string& api_key,
    const std::string& experiment_tag,
    const std::string& client_version,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<TutorialFetcherImpl>(
      url, country_code, accept_languages, api_key, experiment_tag,
      client_version, url_loader_factory);
}

// static
void TutorialFetcher::SetOverrideURLForTesting(const GURL& url) {
  g_override_url_for_testing.Get() = url;
}

TutorialFetcher::TutorialFetcher() = default;
TutorialFetcher::~TutorialFetcher() = default;

}  // namespace video_tutorials
