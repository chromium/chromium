// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/tasks/wait_for_store_initialize_task.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/feed_stream.h"

namespace feed {

WaitForStoreInitializeTask::WaitForStoreInitializeTask(FeedStream* stream)
    : stream_(stream), store_(stream->GetStore()) {}
WaitForStoreInitializeTask::~WaitForStoreInitializeTask() = default;

void WaitForStoreInitializeTask::Run() {
  // |this| stays alive as long as the |store_|, so Unretained is safe.
  store_->Initialize(base::BindOnce(
      &WaitForStoreInitializeTask::OnStoreInitialized, base::Unretained(this)));
}

void WaitForStoreInitializeTask::OnStoreInitialized() {
  store_->ReadMetadata(base::BindOnce(
      &WaitForStoreInitializeTask::OnMetadataLoaded, base::Unretained(this)));
}

void WaitForStoreInitializeTask::OnMetadataLoaded(
    std::unique_ptr<feedstore::Metadata> metadata) {
  if (!metadata || metadata->stream_schema_version() != 1) {
    if (!metadata) {
      metadata = std::make_unique<feedstore::Metadata>();
    }
    store_->UpgradeFromStreamSchemaV0(
        std::move(*metadata), base::BindOnce(&WaitForStoreInitializeTask::Done,
                                             base::Unretained(this)));
    return;
  }
  Done(std::move(*metadata));
}

void WaitForStoreInitializeTask::Done(feedstore::Metadata metadata) {
  stream_->GetMetadata()->Populate(std::move(metadata));
  TaskComplete();
}

}  // namespace feed
