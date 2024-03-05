// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/upload_list/upload_list.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

namespace {

// USER_VISIBLE because loading uploads blocks chrome://crashes,
// chrome://webrtc-logs and the feedback UI. See https://crbug.com/972526.
constexpr base::TaskTraits kLoadingTaskTraits = {
    base::MayBlock(), base::TaskPriority::USER_BLOCKING,
    base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};

}  // namespace

UploadList::UploadInfo::UploadInfo(const std::string& upload_id,
                                   const base::Time& upload_time,
                                   const std::string& local_id,
                                   const base::Time& capture_time,
                                   State state)
    : upload_id(upload_id),
      upload_time(upload_time),
      local_id(local_id),
      capture_time(capture_time),
      state(state) {}

UploadList::UploadInfo::UploadInfo(const std::string& local_id,
                                   const base::Time& capture_time,
                                   State state,
                                   int64_t file_size)
    : local_id(local_id),
      capture_time(capture_time),
      state(state),
      file_size(file_size) {}

UploadList::UploadInfo::UploadInfo(const std::string& upload_id,
                                   const base::Time& upload_time)
    : upload_id(upload_id), upload_time(upload_time), state(State::Uploaded) {}

UploadList::UploadInfo::UploadInfo(const UploadInfo& upload_info) = default;

UploadList::UploadInfo::~UploadInfo() = default;

UploadList::UploadList() = default;

UploadList::~UploadList() = default;

void UploadList::Load(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  load_callback_ = std::move(callback);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kLoadingTaskTraits,
      base::BindOnce(&UploadList::LoadUploadList, this),
      base::BindOnce(&UploadList::OnLoadComplete, this));
}

void UploadList::Clear(const base::Time& begin,
                       const base::Time& end,
                       base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  clear_callback_ = std::move(callback);
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, kLoadingTaskTraits,
      base::BindOnce(&UploadList::ClearUploadList, this, begin, end),
      base::BindOnce(&UploadList::OnClearComplete, this));
}

void UploadList::CancelLoadCallback() {
  load_callback_.Reset();
}

void UploadList::RequestSingleUploadAsync(const std::string& local_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTask(
      FROM_HERE, kLoadingTaskTraits,
      base::BindOnce(&UploadList::RequestSingleUpload, this, local_id));
}

std::vector<const UploadList::UploadInfo*> UploadList::GetUploads(
    size_t max_count) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<const UploadInfo*> uploads;
  const size_t copied_size = std::min(uploads_.size(), max_count);
  uploads.reserve(copied_size);
  for (size_t i = 0; i < copied_size; ++i) {
    uploads.push_back(uploads_[i].get());
  }
  return uploads;
}

void UploadList::OnLoadComplete(
    std::vector<std::unique_ptr<UploadInfo>> uploads) {
  uploads_ = std::move(uploads);
  if (!load_callback_.is_null()) {
    std::move(load_callback_).Run();
  }
}

void UploadList::OnClearComplete() {
  if (!clear_callback_.is_null())
    std::move(clear_callback_).Run();
}
