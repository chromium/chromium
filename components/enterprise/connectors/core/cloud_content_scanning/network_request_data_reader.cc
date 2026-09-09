// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/network_request_data_reader.h"

#include <utility>

#include "base/task/sequenced_task_runner.h"
#include "net/base/io_buffer.h"
#include "net/base/upload_data_stream.h"
#include "services/network/public/cpp/resource_request_body.h"

namespace enterprise_connectors {

NetworkRequestDataReader::NetworkRequestDataReader() = default;

NetworkRequestDataReader::NetworkRequestDataReader(
    scoped_refptr<network::ResourceRequestBody> request_body)
    : request_body_(std::move(request_body)) {
  // TODO(crbug.com/473031850): Add logic to instantiate the members of this
  // class.
}

NetworkRequestDataReader::NetworkRequestDataReader(NetworkRequestDataReader&&) =
    default;

NetworkRequestDataReader& NetworkRequestDataReader::operator=(
    NetworkRequestDataReader&&) = default;

NetworkRequestDataReader::~NetworkRequestDataReader() = default;

}  // namespace enterprise_connectors
