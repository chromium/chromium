// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/core/power_bookmark_service.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/task/sequenced_task_runner.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/power_bookmarks/core/power_bookmark_data_provider.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace power_bookmarks {

PowerBookmarkService::PowerBookmarkService(BookmarkModel* model)
    : model_(model) {
  if (model_) {
    model_observation_.Observe(model_);
  }
}

PowerBookmarkService::~PowerBookmarkService() = default;

void PowerBookmarkService::AddDataProvider(
    PowerBookmarkDataProvider* data_provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_providers_.emplace_back(data_provider);
}

void PowerBookmarkService::RemoveDataProvider(
    PowerBookmarkDataProvider* data_provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = std::ranges::find(data_providers_, data_provider);
  if (it != data_providers_.end())
    data_providers_.erase(it);
}

void PowerBookmarkService::BookmarkNodeAdded(const BookmarkNode* parent,
                                             size_t index,
                                             bool newly_added) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!newly_added)
    return;

  const BookmarkNode* node = parent->children()[index].get();
  std::unique_ptr<PowerBookmarkMeta> meta =
      std::make_unique<PowerBookmarkMeta>();

  for (power_bookmarks::PowerBookmarkDataProvider* data_provider :
       data_providers_) {
    data_provider->AttachMetadataForNewBookmark(node, meta.get());
  }

  SetNodePowerBookmarkMeta(model_, node, std::move(meta));
}

}  // namespace power_bookmarks
