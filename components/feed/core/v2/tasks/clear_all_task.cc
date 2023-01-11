// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/tasks/clear_all_task.h"

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"

#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/public/persistent_key_value_store.h"

namespace feed {

ClearAllTask::ClearAllTask(FeedStream* stream) : stream_(*stream) {}
ClearAllTask::~ClearAllTask() = default;

void ClearAllTask::Run() {
  stream_->UnloadModels();
  stream_->GetPersistentKeyValueStore().ClearAll(base::DoNothing());
  stream_->GetStore().ClearAll(
      base::BindOnce(&ClearAllTask::StoreClearComplete, GetWeakPtr()));
}

void ClearAllTask::StoreClearComplete(bool ok) {
  DLOG_IF(ERROR, !ok) << "FeedStore::ClearAll failed";
  stream_->FinishClearAll();
  TaskComplete();
}

}  // namespace feed
