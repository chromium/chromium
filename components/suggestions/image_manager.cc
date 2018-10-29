// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/suggestions/image_manager.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/suggestions/image_encoder.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/gfx/image/image.h"

using leveldb_proto::ProtoDatabase;

namespace {

// Statistics are logged to UMA with this string as part of histogram name. They
// can all be found under LevelDB.*.ImageManager. Changing this needs to
// synchronize with histograms.xml, AND will also become incompatible with older
// browsers still reporting the previous values.
const char kDatabaseUMAClientName[] = "ImageManager";

std::unique_ptr<SkBitmap> DecodeImage(
    scoped_refptr<base::RefCountedMemory> encoded_data) {
  return suggestions::DecodeJPEGToSkBitmap(encoded_data->front(),
                                           encoded_data->size());
}

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("suggestions_image_manager", R"(
        semantics {
          sender: "Suggestions Service Thumbnail Fetch"
          description:
            "Retrieves thumbnails for site suggestions based on the user's "
            "synced browsing history, for use e.g. on the New Tab page."
          trigger:
            "Triggered when a thumbnail for a suggestion is required, and no "
            "local thumbnail is available."
          data: "The URL for which to retrieve a thumbnail."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can disable this feature by signing out of Chrome, or "
            "disabling Sync or History Sync in Chrome settings under 'Advanced "
            "sync settings...'. The feature is enabled by default."
        chrome_policy {
          SyncDisabled {
            policy_options {mode: MANDATORY}
            SyncDisabled: true
          }
        }
        chrome_policy {
          SigninAllowed {
            policy_options {mode: MANDATORY}
            SigninAllowed: false
          }
        }
      })");

}  // namespace

namespace suggestions {

ImageManager::ImageManager() : weak_ptr_factory_(this) {}

ImageManager::ImageManager(
    std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher,
    std::unique_ptr<ProtoDatabase<ImageData>> database,
    const base::FilePath& database_dir)
    : image_fetcher_(std::move(image_fetcher)),
      database_(std::move(database)),
      background_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::TaskPriority::USER_VISIBLE})),
      database_ready_(false),
      weak_ptr_factory_(this) {
  database_->Init(kDatabaseUMAClientName, database_dir,
                  leveldb_proto::CreateSimpleOptions(),
                  base::BindOnce(&ImageManager::OnDatabaseInit,
                                 weak_ptr_factory_.GetWeakPtr()));
}

ImageManager::~ImageManager() {}

ImageManager::ImageCacheRequest::ImageCacheRequest() {}

ImageManager::ImageCacheRequest::ImageCacheRequest(
    const ImageCacheRequest& other) = default;

ImageManager::ImageCacheRequest::~ImageCacheRequest() {}

void ImageManager::Initialize(const SuggestionsProfile& suggestions) {
  image_url_map_.clear();
  for (int i = 0; i < suggestions.suggestions_size(); ++i) {
    const ChromeSuggestion& suggestion = suggestions.suggestions(i);
    if (suggestion.has_thumbnail()) {
      image_url_map_[GURL(suggestion.url())] = GURL(suggestion.thumbnail());
    }
  }
}

void ImageManager::AddImageURL(const GURL& url, const GURL& image_url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  image_url_map_[url] = image_url;
}

void ImageManager::GetImageForURL(const GURL& url, ImageCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // If |url| is not found in |image_url_map_|, then invoke |callback| with
  // NULL since there is no associated image for this |url|.
  GURL image_url;
  if (!GetImageURL(url, &image_url)) {
    callback.Run(url, gfx::Image());
    return;
  }

  // |database_| can be NULL if something went wrong in initialization.
  if (database_.get() && !database_ready_) {
    // Once database is initialized, it will serve pending requests from either
    // cache or network.
    QueueCacheRequest(url, image_url, callback);
    return;
  }

  ServeFromCacheOrNetwork(url, image_url, callback);
}

void ImageManager::SaveImageAndForward(
    const ImageCallback& image_callback,
    const std::string& url,
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& metadata) {
  // |image| can be empty if image fetch was unsuccessful.
  if (!image.IsEmpty())
    SaveImage(url, *image.ToSkBitmap());

  image_callback.Run(GURL(url), image);
}

bool ImageManager::GetImageURL(const GURL& url, GURL* image_url) {
  DCHECK(image_url);
  auto it = image_url_map_.find(url);
  if (it == image_url_map_.end())
    return false;  // Not found.
  *image_url = it->second;
  return true;
}

void ImageManager::QueueCacheRequest(const GURL& url,
                                     const GURL& image_url,
                                     ImageCallback callback) {
  // To be served when the database has loaded.
  auto it = pending_cache_requests_.find(url);
  if (it != pending_cache_requests_.end()) {
    // Request already queued for this url.
    it->second.callbacks.push_back(callback);
    return;
  }

  ImageCacheRequest request;
  request.url = url;
  request.image_url = image_url;
  request.callbacks.push_back(callback);
  pending_cache_requests_[url] = request;
}

