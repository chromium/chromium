// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/common/url_icon_source.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_operations.h"

using content::BrowserThread;

namespace app_list {

UrlIconSource::UrlIconSource(IconLoadedCallback icon_loaded_callback,
                             content::BrowserContext* browser_context,
                             const GURL& icon_url,
                             int icon_size,
                             int default_icon_resource_id)
    : icon_loaded_callback_(std::move(icon_loaded_callback)),
      browser_context_(browser_context),
      icon_url_(icon_url),
      icon_size_(icon_size),
      default_icon_resource_id_(default_icon_resource_id),
      icon_fetch_attempted_(false) {
  DCHECK(!icon_loaded_callback_.is_null());
}

UrlIconSource::~UrlIconSource() {
}

void UrlIconSource::StartIconFetch() {
  icon_fetch_attempted_ = true;

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = icon_url_;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("url_icon_source_fetch", R"(
          semantics {
            sender: "URL Icon Source"
            description:
              "Chrome OS downloads an app icon for display in the app list."
            trigger:
              "An icon/image needs to be downloaded to be displayed."
            data:
              "URL of the icon/image. "
              "No user information is sent."
            destination: WEBSITE
          }
          policy {
            cookies_allowed: NO
            setting: "Unconditionally enabled on Chrome OS."
            policy_exception_justification:
              "Not implemented, considered not useful."
          })");
  simple_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                    traffic_annotation);
  network::mojom::URLLoaderFactory* loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(browser_context_)
          ->GetURLLoaderFactoryForBrowserProcess()
          .get();
  simple_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory, base::BindOnce(&UrlIconSource::OnSimpleLoaderComplete,
                                     base::Unretained(this)));
}

gfx::ImageSkiaRep UrlIconSource::GetImageForScale(float scale) {
  if (!icon_fetch_attempted_)
    StartIconFetch();

  if (!icon_.isNull())
    return icon_.GetRepresentation(scale);

  return ui::ResourceBundle::GetSharedInstance()
      .GetImageSkiaNamed(default_icon_resource_id_)->GetRepresentation(scale);
}

void UrlIconSource::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    return;
  }

  // Call start to begin decoding.  The ImageDecoder will call OnImageDecoded
  // with the data when it is done.
  ImageDecoder::Start(this, *response_body);
}

void UrlIconSource::OnImageDecoded(const SkBitmap& decoded_image) {
  const float scale = decoded_image.width() / icon_size_;
  icon_ = gfx::ImageSkia::CreateFromBitmap(decoded_image, scale);
  DCHECK(!icon_loaded_callback_.is_null());
  std::move(icon_loaded_callback_).Run();
}

void UrlIconSource::OnDecodeImageFailed() {
  // Failed to decode image. Do nothing and just use the default icon.
}

}  // namespace app_list
