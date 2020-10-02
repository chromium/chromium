// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/federated_learning/floc_blocklist_service.h"

#include <utility>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "components/federated_learning/floc_constants.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace federated_learning {

namespace {

class CopyingFileInputStream : public google::protobuf::io::CopyingInputStream {
 public:
  explicit CopyingFileInputStream(base::File file) : file_(std::move(file)) {}

  CopyingFileInputStream(const CopyingFileInputStream&) = delete;
  CopyingFileInputStream& operator=(const CopyingFileInputStream&) = delete;

  ~CopyingFileInputStream() override = default;

  // google::protobuf::io::CopyingInputStream:
  int Read(void* buffer, int size) override {
    return file_.ReadAtCurrentPosNoBestEffort(static_cast<char*>(buffer), size);
  }

 private:
  base::File file_;
};

FlocId FilterByBlocklistOnBackgroundThread(const FlocId& unfiltered_floc,
                                           const base::FilePath& file_path) {
  DCHECK(unfiltered_floc.IsValid());
  base::File blocklist_file(file_path,
                            base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!blocklist_file.IsValid())
    return FlocId();

  CopyingFileInputStream copying_stream(std::move(blocklist_file));
  google::protobuf::io::CopyingInputStreamAdaptor zero_copy_stream_adaptor(
      &copying_stream);

  google::protobuf::io::CodedInputStream input_stream(
      &zero_copy_stream_adaptor);

  const uint64_t kMaxPossibleFloc = (1ULL << kMaxNumberOfBitsInFloc) - 1;

  uint64_t next;
  while (input_stream.ReadVarint64(&next)) {
    if (next > kMaxPossibleFloc) {
      // A sign of a corrupted file. Block the floc to be safe.
      // TODO: add metrics
      return FlocId();
    }

    if (unfiltered_floc.ToUint64() == next) {
      // The floc should be blocked.
      return FlocId();
    }
  }

  // The floc should be not blocked.
  return unfiltered_floc;
}

}  // namespace

FlocBlocklistService::FlocBlocklistService()
    : background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      weak_ptr_factory_(this) {}

FlocBlocklistService::~FlocBlocklistService() = default;

void FlocBlocklistService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FlocBlocklistService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FlocBlocklistService::IsBlocklistFileReady() const {
  return first_file_ready_seen_;
}

void FlocBlocklistService::OnBlocklistFileReady(const base::FilePath& file_path,
                                                const base::Version& version) {
  blocklist_file_path_ = file_path;
  blocklist_version_ = version;
  first_file_ready_seen_ = true;

  for (auto& observer : observers_)
    observer.OnBlocklistFileReady();
}

void FlocBlocklistService::FilterByBlocklist(
    const FlocId& unfiltered_floc,
    const base::Optional<base::Version>& version_to_validate,
    FilterByBlocklistCallback callback) {
  DCHECK(unfiltered_floc.IsValid());
  DCHECK(first_file_ready_seen_);
  if (version_to_validate &&
      version_to_validate.value().CompareTo(blocklist_version_) != 0) {
    std::move(callback).Run(FlocId());
    return;
  }

  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&FilterByBlocklistOnBackgroundThread, unfiltered_floc,
                     blocklist_file_path_),
      std::move(callback));
}

void FlocBlocklistService::SetBackgroundTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
  background_task_runner_ = background_task_runner;
}

}  // namespace federated_learning
