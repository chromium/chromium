// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/sanitized_image_source.h"

#include <map>
#include <memory>
#include <string>
#include <string_view>

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
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ipc/ipc_channel.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/encode/SkWebpEncoder.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/codec/webp_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "url/url_util.h"

namespace {

const int64_t kMaxImageSizeInBytes =
    static_cast<int64_t>(IPC::Channel::kMaximumMessageSize);

constexpr char kEncodeTypeKey[] = "encodeType";
constexpr char kIsGooglePhotosKey[] = "isGooglePhotos";
constexpr char kStaticEncodeKey[] = "staticEncode";
constexpr char kUrlKey[] = "url";

std::map<std::string, std::string> ParseParams(std::string_view param_string) {
  url::Component query(0, param_string.size());
  url::Component key;
  url::Component value;
  constexpr int kMaxUriDecodeLen = 2048;
  std::map<std::string, std::string> params;
  while (url::ExtractQueryKeyValue(param_string, &query, &key, &value)) {
    url::RawCanonOutputW<kMaxUriDecodeLen> output;
    url::DecodeURLEscapeSequences(param_string.substr(value.begin, value.len),
                                  url::DecodeURLMode::kUTF8OrIsomorphic,
                                  &output);
    params.insert({std::string(param_string.substr(key.begin, key.len)),
                   base::UTF16ToUTF8(output.view())});
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

void SanitizedImageSource::DataDecoderDelegate::DecodeImage(
    const std::string& data,
    DecodeImageCallback callback) {
  base::span<const uint8_t> bytes = base::make_span(
      reinterpret_cast<const uint8_t*>(data.data()), data.size());

  data_decoder::DecodeImage(
      &data_decoder_, bytes, data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/true, data_decoder::kDefaultMaxSizeInBytes,
      /*desired_image_frame_size=*/gfx::Size(), std::move(callback));
}

void SanitizedImageSource::DataDecoderDelegate::DecodeAnimation(
    const std::string& data,
    DecodeAnimationCallback callback) {
  base::span<const uint8_t> bytes = base::make_span(
      reinterpret_cast<const uint8_t*>(data.data()), data.size());

  data_decoder::DecodeAnimation(&data_decoder_, bytes, /*shrink_to_fit=*/true,
                                kMaxImageSizeInBytes, std::move(callback));
}

SanitizedImageSource::SanitizedImageSource(Profile* profile)
    : SanitizedImageSource(profile,
                           profile->GetDefaultStoragePartition()
                               ->GetURLLoaderFactoryForBrowserProcess(),
                           std::make_unique<DataDecoderDelegate>()) {}

SanitizedImageSource::SanitizedImageSource(
    Profile* profile,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<DataDecoderDelegate> delegate)
    : identity_manager_(IdentityManagerFactory::GetForProfile(profile)),
      url_loader_factory_(url_loader_factory),
      data_decoder_delegate_(std::move(delegate)) {}

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
    std::move(callback).Run(nullptr);
    return;
  }

  RequestAttributes request_attributes;
  GURL image_url = GURL(image_url_or_params);
  bool send_auth_token = false;
  if (!image_url.is_valid()) {
    // Attempt to parse URL and additional options from params.
    auto params = ParseParams(image_url_or_params);

    auto url_it = params.find(kUrlKey);
    if (url_it == params.end()) {
      std::move(callback).Run(nullptr);
      return;
    }
    image_url = GURL(url_it->second);

    auto static_encode_it = params.find(kStaticEncodeKey);
    if (static_encode_it != params.end()) {
      request_attributes.static_encode = static_encode_it->second == "true";
    }

    auto encode_type_ir = params.find(kEncodeTypeKey);
    if (encode_type_ir != params.end()) {
      request_attributes.encode_type =
          encode_type_ir->second == "webp"
              ? RequestAttributes::EncodeType::kWebP
              : RequestAttributes::EncodeType::kPng;
    }

    auto google_photos_it = params.find(kIsGooglePhotosKey);
    if (google_photos_it != params.end() &&
        google_photos_it->second == "true" && IsGooglePhotosUrl(image_url)) {
      send_auth_token = true;
    }
  }

  if (image_url.SchemeIs(url::kHttpScheme)) {
    // Disallow any HTTP requests, treat them as a failure instead.
    std::move(callback).Run(nullptr);
    return;
  }

  request_attributes.image_url = image_url;

  // Download the image body.
  if (!send_auth_token) {
    StartImageDownload(std::move(request_attributes), std::move(callback));
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
         RequestAttributes request_attributes,
         content::URLDataSource::GotDataCallback callback,
         GoogleServiceAuthError error,
         signin::AccessTokenInfo access_token_info) {
        if (error.state() != GoogleServiceAuthError::NONE) {
          LOG(ERROR) << "Failed to authenticate for Google Photos in order to "
                        "download "
                     << request_attributes.image_url.spec()
                     << ". Error message: " << error.ToString();
          return;
        }

        request_attributes.access_token_info = access_token_info;

        if (self) {
          self->StartImageDownload(std::move(request_attributes),
                                   std::move(callback));
        }
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(fetcher),
      std::move(request_attributes), std::move(callback)));
}