void ImageManager::OnCacheImageDecoded(const GURL& url,
                                       const GURL& image_url,
                                       const ImageCallback& callback,
                                       std::unique_ptr<SkBitmap> bitmap) {
  if (bitmap) {
    callback.Run(url, gfx::Image::CreateFrom1xBitmap(*bitmap));
  } else {
    image_fetcher_->FetchImage(
        url.spec(), image_url,
        base::BindRepeating(&ImageManager::SaveImageAndForward,
                            base::Unretained(this), callback),
        kTrafficAnnotation);
  }
}

scoped_refptr<base::RefCountedMemory> ImageManager::GetEncodedImageFromCache(
    const GURL& url) {
  auto image_iter = image_map_.find(url.spec());
  if (image_iter != image_map_.end()) {
    return image_iter->second;
  }
  return nullptr;
}

void ImageManager::ServeFromCacheOrNetwork(const GURL& url,
                                           const GURL& image_url,
                                           ImageCallback callback) {
  scoped_refptr<base::RefCountedMemory> encoded_data =
      GetEncodedImageFromCache(url);
  if (encoded_data) {
    base::PostTaskAndReplyWithResult(
        background_task_runner_.get(), FROM_HERE,
        base::Bind(&DecodeImage, encoded_data),
        base::Bind(&ImageManager::OnCacheImageDecoded,
                   weak_ptr_factory_.GetWeakPtr(), url, image_url, callback));
  } else {
    image_fetcher_->FetchImage(
        url.spec(), image_url,
        base::BindRepeating(&ImageManager::SaveImageAndForward,
                            base::Unretained(this), callback),
        kTrafficAnnotation);
  }
}

void ImageManager::SaveImage(const std::string& url, const SkBitmap& bitmap) {
  scoped_refptr<base::RefCountedBytes> encoded_data(
      new base::RefCountedBytes());
  // TODO(treib): Should encoding happen on the |background_task_runner_|?
  // *De*coding happens there.
  if (!EncodeSkBitmapToJPEG(bitmap, &encoded_data->data())) {
    return;
  }

  // Update the image map.
  image_map_.insert({url, encoded_data});

  if (!database_ready_)
    return;

  // Save the resulting bitmap to the database.
  ImageData data;
  data.set_url(url);
  data.set_data(encoded_data->front(), encoded_data->size());
  std::unique_ptr<ProtoDatabase<ImageData>::KeyEntryVector> entries_to_save(
      new ProtoDatabase<ImageData>::KeyEntryVector());
  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());
  entries_to_save->push_back(std::make_pair(data.url(), data));
  database_->UpdateEntries(std::move(entries_to_save),
                           std::move(keys_to_remove),
                           base::BindOnce(&ImageManager::OnDatabaseSave,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void ImageManager::OnDatabaseInit(bool success) {
  if (!success) {
    DVLOG(1) << "Image database init failed.";
    database_.reset();
    ServePendingCacheRequests();
    return;
  }
  database_->LoadEntries(base::BindOnce(&ImageManager::OnDatabaseLoad,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void ImageManager::OnDatabaseLoad(bool success,
                                  std::unique_ptr<ImageDataVector> entries) {
  if (!success) {
    DVLOG(1) << "Image database load failed.";
    database_.reset();
    ServePendingCacheRequests();
    return;
  }
  database_ready_ = true;

  LoadEntriesInCache(std::move(entries));
  ServePendingCacheRequests();
}

void ImageManager::OnDatabaseSave(bool success) {
  if (!success) {
    DVLOG(1) << "Image database save failed.";
    database_.reset();
    database_ready_ = false;
  }
}

void ImageManager::LoadEntriesInCache(
    std::unique_ptr<ImageDataVector> entries) {
  for (auto it = entries->begin(); it != entries->end(); ++it) {
    std::vector<unsigned char> encoded_data(it->data().begin(),
                                            it->data().end());

    image_map_.insert(
        {it->url(), base::RefCountedBytes::TakeVector(&encoded_data)});
  }
}

void ImageManager::ServePendingCacheRequests() {
  for (auto it = pending_cache_requests_.begin();
       it != pending_cache_requests_.end(); ++it) {
    const ImageCacheRequest& request = it->second;
    for (auto callback_it = request.callbacks.begin();
         callback_it != request.callbacks.end(); ++callback_it) {
      ServeFromCacheOrNetwork(request.url, request.image_url, *callback_it);
    }
  }
  pending_cache_requests_.clear();
}

}  // namespace suggestions
