// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport/resume_body_connection_delegate.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/test_future.h"
#include "components/browser_actuator/internal/transport/stream_connection_delegate.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

std::unique_ptr<ResumeBodyConnectionDelegate> MakeDelegate(std::string body) {
  return std::make_unique<ResumeBodyConnectionDelegate>(
      base::BindRepeating([](std::string b) { return b; }, std::move(body)),
      std::make_unique<DefaultStreamConnectionDelegate>());
}

TEST(ResumeBodyConnectionDelegateTest, ProvidesProviderOutputAsProtoBody) {
  auto delegate = MakeDelegate("serialized-watch-request");

  std::optional<StreamUploadBody> body = delegate->GetConnectionRequestBody();
  ASSERT_TRUE(body);
  EXPECT_EQ(body->content, "serialized-watch-request");
  EXPECT_EQ(body->content_type, "application/x-protobuf");
}

TEST(ResumeBodyConnectionDelegateTest, PrepareRequestForwardsThroughInner) {
  auto delegate = MakeDelegate("body");

  // The delegate adds nothing to the request itself; the pass-through inner
  // delegate completes it synchronously.
  base::test::TestFuture<std::unique_ptr<network::ResourceRequest>> future;
  delegate->PrepareRequest(std::make_unique<network::ResourceRequest>(),
                           future.GetCallback());
  EXPECT_TRUE(future.Take());
}

}  // namespace
}  // namespace browser_actuator