SanitizedImageSource::RequestAttributes::RequestAttributes() = default;
SanitizedImageSource::RequestAttributes::RequestAttributes(
    const RequestAttributes&) = default;
SanitizedImageSource::RequestAttributes::~RequestAttributes() = default;

void SanitizedImageSource::StartImageDownload(
    RequestAttributes request_attributes,
    content::URLDataSource::GotDataCallback callback) {
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
  request->url = request_attributes.image_url;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  if (request_attributes.access_token_info) {
    request->headers.SetHeader(
        net::HttpRequestHeaders::kAuthorization,
        "Bearer " + request_attributes.access_token_info->token);
  }

  auto loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  auto* loader_ptr = loader.get();
  loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&SanitizedImageSource::OnImageLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(loader),
                     std::move(request_attributes), std::move(callback)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

std::string SanitizedImageSource::GetMimeType(const GURL& url) {
  return "image/png";
}

bool SanitizedImageSource::ShouldReplaceExistingSource() {
  // Leave the existing DataSource in place, otherwise we'll drop any pending
  // requests on the floor.
  return false;
}

void SanitizedImageSource::OnImageLoaded(
    std::unique_ptr<network::SimpleURLLoader> loader,
    RequestAttributes request_attributes,
    content::URLDataSource::GotDataCallback callback,
    std::unique_ptr<std::string> body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (loader->NetError() != net::OK || !body) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (request_attributes.static_encode) {
    data_decoder_delegate_->DecodeImage(
        *body,
        base::BindOnce(&SanitizedImageSource::EncodeAndReplyStaticImage,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(request_attributes), std::move(callback)));
    return;
  }

  data_decoder_delegate_->DecodeAnimation(
      *body,
      base::BindOnce(&SanitizedImageSource::OnAnimationDecoded,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(request_attributes), std::move(callback)));
}

void SanitizedImageSource::OnAnimationDecoded(
    RequestAttributes request_attributes,
    content::URLDataSource::GotDataCallback callback,
    std::vector<data_decoder::mojom::AnimationFramePtr> mojo_frames) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!mojo_frames.size()) {
    std::move(callback).Run(nullptr);
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (mojo_frames.size() > 1) {
    // The image is animated, re-encode as WebP animated image and send to
    // requester.
    EncodeAndReplyAnimatedImage(std::move(callback), std::move(mojo_frames));
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Re-encode as static image and send to requester.
  EncodeAndReplyStaticImage(std::move(request_attributes), std::move(callback),
                            mojo_frames[0]->bitmap);
}

void SanitizedImageSource::EncodeAndReplyStaticImage(
    RequestAttributes request_attributes,
    content::URLDataSource::GotDataCallback callback,
    const SkBitmap& bitmap) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const SkBitmap& bitmap,
             RequestAttributes::EncodeType encode_type) {
            auto encoded = base::MakeRefCounted<base::RefCountedBytes>();
            const bool success =
                encode_type == RequestAttributes::EncodeType::kWebP
                    ? gfx::WebpCodec::Encode(bitmap, /*quality=*/90,
                                             &encoded->as_vector())
                    : gfx::PNGCodec::EncodeBGRASkBitmap(
                          bitmap, /*discard_transparency=*/false,
                          &encoded->as_vector());
            return success ? encoded
                           : base::MakeRefCounted<base::RefCountedBytes>();
          },
          bitmap, request_attributes.encode_type),
      std::move(callback));
  return;
}

void SanitizedImageSource::EncodeAndReplyAnimatedImage(
    content::URLDataSource::GotDataCallback callback,
    std::vector<data_decoder::mojom::AnimationFramePtr> mojo_frames) {
  std::vector<gfx::WebpCodec::Frame> frames;
  for (auto& mojo_frame : mojo_frames) {
    gfx::WebpCodec::Frame frame;
    frame.bitmap = mojo_frame->bitmap;
    frame.duration = mojo_frame->duration.InMilliseconds();
    frames.push_back(frame);
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const std::vector<gfx::WebpCodec::Frame>& frames) {
            SkWebpEncoder::Options options;
            options.fCompression = SkWebpEncoder::Compression::kLossless;
            // Lower quality under kLosless compression means compress faster
            // into larger files.
            options.fQuality = 0;

            auto encoded = gfx::WebpCodec::EncodeAnimated(frames, options);
            if (encoded.has_value()) {
              return base::MakeRefCounted<base::RefCountedBytes>(
                  encoded.value());
            }

            return base::MakeRefCounted<base::RefCountedBytes>();
          },
          frames),
      std::move(callback));
}
