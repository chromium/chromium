// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_COMMON_PARALLEL_DOWNLOAD_UTILS_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_COMMON_PARALLEL_DOWNLOAD_UTILS_H_

#include <vector>

#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_file_impl.h"
#include "components/download/public/common/download_item.h"

namespace download {

// Given an array of slices that are received, returns an array of slices to
// download. |received_slices| must be ordered by offsets.
COMPONENTS_DOWNLOAD_EXPORT std::vector<DownloadItem::ReceivedSlice>
FindSlicesToDownload(
    const std::vector<DownloadItem::ReceivedSlice>& received_slices);

// Adds or merges a new received slice into a vector of sorted slices. If the
// slice can be merged with the slice preceding it, merge the 2 slices.
// Otherwise, insert the slice and keep the vector sorted. Returns the index
// of the newly updated slice.
COMPONENTS_DOWNLOAD_EXPORT size_t AddOrMergeReceivedSliceIntoSortedArray(
    const DownloadItem::ReceivedSlice& new_slice,
    std::vector<DownloadItem::ReceivedSlice>& received_slices);

// Returns if a preceding stream can still download the part of content that
// was arranged to |error_stream|.
COMPONENTS_DOWNLOAD_EXPORT bool CanRecoverFromError(
    const DownloadFileImpl::SourceStream* error_stream,
    const DownloadFileImpl::SourceStream* preceding_neighbor);

// Chunks the content that starts from |current_offset|, into at most
// std::max(|request_count|, 1) smaller slices.
// Each slice contains at least |min_slice_size| bytes unless |total_length|
// is less than |min_slice_size|.
// The last slice is half opened.
COMPONENTS_DOWNLOAD_EXPORT std::vector<download::DownloadItem::ReceivedSlice>
FindSlicesForRemainingContent(int64_t current_offset,
                              int64_t total_length,
                              int request_count,
                              int64_t min_slice_size);

// Finch configuration utilities.
//
// Get the minimum slice size to use parallel download from finch configuration.
// A slice won't be further chunked into smaller slices if the size is less
// than the minimum size.
COMPONENTS_DOWNLOAD_EXPORT int64_t GetMinSliceSizeConfig();

// Get the request count for parallel download from finch configuration.
COMPONENTS_DOWNLOAD_EXPORT int GetParallelRequestCountConfig();

// Get the time delay to send parallel requests after the response of original
// request is handled.
COMPONENTS_DOWNLOAD_EXPORT base::TimeDelta GetParallelRequestDelayConfig();

// Get the required remaining time before creating parallel requests.
COMPONENTS_DOWNLOAD_EXPORT base::TimeDelta
GetParallelRequestRemainingTimeConfig();

// Given an ordered array of slices, get the maximum size of a contiguous data
// block that starts from offset 0. If the first slice doesn't start from offset
// 0, return 0.
COMPONENTS_DOWNLOAD_EXPORT int64_t GetMaxContiguousDataBlockSizeFromBeginning(
    const download::DownloadItem::ReceivedSlices& slices);

// Returns whether parallel download is enabled.
COMPONENTS_DOWNLOAD_EXPORT bool IsParallelDownloadEnabled();

// Print the states of received slices for debugging.
void DebugSlicesInfo(const DownloadItem::ReceivedSlices& slices);

}  //  namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_COMMON_PARALLEL_DOWNLOAD_UTILS_H_
