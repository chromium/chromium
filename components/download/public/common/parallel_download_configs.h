// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_PARALLEL_DOWNLOAD_CONFIGS_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_PARALLEL_DOWNLOAD_CONFIGS_H_

namespace download {

// Finch parameter key value to enable parallel download. Used in enabled
// experiment group that needs other parameters, such as min_slice_size, but
// don't want to actually do parallel download.
constexpr char kEnableParallelDownloadFinchKey[] = "enable_parallel_download";

// Finch parameter key value for minimum slice size in bytes to use parallel
// download.
constexpr char kMinSliceSizeFinchKey[] = "min_slice_size";

// Finch parameter key value for number of parallel requests in a parallel
// download, including the original request.
constexpr char kParallelRequestCountFinchKey[] = "request_count";

// Finch parameter key value for the delay time in milliseconds to send
// parallel requests after response of the original request is handled.
constexpr char kParallelRequestDelayFinchKey[] = "parallel_request_delay";

// Finch parameter key value for the remaining time in seconds that is required
// to send parallel requests.
constexpr char kParallelRequestRemainingTimeFinchKey[] =
    "parallel_request_remaining_time";

}  //  namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_PARALLEL_DOWNLOAD_CONFIGS_H_
