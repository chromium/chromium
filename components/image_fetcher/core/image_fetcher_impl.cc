// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/image_fetcher_impl.h"

#include <string>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/gfx/image/image.h"

namespace image_fetcher {

ImageFetcherImpl::ImageFetcherImpl(
    std::unique_ptr<ImageDecoder> image_decoder,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory),
      image_decoder_(std::move(image_decoder)),
      image_data_fetcher_(new ImageDataFetcher(url_loader_factory)) {}

ImageFetcherImpl::~ImageFetcherImpl() = default;

ImageFetcherImpl::ImageRequest::ImageRequest() = default;
ImageFetcherImpl::ImageRequest::ImageRequest(ImageRequest&& other) = default;

ImageFetcherImpl::ImageRequest::~ImageRequest() = default;

void ImageFetcherImpl::FetchImageAndData(
    const GURL& image_url,
    ImageDataFetcherCallback image_data_callback,
    ImageFetcherCallback image_callback,
    ImageFetcherParams params) {
  const ImageRequestKey request_key{
      .image_url = image_url,
      .max_download_size = params.max_download_size(),
      .frame_width = params.frame_size().width(),
      .frame_height = params.frame_size().height(),
      .data_decoder = params.data_decoder()};
  // Share an in-flight request only when its download and decode options match.
  auto it = pending_net_requests_.find(request_key);
  if (it == pending_net_requests_.end()) {
    ImageRequest request;
    request.id = image_url.spec();
    if (image_callback) {
      request.image_callbacks.push_back(std::move(image_callback));
    }
    if (image_data_callback) {
      request.image_data_callbacks.push_back(std::move(image_data_callback));
    }
    pending_net_requests_.emplace(request_key, std::move(request));

    image_data_fetcher_->SetImageDownloadLimit(params.max_download_size());
    image_data_fetcher_->FetchImageData(
        image_url,
        base::BindOnce(&ImageFetcherImpl::OnImageURLFetched,
                       weak_ptr_factory_.GetWeakPtr(), request_key, params),
        params);
  } else {
    ImageRequest* request = &it->second;
    // Request in progress. Register as an interested callback.
    // TODO(treib,markusheintz): We're not guaranteed that the ID also matches.
    //                           We probably have to store them all.
    if (image_callback) {
      request->image_callbacks.push_back(std::move(image_callback));
    }
    // Call callback if data is already fetched, otherwise register it for
    // later.
    if (image_data_callback) {
      if (request->image_data_fetched) {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(&ImageFetcherImpl::RunImageDataCallback,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(image_data_callback), request->image_data,
                           request->request_metadata));
      } else {
        request->image_data_callbacks.push_back(std::move(image_data_callback));
      }
    }
  }
}

void ImageFetcherImpl::OnImageURLFetched(const ImageRequestKey& request_key,
                                         ImageFetcherParams params,
                                         const std::string& image_data,
                                         const RequestMetadata& metadata) {
  auto it = pending_net_requests_.find(request_key);
  CHECK(it != pending_net_requests_.end());
  ImageRequest* request = &it->second;
  DCHECK(!request->image_data_fetched);
  DCHECK_EQ(RequestMetadata::RESPONSE_CODE_INVALID,
            request->request_metadata.http_response_code);
  request->image_data = image_data;
  request->image_data_fetched = true;
  request->request_metadata = metadata;

  // Callbacks may enqueue more requests or destroy this fetcher. Publish the
  // fetched data first and drain a separate list to avoid invalidating it.
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  auto data_callbacks = std::move(request->image_data_callbacks);
  for (auto& callback : data_callbacks) {
    std::move(callback).Run(image_data, metadata);
    if (!weak_this) {
      return;
    }
  }

  if (image_data.empty() || request->image_callbacks.empty()) {
    auto image_callbacks = std::move(request->image_callbacks);
    pending_net_requests_.erase(it);
    for (auto& callback : image_callbacks) {
      std::move(callback).Run(gfx::Image(), metadata);
      if (!weak_this) {
        return;
      }
    }
    return;
  }
  image_decoder_->DecodeImage(image_data, params.frame_size(),
                              params.data_decoder(),
                              base::BindOnce(&ImageFetcherImpl::OnImageDecoded,
                                             weak_this, request_key, metadata));
}

void ImageFetcherImpl::OnImageDecoded(const ImageRequestKey& request_key,
                                      const RequestMetadata& metadata,
                                      const gfx::Image& image) {
  auto image_iter = pending_net_requests_.find(request_key);
  CHECK(image_iter != pending_net_requests_.end());
  // A callback may request the same image again. Remove this completed request
  // before invoking user code so the new request cannot join a finished decode.
  ImageRequest request = std::move(image_iter->second);
  pending_net_requests_.erase(image_iter);
  DCHECK(request.image_data_callbacks.empty());

  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  for (auto& callback : request.image_callbacks) {
    std::move(callback).Run(image, metadata);
    if (!weak_this) {
      return;
    }
  }
}

void ImageFetcherImpl::RunImageDataCallback(
    ImageDataFetcherCallback image_data_callback,
    std::string image_data,
    RequestMetadata request_metadata) {
  std::move(image_data_callback)
      .Run(std::move(image_data), std::move(request_metadata));
}

ImageDecoder* ImageFetcherImpl::GetImageDecoder() {
  return image_decoder_.get();
}

}  // namespace image_fetcher
