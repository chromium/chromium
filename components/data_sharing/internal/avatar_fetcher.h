// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_AVATAR_FETCHER_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_AVATAR_FETCHER_H_

#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "url/gurl.h"

namespace image_fetcher {
class ImageFetcher;
struct RequestMetadata;
}  // namespace image_fetcher

namespace gfx {
class Image;
}  // namespace gfx

namespace data_sharing {
// Wrapper that fetches avatar images given avatar urls. Depends on client
// passed in image fecther to do the fetching. Owned by DataSharingServiceImpl.
// TODO(crbug.com/381287587): Add access token support to ImageFetcher to handle
// the case where the requestee sets profile picture as private.
class AvatarFetcher {
 public:
  using ImageCallback = base::OnceCallback<void(const gfx::Image&)>;

  explicit AvatarFetcher();
  ~AvatarFetcher();
  AvatarFetcher(const AvatarFetcher& other) = delete;
  AvatarFetcher& operator=(const AvatarFetcher& other) = delete;

  // Performs a request for the avatar at the given URL. |callback| is
  // triggered with the result of the request, whether the returned
  // image is empty or not. It is expected that each platform performs
  // its own logic for rendering a placeholder when the image is empty,
  // due to the need for theme-aware colors.
  void Fetch(const GURL& avatar_url,
             int size,
             ImageCallback callback,
             image_fetcher::ImageFetcher* image_fetcher);

 private:
  void OnImageFetched(ImageCallback callback,
                      const gfx::Image& image,
                      const image_fetcher::RequestMetadata& metadata);

  base::WeakPtrFactory<AvatarFetcher> weak_ptr_factory_{this};
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_AVATAR_FETCHER_H_
