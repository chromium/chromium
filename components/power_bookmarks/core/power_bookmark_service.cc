// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/core/power_bookmark_service.h"

#include "base/ranges/algorithm.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/power_bookmarks/core/power_bookmark_data_provider.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/powers/power.h"
#include "components/power_bookmarks/core/powers/power_overview.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/storage/power_bookmark_backend.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace power_bookmarks {

PowerBookmarkService::PowerBookmarkService(
    BookmarkModel* model,
    const base::FilePath& database_dir,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
    : model_(model), backend_task_runner_(backend_task_runner) {
  if (model_)
    model_->AddObserver(this);

  backend_ = base::SequenceBound<PowerBookmarkBackend>(backend_task_runner_,
                                                       database_dir);
  // Features that wish to use the real database, must call
  // `InitPowerBookmarkDatabase`.
  backend_.AsyncCall(&PowerBookmarkBackend::Init)
      .WithArgs(/*use_database=*/false);
}

PowerBookmarkService::~PowerBookmarkService() {
  if (model_)
    model_->RemoveObserver(this);

  backend_.AsyncCall(&PowerBookmarkBackend::Shutdown);
  backend_task_runner_ = nullptr;
}

void PowerBookmarkService::InitPowerBookmarkDatabase() {
  backend_.AsyncCall(&PowerBookmarkBackend::Init)
      .WithArgs(/*use_database=*/true);
}

void PowerBookmarkService::GetPowersForURL(const GURL& url,
                                           const PowerType& power_type,
                                           PowersCallback callback) {
  backend_.AsyncCall(&PowerBookmarkBackend::GetPowersForURL)
      .WithArgs(url, power_type)
      .Then(std::move(callback));
}

void PowerBookmarkService::GetPowerOverviewsForType(
    const PowerType& power_type,
    PowerOverviewsCallback callback) {
  backend_.AsyncCall(&PowerBookmarkBackend::GetPowerOverviewsForType)
      .WithArgs(power_type)
      .Then(std::move(callback));
}

void PowerBookmarkService::CreatePower(std::unique_ptr<Power> power,
                                       SuccessCallback callback) {
  backend_.AsyncCall(&PowerBookmarkBackend::CreatePower)
      .WithArgs(std::move(power))
      .Then(std::move(callback));
}

void PowerBookmarkService::UpdatePower(std::unique_ptr<Power> power,
                                       SuccessCallback callback) {
  backend_.AsyncCall(&PowerBookmarkBackend::UpdatePower)
      .WithArgs(std::move(power))
      .Then(std::move(callback));
}

void PowerBookmarkService::DeletePower(const base::GUID& guid,
                                       SuccessCallback callback) {
  backend_.AsyncCall(&PowerBookmarkBackend::DeletePower)
      .WithArgs(guid)
      .Then(std::move(callback));
}

void PowerBookmarkService::DeletePowersForURL(const GURL& url,
                                              const PowerType& power_type,
                                              SuccessCallback callback) {
  backend_.AsyncCall(&PowerBookmarkBackend::DeletePowersForURL)
      .WithArgs(url, power_type)
      .Then(std::move(callback));
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