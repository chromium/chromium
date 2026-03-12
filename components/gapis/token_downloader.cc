// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gapis/token_downloader.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/gapis/proto/obtain_token.pb.h"
#include "google_apis/credentials_mode.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace gapis {

BASE_FEATURE(kEnableGapis, base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

constexpr char kObtainTokenEndpoint[] = "token";
constexpr base::TimeDelta kRequestTimeout = base::Seconds(10);

GURL GetObtainTokenURL(const GURL& gapis_service_url) {
  std::string path = gapis_service_url.GetPath();
  if (path.empty() || *path.rbegin() != '/') {
    path += '/';
  }
  path += kObtainTokenEndpoint;
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return gapis_service_url.ReplaceComponents(replacements);
}

}  // namespace

TokenDownloader::TokenDownloader(
    const GURL& gapis_service_url,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : obtain_token_url_(GetObtainTokenURL(gapis_service_url)),
      url_loader_factory_(url_loader_factory) {
  CHECK(!gapis_service_url.is_empty());
  CHECK(url_loader_factory_);
  CHECK(base::FeatureList::IsEnabled(kEnableGapis));
}

TokenDownloader::~TokenDownloader() = default;

void TokenDownloader::FetchToken(FetchTokenCallback callback,
                                 const std::string& access_token,
                                 const std::string& signed_challenge) {
  CHECK(!simple_url_loader_);
  CHECK(!fetch_token_callback_);
  CHECK(callback);
  CHECK(!timer_.IsRunning());
  CHECK(!access_token.empty());

  fetch_token_callback_ = std::move(callback);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = obtain_token_url_;
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode =
      google_apis::GetOmitCredentialsModeForGaiaRequests();
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StringPrintf("Bearer %s", access_token.c_str()));

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gapis_token_downloader", R"(
        semantics {
          sender: "Chrome Sync"
          description:
            "A network request to download a token from the Sync service."
          trigger: "Request to Google APIs service."
          data: "An OAuth2 access token."
          destination: GOOGLE_OWNED_SERVICE
          user_data {
            type: ACCESS_TOKEN
          }
          internal {
            contacts {
              email: "rushans@google.com"
            }
            contacts {
              email: "msarda@chromium.org"
            }
          }
          last_reviewed: "2026-02-26"
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          policy_exception_justification: "Prototype feature behind a feature"
            " toggle disabled by default."
        })");

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  gapis_pb::ObtainTokenRequest request_proto;
  request_proto.set_signed_challenge(signed_challenge);

  simple_url_loader_->AttachStringForUpload(request_proto.SerializeAsString(),
                                            "application/x-protobuf");
  simple_url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&TokenDownloader::OnSimpleLoaderComplete,
                     base::Unretained(this)),
      /*max_body_size=*/1024 * 1024);
  timer_.Start(FROM_HERE, kRequestTimeout, this, &TokenDownloader::OnTimeout);
}

void TokenDownloader::OnSimpleLoaderComplete(
    std::optional<std::string> response_body) {
  timer_.Stop();
  simple_url_loader_.reset();

  gapis_pb::ObtainTokenResponse response_proto;
  if (!response_body || !response_proto.ParseFromString(*response_body)) {
    std::move(fetch_token_callback_).Run(std::string());
    return;
  }

  std::move(fetch_token_callback_).Run(response_proto.token());
}

void TokenDownloader::OnTimeout() {
  simple_url_loader_.reset();
  std::move(fetch_token_callback_).Run(std::string());
}

}  // namespace gapis
