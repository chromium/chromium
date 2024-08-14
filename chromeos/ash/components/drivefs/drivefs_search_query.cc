// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_search_query.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/drivefs/drivefs_search_query_delegate.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "components/drive/file_errors.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace drivefs {

namespace {

bool IsCloudSharedWithMeQuery(const drivefs::mojom::QueryParametersPtr& query) {
  return query->query_source ==
             drivefs::mojom::QueryParameters::QuerySource::kCloudOnly &&
         query->shared_with_me && !query->text_content && !query->title;
}

}  // namespace

DriveFsSearchQuery::DriveFsSearchQuery(
    base::WeakPtr<DriveFsSearchQueryDelegate> delegate,
    mojom::QueryParametersPtr query)
    : delegate_(std::move(delegate)), query_(std::move(query)) {
  Init();
}

DriveFsSearchQuery::~DriveFsSearchQuery() = default;

mojom::QueryParameters::QuerySource DriveFsSearchQuery::source() {
  return query_->query_source;
}

void DriveFsSearchQuery::Init() {
  remote_.reset();

  if (delegate_ == nullptr) {
    return;
  }

  // The only cacheable query is 'shared with me'.
  if (IsCloudSharedWithMeQuery(query_)) {
    // Check if we should have the response cached.
    if (delegate_->WithinQueryCacheTtl()) {
      query_->query_source =
          drivefs::mojom::QueryParameters::QuerySource::kLocalOnly;
    }
  }

  if (delegate_->IsOffline() &&
      query_->query_source !=
          drivefs::mojom::QueryParameters::QuerySource::kLocalOnly) {
    // No point trying cloud query if we know we are offline.
    AdjustQueryForOffline();
  }

  delegate_->StartMojoSearchQuery(remote_.BindNewPipeAndPassReceiver(),
                                  query_.Clone());
}

void DriveFsSearchQuery::GetNextPage(
    mojom::SearchQuery::GetNextPageCallback callback) {
  // `remote_` might not be bound if `delegate_` was null when `Init()` was
  // called.
  if (remote_.is_bound()) {
    remote_->GetNextPage(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
        base::BindOnce(&DriveFsSearchQuery::OnGetNextPage,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        drive::FILE_ERROR_ABORT, std::nullopt));
  } else {
    OnGetNextPage(std::move(callback), drive::FILE_ERROR_ABORT, std::nullopt);
  }
}

void DriveFsSearchQuery::OnGetNextPage(
    mojom::SearchQuery::GetNextPageCallback callback,
    drive::FileError error,
    std::optional<std::vector<drivefs::mojom::QueryItemPtr>> items) {
  if (!first_page_returned_ && error == drive::FILE_ERROR_NO_CONNECTION &&
      query_->query_source !=
          drivefs::mojom::QueryParameters::QuerySource::kLocalOnly) {
    // Retry with offline query only if we have never returned any pages.
    AdjustQueryForOffline();
    Init();
    GetNextPage(std::move(callback));
    return;
  }

  if (delegate_ != nullptr && error == drive::FILE_ERROR_OK &&
      IsCloudSharedWithMeQuery(query_)) {
    // Mark that DriveFS should have cached the required info.
    delegate_->UpdateLastSharedWithMeResponse();
  }

  if (drive::IsFileErrorOk(error)) {
    first_page_returned_ = true;
  }

  std::move(callback).Run(error, std::move(items));
}

void DriveFsSearchQuery::AdjustQueryForOffline() {
  query_->query_source =
      drivefs::mojom::QueryParameters::QuerySource::kLocalOnly;
  if (query_->text_content) {
    // Full-text searches not supported offline.
    std::swap(query_->text_content, query_->title);
    query_->text_content.reset();
  }
}

}  // namespace drivefs
