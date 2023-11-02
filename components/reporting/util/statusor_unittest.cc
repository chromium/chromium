// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/statusor.h"

#include "components/reporting/util/status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

TEST(StatusOr, MoveConstructFromAndExtractToStatusImplicitly) {
  Status status(error::INTERNAL, "internal error");
  base::unexpected<Status> unexpected_status(status);
  StatusOr<int> status_or(std::move(unexpected_status));
  Status extracted_status{std::move(status_or).error()};
  EXPECT_EQ(status, extracted_status);
}

TEST(StatusOr, CopyConstructFromAndExtractToStatusImplicitly) {
  Status status(error::INTERNAL, "internal error");
  base::unexpected<Status> unexpected_status(status);
  StatusOr<int> status_or(unexpected_status);
  Status extracted_status{status_or.error()};
  EXPECT_EQ(status, extracted_status);
}

}  // namespace
}  // namespace reporting
