// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_SERVICE_IMAGE_SERVICE_HANDLER_H_
#define COMPONENTS_IMAGE_SERVICE_IMAGE_SERVICE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "components/image_service/mojom/image_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace image_service {

class ImageService;

// Handles ImageService related communication between C++ and WebUI in the
// renderer.
class ImageServiceHandler : public mojom::ImageServiceHandler {
 public:
  ImageServiceHandler(
      mojo::PendingReceiver<mojom::ImageServiceHandler> pending_page_handler,
      base::WeakPtr<image_service::ImageService> image_service);
  ImageServiceHandler(const ImageServiceHandler&) = delete;
  ImageServiceHandler& operator=(const ImageServiceHandler&) = delete;
  ~ImageServiceHandler() override;

  // ::mojom::ImageServiceHandler:
  void GetPageImageUrl(mojom::ClientId client_id,
                       const GURL& page_url,
                       mojom::OptionsPtr options,
                       GetPageImageUrlCallback callback) override;

 private:
  // Callback for `GetImageServiceUrl()`.
  void OnGotImageServiceResult(GetPageImageUrlCallback callback,
                               const GURL& image_url);

  mojo::Receiver<mojom::ImageServiceHandler> page_handler_;

  const base::WeakPtr<image_service::ImageService> image_service_;

  // Used to scope callbacks to the lifetime of the handler.
  base::WeakPtrFactory<ImageServiceHandler> weak_factory_{this};
};

}  // namespace image_service

#endif  // CHROME_BROWSER_COMPONENTS_IMAGE_SERVICE_IMAGE_SERVICE_HANDLER_H_
