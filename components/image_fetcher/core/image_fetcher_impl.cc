// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/image_fetcher_impl.h"

#include <string>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
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

ImageFetcherImpl::~ImageFetcherImpl() {}

ImageFetcherImpl::ImageRequest::ImageRequest() {}
ImageFetcherImpl::ImageRequest::ImageRequest(ImageRequest&& other) = default;

ImageFetcherImpl::ImageRequest::~ImageRequest() {}

void ImageFetcherImpl::SetDataUseServiceName(
    DataUseServiceName data_use_service_name) {
  image_data_fetcher_->SetDataUseServiceName(data_use_service_name);
}

void ImageFetcherImpl::SetDesiredImageFrameSize(const gfx::Size& size) {
  desired_image_frame_size_ = size;
}

void ImageFetcherImpl::SetImageDownloadLimit(
    base::Optional<int64_t> max_download_bytes) {
  image_data_fetcher_->SetImageDownloadLimit(max_download_bytes);
}

void ImageFetcherImpl::FetchImageAndData(
    const std::string& id,
    const GURL& image_url,
    ImageDataFetcherCallback image_data_callback,
    ImageFetcherCallback image_callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  // Before starting to fetch the image. Look for a request in progress for
  // |image_url|, and queue if appropriate.
  auto it = pending_net_requests_.find(image_url);
  if (it == pending_net_requests_.end()) {
    ImageRequest request;
    request.id = id;
    if (image_callback) {
      request.image_callbacks.push_back(std::move(image_callback));
    }
    if (image_data_callback) {
      request.image_data_callbacks.push_back(std::move(image_data_callback));
    }
    pending_net_requests_.emplace(image_url, std::move(request));

    image_data_fetcher_->FetchImageData(
        image_url,
        base::BindOnce(&ImageFetcherImpl::OnImageURLFetched,
                       base::Unretained(this), image_url),
        traffic_annotation);
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
      if (!request->image_data.empty()) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(image_data_callback), request->image_data,
                           request->request_metadata));
      } else {
        request->image_data_callbacks.push_back(std::move(image_data_callback));
      }
    }
  }
}

void ImageFetcherImpl::OnImageURLFetched(const GURL& image_url,
                                         const std::string& image_data,
                                         const RequestMetadata& metadata) {
  auto it = pending_net_requests_.find(image_url);
  DCHECK(it != pending_net_requests_.end());
  ImageRequest* request = &it->second;
  DCHECK(request->image_data.empty());
  DCHECK_EQ(RequestMetadata::RESPONSE_CODE_INVALID,
            request->request_metadata.http_response_code);
  for (auto& callback : request->image_data_callbacks) {
    std::move(callback).Run(image_data, metadata);
  }
  request->image_data_callbacks.clear();

  if (image_data.empty() || request->image_callbacks.empty()) {
    for (auto& callback : request->image_callbacks) {
      std::move(callback).Run(request->id, gfx::Image(), metadata);
    }
    pending_net_requests_.erase(it);
    return;
  }
  request->image_data = image_data;
  request->request_metadata = metadata;
  image_decoder_->DecodeImage(
      image_data, desired_image_frame_size_,
      base::BindRepeating(&ImageFetcherImpl::OnImageDecoded,
                          base::Unretained(this), image_url, metadata));
}

void ImageFetcherImpl::OnImageDecoded(const GURL& image_url,
                                      const RequestMetadata& metadata,
                                      const gfx::Image& image) {
  // Get request for the given image_url from the request queue.
  auto image_iter = pending_net_requests_.find(image_url);
  DCHECK(image_iter != pending_net_requests_.end());
  ImageRequest* request = &image_iter->second;

  // Run all image callbacks.
  for (auto& callback : request->image_callbacks) {
    std::move(callback).Run(request->id, image, metadata);
  }

  // Erase the completed ImageRequest.
  DCHECK(request->image_data_callbacks.empty());
  pending_net_requests_.erase(image_iter);
}

ImageDecoder* ImageFetcherImpl::GetImageDecoder() {
  return image_decoder_.get();
}

}  // namespace image_fetcher
