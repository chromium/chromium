// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_NETWORK_REQUEST_DATA_READER_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_NETWORK_REQUEST_DATA_READER_H_

#include <memory>
#include <vector>

#include "base/files/file.h"
#include "base/memory/scoped_refptr.h"
#include "net/log/net_log_with_source.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace net {
class IOBufferWithSize;
class UploadDataStream;
}  // namespace net

namespace network {
class ResourceRequestBody;
}  // namespace network

namespace enterprise_connectors {

// A helper class that encapsulates network request data and provides stream
// reading functionality for network request scanning.
class NetworkRequestDataReader {
 public:
  NetworkRequestDataReader();
  explicit NetworkRequestDataReader(
      scoped_refptr<network::ResourceRequestBody> request_body);
  NetworkRequestDataReader(const NetworkRequestDataReader&) = delete;
  NetworkRequestDataReader& operator=(const NetworkRequestDataReader&) = delete;
  NetworkRequestDataReader(NetworkRequestDataReader&&);
  NetworkRequestDataReader& operator=(NetworkRequestDataReader&&);
  ~NetworkRequestDataReader();

 private:
  // Body of a network request to be scanned.
  scoped_refptr<network::ResourceRequestBody> request_body_;

  // Helper instantiated by `request_body_` to read its data.
  std::unique_ptr<net::UploadDataStream> request_body_getter_;

  // Helper members to instantiate `request_body_getter_` and read data from
  // `request_body_`.
  scoped_refptr<base::SequencedTaskRunner> network_request_task_runner_;
  std::vector<base::File> opened_files_;
  net::NetLogWithSource net_log_;
  scoped_refptr<net::IOBufferWithSize> buffer_;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_NETWORK_REQUEST_DATA_READER_H_
