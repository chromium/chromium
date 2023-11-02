// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_CONSTANTS_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_CONSTANTS_H_

namespace download {

// The type of completion when the download entry transits to complete state.
// TODO(xingliu): Implement timeout and unknown failure types.
enum class CompletionType {
  // The download is successfully finished.
  SUCCEED = 0,
  // The download is interrupted and failed.
  FAIL = 1,
  // The download is aborted by the client.
  ABORT = 2,
  // The download is timed out and the connection is closed.
  TIMEOUT = 3,
  // The download is failed for unknown reasons.
  UNKNOWN = 4,
  // The download is cancelled by the client.
  CANCEL = 5,
  // The download expended it's number of expensive retries.
  OUT_OF_RETRIES = 6,
  // The download expended it's number of 'free' retries.
  OUT_OF_RESUMPTIONS = 7,
  // The upload was timed out due to unresponsive client.
  UPLOAD_TIMEOUT = 8,
  // The count of entries for the enum.
  COUNT = 9,
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_CONSTANTS_H_
