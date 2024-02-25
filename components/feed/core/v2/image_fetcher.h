// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_IMAGE_FETCHER_H_
#define COMPONENTS_FEED_CORE_V2_IMAGE_FETCHER_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/feed/core/v2/public/types.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace feed {

struct NetworkResponse;

// Fetcher object to retrieve an image resource from a URL.
class ImageFetcher {
 public:
  using ImageCallback = base::OnceCallback<void(NetworkResponse)>;
  explicit ImageFetcher(
      scoped_refptr<::network::SharedURLLoaderFactory> url_loader_factory);
  virtual ~ImageFetcher();
  ImageFetcher(const ImageFetcher&) = delete;
  ImageFetcher& operator=(const ImageFetcher&) = delete;

  virtual ImageFetchId Fetch(const GURL& url, ImageCallback callback);
  virtual void Cancel(ImageFetchId id);

 private:
  struct PendingRequest {
    PendingRequest(std::unique_ptr<network::SimpleURLLoader> loader,
                   ImageCallback callback);
    PendingRequest(PendingRequest&& other);
    PendingRequest& operator=(PendingRequest&& other);
    ~PendingRequest();

    std::unique_ptr<network::SimpleURLLoader> loader;
    ImageCallback callback;
  };

  // Called when fetch request completes.
  void OnFetchComplete(ImageFetchId id,
                       const GURL& url,
                       std::unique_ptr<std::string> response_data);

  std::optional<PendingRequest> RemovePending(ImageFetchId id);

  uint64_t GetTrackId(ImageFetchId id) const;

  ImageFetchId::Generator id_generator_;
  base::flat_map<ImageFetchId, PendingRequest> pending_requests_;
  const scoped_refptr<::network::SharedURLLoaderFactory> url_loader_factory_;
  base::WeakPtrFactory<ImageFetcher> weak_factory_{this};
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_IMAGE_FETCHER_H_
