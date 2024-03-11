// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_IMAGE_SERVICE_IMAGE_SERVICE_H_
#define COMPONENTS_PAGE_IMAGE_SERVICE_IMAGE_SERVICE_H_

#include "base/functional/callback_forward.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"
#include "url/gurl.h"

namespace page_image_service {

// Base pure virtual interface for ImageService so that it can be mocked in
// tests.
class ImageService : public KeyedService {
 public:
  using ResultCallback = base::OnceCallback<void(const GURL& image_url)>;

  ImageService() = default;
  ImageService(const ImageService&) = delete;
  ImageService& operator=(const ImageService&) = delete;

  ~ImageService() override = default;

  // Fetches an image appropriate for `page_url`, returning the result
  // asynchronously to `callback`. The callback is always invoked. If there are
  // no images available, it is invoked with an empty GURL result.
  virtual void FetchImageFor(mojom::ClientId client_id,
                             const GURL& page_url,
                             const mojom::Options& options,
                             ResultCallback callback) = 0;

  // Gets a weak pointer to this service. Used when UIs want to create an
  // object whose lifetime might exceed the service.
  virtual base::WeakPtr<ImageService> GetWeakPtr() = 0;
};

}  // namespace page_image_service

#endif  // COMPONENTS_PAGE_IMAGE_SERVICE_IMAGE_SERVICE_H_
