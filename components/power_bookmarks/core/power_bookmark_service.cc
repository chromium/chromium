// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/core/power_bookmark_service.h"

#include "base/ranges/algorithm.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace power_bookmarks {

PowerBookmarkService::PowerBookmarkService(BookmarkModel* model)
    : model_(model) {
  if (model_)
    model_->AddObserver(this);
}

PowerBookmarkService::~PowerBookmarkService() {
  if (model_)
    model_->RemoveObserver(this);
}

void PowerBookmarkService::AddDataProvider(
    PowerBookmarkDataProvider* data_provider) {
  data_providers_.emplace_back(data_provider);
}

void PowerBookmarkService::RemoveDataProvider(
    PowerBookmarkDataProvider* data_provider) {
  auto it = base::ranges::find(data_providers_, data_provider);
  if (it != data_providers_.end())
    data_providers_.erase(it);
}

void PowerBookmarkService::BookmarkNodeAdded(BookmarkModel* model,
                                             const BookmarkNode* parent,
                                             size_t index,
                                             bool newly_added) {
  if (!newly_added)
    return;

  const BookmarkNode* node = parent->children()[index].get();
  std::unique_ptr<PowerBookmarkMeta> meta =
      std::make_unique<PowerBookmarkMeta>();

  for (auto* data_provider : data_providers_) {
    data_provider->AttachMetadataForNewBookmark(node, meta.get());
  }

  SetNodePowerBookmarkMeta(model, node, std::move(meta));
}

}  // namespace power_bookmarks