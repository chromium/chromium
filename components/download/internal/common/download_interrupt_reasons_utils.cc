// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_interrupt_reasons_utils.h"

#include "base/notreached.h"

namespace download {

DownloadInterruptReason ConvertFileErrorToInterruptReason(
    base::File::Error file_error) {
  switch (file_error) {
    case base::File::FILE_OK:
      return DOWNLOAD_INTERRUPT_REASON_NONE;

    case base::File::FILE_ERROR_IN_USE:
      return DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR;

    case base::File::FILE_ERROR_ACCESS_DENIED:
      return DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED;

    case base::File::FILE_ERROR_TOO_MANY_OPENED:
      return DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR;

    case base::File::FILE_ERROR_NO_MEMORY:
      return DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR;

    case base::File::FILE_ERROR_NO_SPACE:
      return DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE;

    case base::File::FILE_ERROR_SECURITY:
      return DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED;

    default:
      return DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
  }
}

DownloadInterruptReason ConvertNetErrorToInterruptReason(
    net::Error net_error,
    DownloadInterruptSource source) {
  switch (net_error) {
    case net::OK:
      return DOWNLOAD_INTERRUPT_REASON_NONE;

    // File errors.

    // The file is too large.
    case net::ERR_FILE_TOO_BIG:
      return DOWNLOAD_INTERRUPT_REASON_FILE_TOO_LARGE;

    // Permission to access a resource, other than the network, was denied.
    case net::ERR_ACCESS_DENIED:
      return DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED;

    // There were not enough resources to complete the operation.
    case net::ERR_INSUFFICIENT_RESOURCES:
      return DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR;

    // Memory allocation failed.
    case net::ERR_OUT_OF_MEMORY:
      return DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR;

    // The path or file name is too long.
    case net::ERR_FILE_PATH_TOO_LONG:
      return DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG;

    // Not enough room left on the disk.
    case net::ERR_FILE_NO_SPACE:
      return DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE;

    // The file has a virus.
    case net::ERR_FILE_VIRUS_INFECTED:
      return DOWNLOAD_INTERRUPT_REASON_FILE_VIRUS_INFECTED;

    // The file was blocked by local policy.
    case net::ERR_BLOCKED_BY_CLIENT:
      return DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED;

    // Network errors.

    // The network operation timed out.
    case net::ERR_TIMED_OUT:
      return DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT;

    // The network connection was lost or changed.
    case net::ERR_NETWORK_CHANGED:
    case net::ERR_INTERNET_DISCONNECTED:
      return DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED;

    // The server has gone down.
    case net::ERR_CONNECTION_FAILED:
      return DOWNLOAD_INTERRUPT_REASON_NETWORK_SERVER_DOWN;

    // Server responses.

    // The server does not support range requests.
    case net::ERR_REQUEST_RANGE_NOT_SATISFIABLE:
      return DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE;

    case net::ERR_CONTENT_LENGTH_MISMATCH:
      return DOWNLOAD_INTERRUPT_REASON_SERVER_CONTENT_LENGTH_MISMATCH;

    default:
      break;
  }

  // Handle errors that don't have mappings, depending on the source.
  switch (source) {
    case DOWNLOAD_INTERRUPT_FROM_DISK:
      return DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
    case DOWNLOAD_INTERRUPT_FROM_NETWORK:
      return DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED;
    case DOWNLOAD_INTERRUPT_FROM_SERVER:
      return DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED;
    default:
      break;
  }

  NOTREACHED();

  return DOWNLOAD_INTERRUPT_REASON_NONE;
}

DownloadInterruptReason ConvertMojoNetworkRequestStatusToInterruptReason(
    mojom::NetworkRequestStatus status) {
  switch (status) {
    case mojom::NetworkRequestStatus::OK:
      return DOWNLOAD_INTERRUPT_REASON_NONE;
    case mojom::NetworkRequestStatus::NETWORK_TIMEOUT:
      return DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT;
    case mojom::NetworkRequestStatus::NETWORK_DISCONNECTED:
      return DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED;
    case mojom::NetworkRequestStatus::NETWORK_SERVER_DOWN:
      return DOWNLOAD_INTERRUPT_REASON_NETWORK_SERVER_DOWN;
    case mojom::NetworkRequestStatus::SERVER_NO_RANGE:
      return DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE;
    case mojom::NetworkRequestStatus::SERVER_CONTENT_LENGTH_MISMATCH:
      return DOWNLOAD_INTERRUPT_REASON_SERVER_CONTENT_LENGTH_MISMATCH;
    case mojom::NetworkRequestStatus::SERVER_UNREACHABLE:
      return DOWNLOAD_INTERRUPT_REASON_SERVER_UNREACHABLE;
    case mojom::NetworkRequestStatus::SERVER_CERT_PROBLEM:
      return DOWNLOAD_INTERRUPT_REASON_SERVER_CERT_PROBLEM;
    case mojom::NetworkRequestStatus::USER_CANCELED:
      return DOWNLOAD_INTERRUPT_REASON_USER_CANCELED;
    case mojom::NetworkRequestStatus::NETWORK_FAILED:
      return DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED;
    default:
      NOTREACHED();
      break;
  }
  return DOWNLOAD_INTERRUPT_REASON_NONE;
}

}  // namespace download
