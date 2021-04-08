// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/tasks/wait_for_store_initialize_task.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/feed_stream.h"

namespace feed {

WaitForStoreInitializeTask::WaitForStoreInitializeTask(
    FeedStore* store,
    FeedStream* stream,
    base::OnceCallback<void(Result)> callback)
    : store_(store), stream_(stream), callback_(std::move(callback)) {}
WaitForStoreInitializeTask::~WaitForStoreInitializeTask() = default;

void WaitForStoreInitializeTask::Run() {
  // |this| stays alive as long as the |store_|, so Unretained is safe.
  store_->Initialize(base::BindOnce(
      &WaitForStoreInitializeTask::OnStoreInitialized, base::Unretained(this)));
}

void WaitForStoreInitializeTask::OnStoreInitialized() {
  store_->ReadMetadata(base::BindOnce(
      &WaitForStoreInitializeTask::OnMetadataLoaded, base::Unretained(this)));
  store_->ReadWebFeedStartupData(
      base::BindOnce(&WaitForStoreInitializeTask::WebFeedStartupDataDone,
                     base::Unretained(this)));
}

void WaitForStoreInitializeTask::OnMetadataLoaded(
    std::unique_ptr<feedstore::Metadata> metadata) {
  if (metadata && metadata->gaia() != stream_->GetSyncSignedInGaia()) {
    store_->ClearAll(base::BindOnce(&WaitForStoreInitializeTask::ClearAllDone,
                                    base::Unretained(this)));
    return;
  }
  MaybeUpgradeStreamSchema(std::move(metadata));
}

void WaitForStoreInitializeTask::ClearAllDone(bool clear_ok) {
  DLOG_IF(ERROR, !clear_ok) << "FeedStore::ClearAll failed";
  // ClearAll just wiped metadata, so send nullptr.
  MaybeUpgradeStreamSchema(nullptr);
}

void WaitForStoreInitializeTask::MaybeUpgradeStreamSchema(
    std::unique_ptr<feedstore::Metadata> metadata) {
  if (!metadata || metadata->stream_schema_version() != 1) {
    if (!metadata) {
      metadata = std::make_unique<feedstore::Metadata>();
      metadata->set_gaia(stream_->GetSyncSignedInGaia());
    }
    store_->UpgradeFromStreamSchemaV0(
        std::move(*metadata),
        base::BindOnce(&WaitForStoreInitializeTask::MetadataDone,
                       base::Unretained(this)));
    return;
  }
  MetadataDone(std::move(*metadata));
}

void WaitForStoreInitializeTask::MetadataDone(feedstore::Metadata metadata) {
  result_.metadata = std::move(metadata);
  Done();
}

void WaitForStoreInitializeTask::WebFeedStartupDataDone(
    FeedStore::WebFeedStartupData data) {
  result_.web_feed_startup_data = std::move(data);
  Done();
}

void WaitForStoreInitializeTask::Done() {
  if (++done_count_ == 2) {
    std::move(callback_).Run(std::move(result_));
    TaskComplete();
  }
}

}  // namespace feed
