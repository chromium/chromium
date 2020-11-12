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

base::Optional<uint64_t> ApplySortingLshOnBackgroundThread(
    uint64_t sim_hash,
    const base::FilePath& file_path) {
  base::File sorting_lsh_clusters_file(
      file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!sorting_lsh_clusters_file.IsValid())
    return base::nullopt;

  CopyingFileInputStream copying_stream(std::move(sorting_lsh_clusters_file));
  google::protobuf::io::CopyingInputStreamAdaptor zero_copy_stream_adaptor(
      &copying_stream);

  google::protobuf::io::CodedInputStream input_stream(
      &zero_copy_stream_adaptor);

  // The file should contain a list of integers. The 7th-order bit represents
  // whether the cohort should be blocked. The number represented by the 1st-6th
  // bits should be within the range [0, MaxNumberOfBitsInFloc]. Suppose the
  // list is l, then S(i) = 2^(l[i] & 0b111111) represents the the number of
  // hashes that can be associated with this floc id. The cumulative sum of S(i)
  // represents the boundary sim_hash values. We will use the higher index to
  // encode |sim_hash|, i.e. if sim_hash is within range
  // [CumSum(S(i-1)), CumSum(S(i))), |i| will be output floc.
  //
  // 0 is always an implicit CumSum boundary, i.e. if
  // 0 <= |sim_hash| < 2^(l[0]), then the index 0 will be the output floc.
  //
  // However, if the is_blocked bit (i.e. l[i] & 0b1000000) indicates that the
  // cohort should be blocked, we will output an invalid floc id.

  // Input sanitization: As we compute on the fly, we will check to make sure
  // each encountered entry, after dropping the is_blocked bit, is within
  // [0, MaxNumberOfBitsInFloc]. Besides, the cumulative sum should be no
  // greater than 2^MaxNumberOfBitsInFloc at any given time. If we cannot find
  // an index i, it means the the final cumulative sum is less than
  // 2^MaxNumberOfBitsInFloc, while we expect it to be exactly
  // 2^MaxNumberOfBitsInFloc, and we should also fail in this case. When some
  // check fails, we will also output an invalid floc id.
  //
  // A stricter sanitization would be to always stream all numbers and check
  // properties. We skip doing this to save some computation cost.
  const uint64_t kExpectedFinalCumulativeSum = (1ULL << kMaxNumberOfBitsInFloc);
  DCHECK_LT(sim_hash, kExpectedFinalCumulativeSum);

  uint64_t cumulative_sum = 0;
  uint32_t next_combined;

  // TODO(yaoxia): Add metrics for when the file has unexpected format.

  for (uint64_t index = 0; input_stream.ReadVarint32(&next_combined); ++index) {
    // Sanitizing error: the entry used more than |kSortingLshMaxBits| bits.
    if ((next_combined >> kSortingLshMaxBits) > 0)
      return base::nullopt;

    bool is_blocked = next_combined & kSortingLshBlockedMask;
    uint32_t next = next_combined & kSortingLshSizeMask;

    // Sanitizing error
    if (next > kMaxNumberOfBitsInFloc)
      return base::nullopt;

    cumulative_sum += (1ULL << next);

    // Sanitizing error
    if (cumulative_sum > kExpectedFinalCumulativeSum)
      return base::nullopt;

    // Found the sim-hash upper bound. Use the index as the new floc.
    if (cumulative_sum > sim_hash) {
      if (is_blocked)
        return base::nullopt;

      return index;
    }
  }

  // Sanitizing error: we didn't find a sim-hash upper bound, but we expect to
  // always find it after finish iterating through the list.
  return base::nullopt;
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

bool FlocSortingLshClustersService::IsSortingLshClustersFileReady() const {
  return first_file_ready_seen_;
}

void FlocSortingLshClustersService::OnSortingLshClustersFileReady(
    const base::FilePath& file_path,
    const base::Version& version) {
  sorting_lsh_clusters_file_path_ = file_path;
  sorting_lsh_clusters_version_ = version;
  first_file_ready_seen_ = true;

  for (auto& observer : observers_)
    observer.OnSortingLshClustersFileReady();
}

void FlocSortingLshClustersService::ApplySortingLsh(
    uint64_t sim_hash,
    ApplySortingLshCallback callback) {
  DCHECK(first_file_ready_seen_);

  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ApplySortingLshOnBackgroundThread, sim_hash,
                     sorting_lsh_clusters_file_path_),
      base::BindOnce(&FlocSortingLshClustersService::DidApplySortingLsh,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     sorting_lsh_clusters_version_));
}

void FlocSortingLshClustersService::SetBackgroundTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
  background_task_runner_ = background_task_runner;
}

void FlocSortingLshClustersService::DidApplySortingLsh(
    ApplySortingLshCallback callback,
    base::Version version,
    base::Optional<uint64_t> final_hash) {
  std::move(callback).Run(std::move(final_hash), std::move(version));
}

}  // namespace federated_learning
