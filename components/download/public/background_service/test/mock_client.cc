// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/background_service/test/mock_client.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "services/network/public/cpp/resource_request_body.h"

namespace download {
namespace test {

MockClient::MockClient() = default;
MockClient::~MockClient() = default;

void MockClient::GetUploadData(const std::string& guid,
                               GetUploadDataCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), nullptr));
}

}  // namespace test
}  // namespace download
