// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/background_service/test/empty_client.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "services/network/public/cpp/resource_request_body.h"

namespace download {
namespace test {

void EmptyClient::OnServiceInitialized(
    bool state_lost,
    const std::vector<DownloadMetaData>& downloads) {}

void EmptyClient::OnServiceUnavailable() {}

void EmptyClient::OnDownloadStarted(
    const std::string& guid,
    const std::vector<GURL>& url_chain,
    const scoped_refptr<const net::HttpResponseHeaders>& headers) {}

void EmptyClient::OnDownloadUpdated(const std::string& guid,
                                    uint64_t bytes_uploaded,
                                    uint64_t bytes_downloaded) {}

void EmptyClient::OnDownloadFailed(const std::string& guid,
                                   const CompletionInfo& completion_info,
                                   FailureReason reason) {}

void EmptyClient::OnDownloadSucceeded(const std::string& guid,
                                      const CompletionInfo& completion_info) {}
bool EmptyClient::CanServiceRemoveDownloadedFile(const std::string& guid,
                                                 bool force_delete) {
  return true;
}

void EmptyClient::GetUploadData(const std::string& guid,
                                GetUploadDataCallback callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), nullptr));
}

}  // namespace test
}  // namespace download
