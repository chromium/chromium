// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_SERVICE_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_SERVICE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_ack.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_cancel_requests.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_request.h"
#include "components/keyed_service/core/keyed_service.h"

namespace enterprise_connectors {

// This class encapsulates the process of getting data scanned through a generic
// interface.
class BinaryUploadService : public KeyedService {
 public:
  // The maximum size of data that can be uploaded via this service.
  constexpr static size_t kMaxUploadSizeBytes = 50 * 1024 * 1024;  // 50 MB

  // Upload the given file contents for deep scanning if the browser is
  // authorized to upload data, otherwise queue the request.
  virtual void MaybeUploadForDeepScanning(
      std::unique_ptr<BinaryUploadRequest> request) = 0;

  // Send an acknowledgement for the request with the given token.
  virtual void MaybeAcknowledge(std::unique_ptr<BinaryUploadAck> ack) = 0;

  // Cancel any requests that match the given criteria.  This is a best effort
  // approach only, since it is possible that requests have been started in a
  // way that they are no longer cancelable.
  virtual void MaybeCancelRequests(
      std::unique_ptr<BinaryUploadCancelRequests> cancel) = 0;

  // Get a WeakPtr to the instance.
  virtual base::WeakPtr<BinaryUploadService> AsWeakPtr() = 0;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_SERVICE_H_
