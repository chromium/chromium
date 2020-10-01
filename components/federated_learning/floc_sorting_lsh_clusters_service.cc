// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/federated_learning/floc_sorting_lsh_clusters_service.h"

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

FlocId ApplySortingLshOnBackgroundThread(const FlocId& raw_floc_id,
                                         const base::FilePath& file_path) {
  DCHECK(raw_floc_id.IsValid());
  base::File sorting_lsh_clusters_file(
      file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!sorting_lsh_clusters_file.IsValid())
    return FlocId();

  CopyingFileInputStream copying_stream(std::move(sorting_lsh_clusters_file));
  google::protobuf::io::CopyingInputStreamAdaptor zero_copy_stream_adaptor(
      &copying_stream);

  google::protobuf::io::CodedInputStream input_stream(
      &zero_copy_stream_adaptor);

  // The file should contain a list of integers within the range [0,
  // MaxNumberOfBitsInFloc]. Suppose the list is l, then 2^(l[i]) represents the
  // the number of hashes that can be associated with this floc id. The
  // cumulative sum of 2^(l[i]) represents the boundary floc values in
  // |raw_floc_id|'s space. We will use the higher index to encode
  // |raw_floc_id|, i.e. if raw_floc_id is within range
  // [CumSum(2^(l[i-1])), CumSum(2^(l[i]))), |i| will be output floc.
  //
  // 0 is always an implicit CumSum boundary, i.e. if
  // 0 <= |raw_floc_id| < 2^(l[0]), then the index 0 will be the output floc.
  //
  // Input sanitization: As we compute on the fly, we will check to make sure
  // each encountered entry is within [0, MaxNumberOfBitsInFloc]. Besides, the
  // cumulative sum should be no greater than 2^MaxNumberOfBitsInFloc at any
  // given time. If we cannot find an index i, it means the the final cumulative
  // sum is less than 2^MaxNumberOfBitsInFloc, while we expect it to be exactly
  // 2^MaxNumberOfBitsInFloc, and we should also fail in this case. When some
  // check fails, we will output an invalid floc id.
  //
  // A stricter sanitization would be to always stream all numbers and check
  // properties. We skip doing this to save some computation cost.
  uint64_t raw_floc_id_as_int = raw_floc_id.ToUint64();
  const uint64_t kExpectedFinalCumulativeSum = (1ULL << kMaxNumberOfBitsInFloc);
  DCHECK(raw_floc_id_as_int < kExpectedFinalCumulativeSum);

  uint64_t cumulative_sum = 0;
  uint32_t next;

  // TODO: Add metrics for when we return an invalid floc, which indicates a
  // wrong/corrupted file.

  for (uint64_t index = 0; input_stream.ReadVarint32(&next); ++index) {
    if (next > kMaxNumberOfBitsInFloc)
      return FlocId();

    cumulative_sum += (1ULL << next);

    if (cumulative_sum > kExpectedFinalCumulativeSum)
      return FlocId();

    if (cumulative_sum > raw_floc_id_as_int)
      return FlocId(index);
  }

  return FlocId();
}

}  // namespace

FlocSortingLshClustersService::FlocSortingLshClustersService()
    : background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      weak_ptr_factory_(this) {}

FlocSortingLshClustersService::~FlocSortingLshClustersService() = default;

void FlocSortingLshClustersService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FlocSortingLshClustersService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FlocSortingLshClustersService::SetBackgroundTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
  background_task_runner_ = background_task_runner;
}

void FlocSortingLshClustersService::ApplySortingLsh(
    const FlocId& raw_floc_id,
    ApplySortingLshCallback callback) {
  DCHECK(raw_floc_id.IsValid());
  DCHECK(sorting_lsh_clusters_file_path_.has_value());
  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ApplySortingLshOnBackgroundThread, raw_floc_id,
                     sorting_lsh_clusters_file_path_.value()),
      std::move(callback));
}

void FlocSortingLshClustersService::OnSortingLshClustersFileReady(
    const base::FilePath& file_path) {
  sorting_lsh_clusters_file_path_ = file_path;

  for (auto& observer : observers_)
    observer.OnSortingLshClustersFileReady();
}

}  // namespace federated_learning
