// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_image_service/image_service_handler.h"

#include "components/page_image_service/image_service.h"

namespace page_image_service {

ImageServiceHandler::ImageServiceHandler(
    mojo::PendingReceiver<mojom::PageImageServiceHandler> pending_page_handler,
    base::WeakPtr<page_image_service::ImageService> image_service)
    : page_handler_(this, std::move(pending_page_handler)),
      image_service_(image_service) {}

ImageServiceHandler::~ImageServiceHandler() = default;

void ImageServiceHandler::GetPageImageUrl(mojom::ClientId client_id,
                                          const GURL& page_url,
                                          mojom::OptionsPtr options,
                                          GetPageImageUrlCallback callback) {
  if (!image_service_ || options.is_null()) {
    return std::move(callback).Run(nullptr);
  }

  image_service_->FetchImageFor(
      client_id, page_url, *options,
      base::BindOnce(&ImageServiceHandler::OnGotImageServiceResult,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ImageServiceHandler::OnGotImageServiceResult(
    GetPageImageUrlCallback callback,
    const GURL& image_url) {
  if (!image_url.is_valid()) {
    return std::move(callback).Run(nullptr);
  }

  auto result_mojom = mojom::ImageResult::New();
  result_mojom->image_url = image_url;
  std::move(callback).Run(std::move(result_mojom));
}

}  // namespace page_image_service
