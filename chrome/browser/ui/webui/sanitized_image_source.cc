// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sanitized_image_source.h"

#include "base/memory/ref_counted_memory.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/strcat.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "url/url_util.h"

SanitizedImageSource::SanitizedImageSource(Profile* profile)
    : SanitizedImageSource(
          profile,
          content::BrowserContext::GetDefaultStoragePartition(profile)
              ->GetURLLoaderFactoryForBrowserProcess(),
          std::make_unique<ImageDecoderImpl>()) {}

SanitizedImageSource::SanitizedImageSource(
    Profile* profile,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<image_fetcher::ImageDecoder> image_decoder)
    : url_loader_factory_(url_loader_factory),
      image_decoder_(std::move(image_decoder)),
      encode_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

SanitizedImageSource::~SanitizedImageSource() = default;

std::string SanitizedImageSource::GetSource() {
  return chrome::kChromeUIImageHost;
}

void SanitizedImageSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GURL url_param = GURL(url.query());
  if (!url_param.is_valid() ||
      url != GURL(base::StrCat(
                 {chrome::kChromeUIImageURL, "?", url_param.spec()}))) {
    std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>());
    return;
  }

  // Download the image body.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("sanitized_image_source", R"(
        semantics {
          sender: "WebUI Sanitized Image Source"
          description:
            "This data source fetches an arbitrary image to be displayed in a "
            "WebUI."
          trigger:
            "When a WebUI triggers the download of chrome://image?<URL> by "
            "e.g. setting that URL as a src on an img tag."
          data: "NONE"
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
  request->url = url_param;
  loaders_.push_back(
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation));
  loaders_.back()->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&SanitizedImageSource::OnImageLoaded,
                     weak_ptr_factory_.GetWeakPtr(), loaders_.back().get(),
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
    network::SimpleURLLoader* loader,
    content::URLDataSource::GotDataCallback callback,
    std::unique_ptr<std::string> body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Take loader out of |loaders_| list and store it in a unique_ptr on the
  // stack to make sure the loader gets cleaned upon return.
  auto it = std::find_if(
      loaders_.begin(), loaders_.end(),
      [loader](const std::unique_ptr<network::SimpleURLLoader>& target) {
        return loader == target.get();
      });
  std::unique_ptr<network::SimpleURLLoader> loader_ptr(std::move(*it));
  loaders_.erase(it);

  if (loader->NetError() != net::OK || !body) {
    std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>());
    return;
  }

  // Send image body to image decoder in isolated process.
  image_decoder_->DecodeImage(
      *body, gfx::Size() /* No particular size desired. */,
      base::BindOnce(&SanitizedImageSource::OnImageDecoded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SanitizedImageSource::OnImageDecoded(
    content::URLDataSource::GotDataCallback callback,
    const gfx::Image& image) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Re-encode vetted image as PNG and send to requester.
  encode_task_runner_->PostTaskAndReplyWithResult(
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
