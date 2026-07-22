// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/public/notebooks_network_service.h"

namespace notebooks {

NotebooksNetworkService::LoadResult::LoadResult(
    std::string result_bytes,
    NotebooksNetworkService::NetworkLoaderStatus status,
    int network_error_code)
    : result_bytes(std::move(result_bytes)),
      status(status),
      network_error_code(network_error_code) {}

}  // namespace notebooks
