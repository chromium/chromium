// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/custom_icon_fetcher.h"

#include <array>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "crypto/hash.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace web_app {

CustomIconFetcher::CustomIconFetcher(
    Profile* profile,
    const GURL& icon_url,
    const std::optional<std::string>& expected_hash)
    : profile_(*profile), icon_url_(icon_url), expected_hash_(expected_hash) {}

CustomIconFetcher::~CustomIconFetcher() = default;

void CustomIconFetcher::StartRequest(Callback callback) {
  callback_ = std::move(callback);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = icon_url_;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("web_app_custom_icon_fetcher", R"(
        semantics {
          sender: "Web App Policy Manager"
          description:
            "Downloads a custom icon for an enterprise force-installed web app "
            "as specified in the WebAppInstallForceList policy."
          trigger:
            "An administrator force-installs a web app with a custom icon "
            "via the WebAppInstallForceList policy."
          internal {
            contacts {
              email: "pwa-team@google.com"
            }
          }
          user_data {
            type: NONE
          }
          data: "None"
          destination: WEBSITE
          last_reviewed: "2026-05-18"
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          chrome_policy {
            WebAppInstallForceList {
              WebAppInstallForceList: ""
            }
          }
        })");

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  simple_url_loader_->DownloadToString(
      profile_->GetURLLoaderFactory().get(),
      base::BindOnce(&CustomIconFetcher::OnDownloaded,
                     weak_ptr_factory_.GetWeakPtr()),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void CustomIconFetcher::OnDownloaded(std::optional<std::string> response_body) {
  if (!response_body) {
    std::move(callback_).Run(std::nullopt);
    return;
  }

  if (expected_hash_) {
    std::array<uint8_t, crypto::hash::kSha256Size> actual_hash =
        crypto::hash::Sha256(*response_body);
    std::string actual_hash_hex = base::HexEncode(actual_hash);
    if (!base::EqualsCaseInsensitiveASCII(actual_hash_hex, *expected_hash_)) {
      std::move(callback_).Run(std::nullopt);
      return;
    }
  }

  ImageDecoder::Start(this, std::move(*response_body));
}

void CustomIconFetcher::OnImageDecoded(const SkBitmap& decoded_image) {
  if (decoded_image.drawsNothing()) {
    std::move(callback_).Run(std::nullopt);
  } else {
    std::move(callback_).Run(decoded_image);
  }
}

void CustomIconFetcher::OnDecodeImageFailed() {
  std::move(callback_).Run(std::nullopt);
}

}  // namespace web_app
