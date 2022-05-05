// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sanitized_image_source.h"

#include <map>
#include <memory>
#include <string>

#include "base/containers/contains.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "url/url_util.h"

namespace {

std::map<std::string, std::string> ParseParams(
    const std::string& param_string) {
  url::Component query(0, param_string.size());
  url::Component key;
  url::Component value;
  constexpr int kMaxUriDecodeLen = 2048;
  std::map<std::string, std::string> params;
  while (
      url::ExtractQueryKeyValue(param_string.c_str(), &query, &key, &value)) {
    url::RawCanonOutputW<kMaxUriDecodeLen> output;
    url::DecodeURLEscapeSequences(param_string.c_str() + value.begin, value.len,
                                  url::DecodeURLMode::kUTF8OrIsomorphic,
                                  &output);
    params.insert({param_string.substr(key.begin, key.len),
                   base::UTF16ToUTF8(
                       base::StringPiece16(output.data(), output.length()))});
  }
  return params;
}

bool IsGooglePhotosUrl(const GURL& url) {
  static const char* const kGooglePhotosHostSuffixes[] = {
      ".ggpht.com",
      ".google.com",
      ".googleusercontent.com",
  };

  for (const char* const suffix : kGooglePhotosHostSuffixes) {
    if (base::EndsWith(url.host_piece(), suffix))
      return true;
  }
  return false;
}

}  // namespace

SanitizedImageSource::SanitizedImageSource(Profile* profile)
    : SanitizedImageSource(profile,
                           profile->GetDefaultStoragePartition()
                               ->GetURLLoaderFactoryForBrowserProcess(),
                           std::make_unique<ImageDecoderImpl>()) {}

SanitizedImageSource::SanitizedImageSource(
    Profile* profile,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<image_fetcher::ImageDecoder> image_decoder)
    : identity_manager_(IdentityManagerFactory::GetForProfile(profile)),
      url_loader_factory_(url_loader_factory),
      image_decoder_(std::move(image_decoder)) {}

SanitizedImageSource::~SanitizedImageSource() = default;

std::string SanitizedImageSource::GetSource() {
  return chrome::kChromeUIImageHost;
}

void SanitizedImageSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string image_url_or_params = url.query();
  if (url != GURL(base::StrCat(
                 {chrome::kChromeUIImageURL, "?", image_url_or_params}))) {
    std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>());
    return;
  }

  GURL image_url = GURL(image_url_or_params);
  bool send_auth_token = false;
  if (!image_url.is_valid()) {
    // Attempt to parse URL and additional options from params.
    auto params = ParseParams(image_url_or_params);

    auto url_it = params.find("url");
    if (url_it == params.end()) {
      std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>());
      return;
    }
    image_url = GURL(url_it->second);

    auto google_photos_it = params.find("isGooglePhotos");
    if (google_photos_it != params.end() &&
        google_photos_it->second == "true" && IsGooglePhotosUrl(image_url)) {
      send_auth_token = true;
    }
  }

  // Download the image body.
  if (!send_auth_token) {
    StartImageDownload(std::move(image_url), std::move(callback),
                       absl::nullopt);
    return;
  }

  // Request an auth token for downloading the image body.
  auto fetcher = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      "sanitized_image_source", identity_manager_,
      signin::ScopeSet({GaiaConstants::kPhotosModuleImageOAuth2Scope}),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
      signin::ConsentLevel::kSignin);
  auto* fetcher_ptr = fetcher.get();
  fetcher_ptr->Start(base::BindOnce(
      [](const base::WeakPtr<SanitizedImageSource>& self,
         std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> fetcher,
         GURL image_url, content::URLDataSource::GotDataCallback callback,
         GoogleServiceAuthError error,
         signin::AccessTokenInfo access_token_info) {
        if (error.state() != GoogleServiceAuthError::NONE) {
          LOG(ERROR) << "Failed to authenticate for Google Photos in order to "
                        "download "
                     << image_url.spec()
                     << ". Error message: " << error.ToString();
          return;
        }

        if (self) {
          self->StartImageDownload(std::move(image_url), std::move(callback),
                                   std::move(access_token_info));
        }
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(fetcher), std::move(image_url),
      std::move(callback)));
}

void SanitizedImageSource::StartImageDownload(
    GURL image_url,
    content::URLDataSource::GotDataCallback callback,
    absl::optional<signin::AccessTokenInfo> access_token_info) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("sanitized_image_source", R"(
        semantics {
          sender: "WebUI Sanitized Image Source"
          description:
            "This data source fetches an arbitrary image to be displayed in a "
            "WebUI."
          trigger:
            "When a WebUI triggers the download of chrome://image?<URL> or "
            "chrome://image?url=<URL>&isGooglePhotos=<bool> by e.g. setting "
            "that URL as a src on an img tag."
          data: "OAuth credentials for the user's Google Photos account when "
                "isGooglePhotos is true."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          policy_exception_justification:
            "This is a helper data source. It can be indirectly disabled by "
            "disabling the requester WebUI."
        })");
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = image_url;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  if (access_token_info) {
    request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                               "Bearer " + access_token_info->token);
  }

  auto loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  auto* loader_ptr = loader.get();
  loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&SanitizedImageSource::OnImageLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(loader),
                     std::move(callback)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

std::string SanitizedImageSource::GetMimeType(const std::string& path) {
  return "image/png";
}

bool SanitizedImageSource::ShouldReplaceExistingSource() {
  // Leave the existing DataSource in place, otherwise we'll drop any pending
  // requests on the floor.
  return false;
}

void SanitizedImageSource::OnImageLoaded(
    std::unique_ptr<network::SimpleURLLoader> loader,
    content::URLDataSource::GotDataCallback callback,
    std::unique_ptr<std::string> body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (loader->NetError() != net::OK || !body) {
    std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>());
    return;
  }

  // Send image body to image decoder in isolated process.
  image_decoder_->DecodeImage(
      *body, gfx::Size() /* No particular size desired. */,
      /*data_decoder=*/nullptr,
      base::BindOnce(&SanitizedImageSource::OnImageDecoded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SanitizedImageSource::OnImageDecoded(
    content::URLDataSource::GotDataCallback callback,
    const gfx::Image& image) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Re-encode vetted image as PNG and send to requester.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const SkBitmap& bitmap) {
            auto encoded = base::MakeRefCounted<base::RefCountedBytes>();
            return gfx::PNGCodec::EncodeBGRASkBitmap(
                       bitmap, /*discard_transparency=*/false, &encoded->data())
                       ? encoded
                       : base::MakeRefCounted<base::RefCountedBytes>();
          },
          image.AsBitmap()),
      std::move(callback));
}
