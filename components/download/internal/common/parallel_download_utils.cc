// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/common/parallel_download_utils.h"

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_save_info.h"
#include "components/download/public/common/parallel_download_configs.h"

namespace download {

namespace {

// Default value for |kMinSliceSizeFinchKey|, when no parameter is specified.
const int64_t kMinSliceSizeParallelDownload = 1365333;

// Default value for |kParallelRequestCountFinchKey|, when no parameter is
// specified.
const int kParallelRequestCount = 3;

// The default remaining download time in seconds required for parallel request
// creation.
const int kDefaultRemainingTimeInSeconds = 2;

// TODO(qinmin): replace this with a comparator operator in
// DownloadItem::ReceivedSlice.
bool compareReceivedSlices(const DownloadItem::ReceivedSlice& lhs,
                           const DownloadItem::ReceivedSlice& rhs) {
  return lhs.offset < rhs.offset;
}

}  // namespace

std::vector<DownloadItem::ReceivedSlice> FindSlicesToDownload(
    const std::vector<DownloadItem::ReceivedSlice>& received_slices) {
  std::vector<DownloadItem::ReceivedSlice> result;
  if (received_slices.empty()) {
    result.emplace_back(0, DownloadSaveInfo::kLengthFullContent);
    return result;
  }

  auto iter = received_slices.begin();
  DCHECK_GE(iter->offset, 0);
  if (iter->offset != 0)
    result.emplace_back(0, iter->offset);

  while (true) {
    int64_t offset = iter->offset + iter->received_bytes;
    auto next = std::next(iter);
    if (next == received_slices.end()) {
      result.emplace_back(offset, DownloadSaveInfo::kLengthFullContent);
      break;
    }

    DCHECK_GE(next->offset, offset);
    if (next->offset > offset)
      result.emplace_back(offset, next->offset - offset);
    iter = next;
  }
  return result;
}

size_t AddOrMergeReceivedSliceIntoSortedArray(
    const DownloadItem::ReceivedSlice& new_slice,
    std::vector<DownloadItem::ReceivedSlice>& received_slices) {
  auto it = std::upper_bound(received_slices.begin(), received_slices.end(),
                             new_slice, compareReceivedSlices);
  if (it != received_slices.begin()) {
    auto prev = std::prev(it);
    if (prev->offset + prev->received_bytes == new_slice.offset) {
      prev->received_bytes += new_slice.received_bytes;
      return static_cast<size_t>(std::distance(received_slices.begin(), prev));
    }
  }

  it = received_slices.emplace(it, new_slice);
  return static_cast<size_t>(std::distance(received_slices.begin(), it));
}

bool CanRecoverFromError(
    const DownloadFileImpl::SourceStream* error_stream,
    const DownloadFileImpl::SourceStream* preceding_neighbor) {
  DCHECK(error_stream->offset() >= preceding_neighbor->offset())
      << "Preceding"
         "stream's offset should be smaller than the error stream.";
  DCHECK_GE(error_stream->length(), 0);

  if (preceding_neighbor->is_finished()) {
    // Check if the preceding stream fetched to the end of the file without
    // error. The error stream doesn't need to download anything.
    if (preceding_neighbor->length() == DownloadSaveInfo::kLengthFullContent &&
        preceding_neighbor->GetCompletionStatus() ==
            DOWNLOAD_INTERRUPT_REASON_NONE) {
      return true;
    }

    // Check if finished preceding stream has already downloaded all data for
    // the error stream.
    if (error_stream->length() > 0) {
      return error_stream->offset() + error_stream->length() <=
             preceding_neighbor->offset() + preceding_neighbor->bytes_read();
    }

    return false;
  }

  // If preceding stream is half open, and still working, we can recover.
  if (preceding_neighbor->length() == DownloadSaveInfo::kLengthFullContent) {
    return true;
  }

  // Check if unfinished preceding stream is able to download data for error
  // stream in the future only when preceding neighbor and error stream both
  // have an upper bound.
  if (error_stream->length() > 0 && preceding_neighbor->length() > 0) {
    return error_stream->offset() + error_stream->length() <=
           preceding_neighbor->offset() + preceding_neighbor->length();
  }

  return false;
}

void DebugSlicesInfo(const DownloadItem::ReceivedSlices& slices) {
  DVLOG(1) << "Received slices size : " << slices.size();
  for (const auto& it : slices) {
    DVLOG(1) << "Slice offset = " << it.offset
             << " , received_bytes = " << it.received_bytes
             << " , finished = " << it.finished;
  }
}

std::vector<DownloadItem::ReceivedSlice> FindSlicesForRemainingContent(
    int64_t current_offset,
    int64_t total_length,
    int request_count,
    int64_t min_slice_size) {
  std::vector<DownloadItem::ReceivedSlice> new_slices;

  if (request_count > 0) {
    int64_t slice_size =
        std::max<int64_t>(total_length / request_count, min_slice_size);
    slice_size = slice_size > 0 ? slice_size : 1;
    for (int i = 0, num_requests = total_length / slice_size;
         i < num_requests - 1; ++i) {
      new_slices.emplace_back(current_offset, slice_size);
      current_offset += slice_size;
    }
  }

  // No strong assumption that content length header is correct. So the last
  // slice is always half open, which sends range request like "Range:50-".
  new_slices.emplace_back(current_offset, DownloadSaveInfo::kLengthFullContent);
  return new_slices;
}

int64_t GetMinSliceSizeConfig() {
  std::string finch_value = base::GetFieldTrialParamValueByFeature(
      features::kParallelDownloading, kMinSliceSizeFinchKey);
  int64_t result;
  return base::StringToInt64(finch_value, &result)
             ? result
             : kMinSliceSizeParallelDownload;
}

int GetParallelRequestCountConfig() {
  std::string finch_value = base::GetFieldTrialParamValueByFeature(
      features::kParallelDownloading, kParallelRequestCountFinchKey);
  int result;
  return base::StringToInt(finch_value, &result) ? result
                                                 : kParallelRequestCount;
}

base::TimeDelta GetParallelRequestDelayConfig() {
  std::string finch_value = base::GetFieldTrialParamValueByFeature(
      features::kParallelDownloading, kParallelRequestDelayFinchKey);
  int64_t time_ms = 0;
  return base::StringToInt64(finch_value, &time_ms)
             ? base::Milliseconds(time_ms)
             : base::Milliseconds(0);
}

base::TimeDelta GetParallelRequestRemainingTimeConfig() {
  std::string finch_value = base::GetFieldTrialParamValueByFeature(
      features::kParallelDownloading, kParallelRequestRemainingTimeFinchKey);
  int time_in_seconds = 0;
  return base::StringToInt(finch_value, &time_in_seconds)
             ? base::Seconds(time_in_seconds)
             : base::Seconds(kDefaultRemainingTimeInSeconds);
}

int64_t GetMaxContiguousDataBlockSizeFromBeginning(
    const DownloadItem::ReceivedSlices& slices) {
  auto iter = slices.begin();

  int64_t size = 0;
  while (iter != slices.end() && iter->offset == size) {
    size += iter->received_bytes;
    iter++;
  }
  return size;
}

bool IsParallelDownloadEnabled() {
  bool feature_enabled =
      base::FeatureList::IsEnabled(features::kParallelDownloading);
  // Disabled when |kEnableParallelDownloadFinchKey| Finch config is set to
  // false.
  bool enabled_parameter = GetFieldTrialParamByFeatureAsBool(
      features::kParallelDownloading, kEnableParallelDownloadFinchKey, true);
  return feature_enabled && enabled_parameter;
}

}  // namespace download
