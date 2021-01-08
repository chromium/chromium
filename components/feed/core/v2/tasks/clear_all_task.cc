// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/tasks/clear_all_task.h"

#include "base/callback.h"
#include "base/logging.h"

#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/feed_stream.h"

namespace feed {

ClearAllTask::ClearAllTask(FeedStream* stream) : stream_(stream) {}
ClearAllTask::~ClearAllTask() = default;

void ClearAllTask::Run() {
  stream_->UnloadModel();
  stream_->GetStore()->ClearAll(
      base::BindOnce(&ClearAllTask::StoreClearComplete, GetWeakPtr()));
}

void ClearAllTask::StoreClearComplete(bool ok) {
  DLOG_IF(ERROR, !ok) << "FeedStore::ClearAll failed";
  stream_->FinishClearAll();
  if (stream_->HasSurfaceAttached()) {
    stream_->TriggerStreamLoad();
  }
  TaskComplete();
}

}  // namespace feed
