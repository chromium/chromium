// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/background_service/client.h"

namespace download {

void Client::OnDownloadStarted(
    const std::string& guid,
    const std::vector<GURL>& url_chain,
    const scoped_refptr<const net::HttpResponseHeaders>& headers) {}

void Client::OnDownloadUpdated(const std::string& guid,
                               uint64_t bytes_uploaded,
                               uint64_t bytes_downloaded) {}

void Client::OnDownloadFailed(const std::string& guid,
                              const download::CompletionInfo& completion_info,
                              download::Client::FailureReason reason) {}

}  // namespace download
