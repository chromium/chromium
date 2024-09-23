// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest_fetcher.h"

#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace web_app {

namespace {

// We need to artificially limit the size of the update manifest, because it is
// loaded into memory.
// TODO(b/282633201): Document the limit.
constexpr size_t kMaxUpdateManifestLength = 5 * 1024 * 1024;

}  // namespace

UpdateManifestFetcher::UpdateManifestFetcher(
    GURL url,
    net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_(std::move(url)),
      partial_traffic_annotation_(std::move(partial_traffic_annotation)),
      url_loader_factory_(std::move(url_loader_factory)) {}

UpdateManifestFetcher::~UpdateManifestFetcher() = default;

void UpdateManifestFetcher::FetchUpdateManifest(FetchCallback fetch_callback) {
  CHECK(!fetch_callback_);
  fetch_callback_ = std::move(fetch_callback);
  DownloadUpdateManifest();
}

void UpdateManifestFetcher::DownloadUpdateManifest() {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::CompleteNetworkTrafficAnnotation("iwa_update_manifest_fetcher",
                                            partial_traffic_annotation_,
                                            R"(
    semantics {
      data:
        "This request does not send any user data. Its destination is the URL "
        "of an Update Manifest for an Isolated Web App that is installed for "
        "the user."
      destination: OTHER
      internal {
        contacts {
          owners: "//chrome/browser/web_applications/isolated_web_apps/OWNERS"
        }
      }
      user_data {
        type: NONE
      }
      last_reviewed: "2023-05-25"
    }
    policy {
      cookies_allowed: NO
    })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url_;
  // Cookies are not allowed.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), std::move(traffic_annotation));

  simple_url_loader_->SetRetryOptions(
      /* max_retries=*/3,
      network::SimpleURLLoader::RETRY_ON_5XX |
          network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  simple_url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&UpdateManifestFetcher::OnUpdateManifestDownloaded,
                     weak_factory_.GetWeakPtr()),
      kMaxUpdateManifestLength);
}

void UpdateManifestFetcher::OnUpdateManifestDownloaded(
    std::unique_ptr<std::string> update_manifest_content) {
  // We may extract some information from the loader about
  // downloading errors in the future.
  simple_url_loader_.reset();

  if (!update_manifest_content) {
    std::move(fetch_callback_).Run(base::unexpected(Error::kDownloadFailed));
    return;
  }

  ParseUpdateManifest(*update_manifest_content);
}

void UpdateManifestFetcher::ParseUpdateManifest(
    const std::string& update_manifest_content) {
  InitializeJsonParser();

  json_parser_->Parse(
      update_manifest_content, base::JSON_PARSE_RFC,
      base::BindOnce(&UpdateManifestFetcher::OnUpdateManifestParsed,
                     base::Unretained(this)));
}

void UpdateManifestFetcher::InitializeJsonParser() {
  CHECK(!json_parser_);
  data_decoder_.GetService()->BindJsonParser(
      json_parser_.BindNewPipeAndPassReceiver());
  json_parser_.set_disconnect_handler(base::BindOnce(
      &UpdateManifestFetcher::OnUpdateManifestParsed, base::Unretained(this),
      std::nullopt, "JsonParser terminated unexpectedly"));
}

void UpdateManifestFetcher::OnUpdateManifestParsed(
    std::optional<base::Value> result,
    const std::optional<std::string>& error) {
  if (!result.has_value()) {
    if (error.has_value()) {
      LOG(ERROR) << "Unable to parse IWA Update Manifest JSON for URL " << url_
                 << ". Error: was" << *error;
    }
    std::move(fetch_callback_).Run(base::unexpected(Error::kInvalidJson));
    return;
  }

  base::expected<UpdateManifest, UpdateManifest::JsonFormatError>
      update_manifest =
          UpdateManifest::CreateFromJson(std::move(*result), url_);

  std::move(fetch_callback_)
      .Run(update_manifest.transform_error(
          [](UpdateManifest::JsonFormatError error) -> Error {
            switch (error) {
              case UpdateManifest::JsonFormatError::kRootNotADictionary:
              case UpdateManifest::JsonFormatError::kChannelsNotADictionary:
              case UpdateManifest::JsonFormatError::kChannelNotADictionary:
              case UpdateManifest::JsonFormatError::kVersionsNotAnArray:
              case UpdateManifest::JsonFormatError::kVersionEntryNotADictionary:
                return Error::kInvalidManifest;
            }
          }));
}

}  // namespace web_app
