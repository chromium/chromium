// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_INSIGHTS_FCP_FILES_H_
#define COMPONENTS_PRIVATE_INSIGHTS_FCP_FILES_H_

#include <string>

#include "third_party/federated_compute/src/fcp/client/files.h"

namespace private_insights {

class FcpFiles : public fcp::client::Files {
 public:
  FcpFiles();
  ~FcpFiles() override;

  FcpFiles(const FcpFiles&) = delete;
  FcpFiles& operator=(const FcpFiles&) = delete;

  absl::StatusOr<std::string> CreateTempFile(  // nocheck
      const std::string& prefix,
      const std::string& suffix) override;
};

}  // namespace private_insights

#endif  // COMPONENTS_PRIVATE_INSIGHTS_FCP_FILES_H_
