// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/tasks/prefetch_images_task.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "components/feed/core/proto/v2/wire/stream_structure.pb.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/tasks/load_stream_from_store_task.h"

namespace feed {
namespace {

// Converts a URL string into a GURL. If the string is not a valid URL, returns
// an empty GURL. Since GURL::spec() asserts on invalid URLs, this is necessary
// to scrub the incoming data from the wire.
GURL SpecToGURL(const std::string& url_string) {
  GURL url(url_string);
  if (!url.is_valid())
    url = GURL();
  return url;
}

}  // namespace

PrefetchImagesTask::PrefetchImagesTask(FeedStream* stream) : stream_(*stream) {
  max_images_per_refresh_ =
      GetFeedConfig().max_prefetch_image_requests_per_refresh;
}

PrefetchImagesTask::~PrefetchImagesTask() = default;

void PrefetchImagesTask::Run() {
  if (stream_->ClearAllInProgress()) {
    // Abort if ClearAll is in progress.
    TaskComplete();
    return;
  }
  StreamType for_you_stream = StreamType(StreamKind::kForYou);
  if (stream_->GetModel(for_you_stream)) {
    PrefetchImagesFromModel(*stream_->GetModel(for_you_stream));
    return;
  }

  // Web feed subscriber is set to true so we don't use the less restrictive
  // staleness number for when there are no subscriptions.
  load_from_store_task_ = std::make_unique<LoadStreamFromStoreTask>(
      LoadStreamFromStoreTask::LoadType::kFullLoad, &*stream_, for_you_stream,
      &stream_->GetStore(),
      /*missed_last_refresh=*/false,
      /*is_web_feed_subscriber=*/true,
      base::BindOnce(&PrefetchImagesTask::LoadStreamComplete,
                     base::Unretained(this)));

  load_from_store_task_->Execute(base::DoNothing());
}

void PrefetchImagesTask::LoadStreamComplete(
    LoadStreamFromStoreTask::Result result) {
  if (!result.update_request) {
    TaskComplete();
    return;
  }

  // It is a bit dangerous to retain the model loaded here. The normal
  // LoadStreamTask flow has various considerations for metrics and signalling
  // surfaces to update. For this reason, we're not going to retain the loaded
  // model for use outside of this task.
  StreamModel::Context model_context;
  StreamModel model(&model_context, LoggingParameters{});
  model.Update(std::move(result.update_request));
  PrefetchImagesFromModel(model);
}

void PrefetchImagesTask::PrefetchImagesFromModel(const StreamModel& model) {
  for (ContentRevision rev : model.GetContentList()) {
    const feedstore::Content* content = model.FindContent(rev);
    if (!content)
      continue;
    for (const feedwire::PrefetchMetadata& metadata :
         content->prefetch_metadata()) {
      MaybePrefetchImage(SpecToGURL(metadata.image_url()));
      MaybePrefetchImage(SpecToGURL(metadata.favicon_url()));
      for (const std::string& url : metadata.additional_image_urls()) {
        MaybePrefetchImage(SpecToGURL(url));
      }
    }
  }

  TaskComplete();
}

void PrefetchImagesTask::MaybePrefetchImage(const GURL& gurl) {
  // If we've already fetched this url, or we've hit the max number of fetches,
  // then don't send a fetch request.
  if (!gurl.is_valid() ||
      (previously_fetched_.find(gurl.spec()) != previously_fetched_.end()) ||
      previously_fetched_.size() >= max_images_per_refresh_)
    return;
  previously_fetched_.insert(gurl.spec());
  stream_->PrefetchImage(gurl);
}

}  // namespace feed
