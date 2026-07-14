// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport/stream_connection_delegate.h"

#include <memory>
#include <utility>

#include "base/test/bind.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

TEST(StreamConnectionDelegateTest, DefaultDelegatePassesRequestThrough) {
  DefaultStreamConnectionDelegate delegate;
  auto request = std::make_unique<network::ResourceRequest>();
  network::ResourceRequest* request_ptr = request.get();

  std::unique_ptr<network::ResourceRequest> received;
  delegate.PrepareRequest(
      std::move(request),
      base::BindLambdaForTesting(
          [&](std::unique_ptr<network::ResourceRequest> prepared) {
            received = std::move(prepared);
          }));
  EXPECT_EQ(request_ptr, received.get());
}

TEST(StreamConnectionDelegateTest, NoRetryOnHttpFailureByDefault) {
  DefaultStreamConnectionDelegate delegate;
  EXPECT_FALSE(delegate.ShouldRetryOnHttpFailure(401));
  EXPECT_FALSE(delegate.ShouldRetryOnHttpFailure(503));
}

}  // namespace
}  // namespace browser_actuator
