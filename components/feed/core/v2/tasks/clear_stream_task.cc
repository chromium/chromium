// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/tasks/clear_stream_task.h"

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"

#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/public/persistent_key_value_store.h"

namespace feed {

ClearStreamTask::ClearStreamTask(FeedStream* stream,
                                 const StreamType& stream_type)
    : stream_(*stream), stream_type_(stream_type) {}
ClearStreamTask::~ClearStreamTask() = default;

void ClearStreamTask::Run() {
  stream_->GetStore().ClearStreamData(
      stream_type_,
      base::BindOnce(&ClearStreamTask::StoreClearComplete, GetWeakPtr()));
}

void ClearStreamTask::StoreClearComplete(bool ok) {
  DLOG_IF(ERROR, !ok) << "FeedStore::ClearStream failed";
  stream_->FinishClearStream(stream_type_);
  TaskComplete();
}

}  // namespace feed
