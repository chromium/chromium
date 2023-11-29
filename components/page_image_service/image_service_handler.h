// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_IMAGE_SERVICE_IMAGE_SERVICE_HANDLER_H_
#define COMPONENTS_PAGE_IMAGE_SERVICE_IMAGE_SERVICE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace page_image_service {

class ImageService;

// Handles ImageService related communication between C++ and WebUI in the
// renderer.
class ImageServiceHandler : public mojom::PageImageServiceHandler {
 public:
  ImageServiceHandler(
      mojo::PendingReceiver<mojom::PageImageServiceHandler>
          pending_page_handler,
      base::WeakPtr<page_image_service::ImageService> image_service);
  ImageServiceHandler(const ImageServiceHandler&) = delete;
  ImageServiceHandler& operator=(const ImageServiceHandler&) = delete;
  ~ImageServiceHandler() override;

  // ::mojom::PageImageServiceHandler:
  void GetPageImageUrl(mojom::ClientId client_id,
                       const GURL& page_url,
                       mojom::OptionsPtr options,
                       GetPageImageUrlCallback callback) override;

 private:
  // Callback for `GetImageServiceUrl()`.
  void OnGotImageServiceResult(GetPageImageUrlCallback callback,
                               const GURL& image_url);

  mojo::Receiver<mojom::PageImageServiceHandler> page_handler_;

  const base::WeakPtr<page_image_service::ImageService> image_service_;

  // Used to scope callbacks to the lifetime of the handler.
  base::WeakPtrFactory<ImageServiceHandler> weak_factory_{this};
};

}  // namespace page_image_service

#endif  // COMPONENTS_PAGE_IMAGE_SERVICE_IMAGE_SERVICE_HANDLER_H_
