// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/tasks/get_prefetch_suggestions_task.h"

#include <utility>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "components/feed/core/proto/v2/wire/stream_structure.pb.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/feed_stream.h"
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

offline_pages::PrefetchSuggestion ConvertToSuggestion(
    const feedwire::PrefetchMetadata& metadata) {
  offline_pages::PrefetchSuggestion result;
  result.article_url = SpecToGURL(metadata.uri());
  result.article_title = metadata.title();
  result.article_attribution = metadata.publisher();
  result.article_snippet = metadata.snippet();
  result.thumbnail_url = SpecToGURL(metadata.image_url());
  result.favicon_url = SpecToGURL(metadata.favicon_url());
  return result;
}

}  // namespace

GetPrefetchSuggestionsTask::GetPrefetchSuggestionsTask(
    FeedStream* stream,
    base::OnceCallback<void(std::vector<offline_pages::PrefetchSuggestion>)>
        result_callback)
    : stream_(stream), result_callback_(std::move(result_callback)) {}

GetPrefetchSuggestionsTask::~GetPrefetchSuggestionsTask() = default;

void GetPrefetchSuggestionsTask::Run() {
  if (stream_->ClearAllInProgress()) {
    // Abort and return an empty list.
    std::move(result_callback_).Run({});
    TaskComplete();
    return;
  }

  if (stream_->GetModel(kForYouStream)) {
    PullSuggestionsFromModel(*stream_->GetModel(kForYouStream));
    return;
  }

  load_from_store_task_ = std::make_unique<LoadStreamFromStoreTask>(
      LoadStreamFromStoreTask::LoadType::kFullLoad, kForYouStream,
      stream_->GetStore(),
      /*missed_last_refresh=*/false,
      base::BindOnce(&GetPrefetchSuggestionsTask::LoadStreamComplete,
                     base::Unretained(this)));

  load_from_store_task_->Execute(base::DoNothing());
}

void GetPrefetchSuggestionsTask::LoadStreamComplete(
    LoadStreamFromStoreTask::Result result) {
  if (!result.update_request) {
    // Give up and return an empty list.
    std::move(result_callback_).Run({});
    TaskComplete();
    return;
  }

  // It is a bit dangerous to retain the model loaded here. The normal
  // LoadStreamTask flow has various considerations for metrics and signalling
  // surfaces to update. For this reason, we're not going to retain the loaded
  // model for use outside of this task.
  StreamModel model;
  model.Update(std::move(result.update_request));
  PullSuggestionsFromModel(model);
}

void GetPrefetchSuggestionsTask::PullSuggestionsFromModel(
    const StreamModel& model) {
  std::vector<offline_pages::PrefetchSuggestion> suggestions;
  for (ContentRevision rev : model.GetContentList()) {
    const feedstore::Content* content = model.FindContent(rev);
    if (!content)
      continue;
    for (const feedwire::PrefetchMetadata& metadata :
         content->prefetch_metadata()) {
      suggestions.push_back(ConvertToSuggestion(metadata));
    }
  }

  std::move(result_callback_).Run(std::move(suggestions));
  TaskComplete();
}

}  // namespace feed
