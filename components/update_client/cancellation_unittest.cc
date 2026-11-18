// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/cancellation.h"

#include <optional>
#include <vector>

#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {

TEST(CancellationTest, Cancels) {
  auto c = base::MakeRefCounted<Cancellation>();
  std::optional<CancelMode> mode;
  c->OnCancel(base::BindLambdaForTesting([&](CancelMode m) { mode = m; }));
  c->Cancel();
  EXPECT_EQ(mode, CancelMode::kDeletePartialDownload);
}

TEST(CancellationTest, CancelsIfAlreadyCancelled) {
  auto c = base::MakeRefCounted<Cancellation>();
  std::optional<CancelMode> mode;
  c->Cancel();
  c->OnCancel(base::BindLambdaForTesting([&](CancelMode m) { mode = m; }));
  EXPECT_EQ(mode, CancelMode::kDeletePartialDownload);
}

TEST(CancellationTest, Clears) {
  auto c = base::MakeRefCounted<Cancellation>();
  bool run = false;
  c->OnCancel(base::BindLambdaForTesting([&](CancelMode) { run = true; }));
  c->Clear();
  c->Cancel();
  ASSERT_FALSE(run);
}

TEST(CancellationTest, IsCancelled) {
  auto c = base::MakeRefCounted<Cancellation>();
  ASSERT_FALSE(c->IsCancelled());
  c->Cancel();
  ASSERT_TRUE(c->IsCancelled());
}

TEST(CancellationTest, Replaces) {
  auto c = base::MakeRefCounted<Cancellation>();
  bool run1 = false;
  bool run2 = false;
  c->OnCancel(base::BindLambdaForTesting([&](CancelMode) { run1 = true; }));
  c->Clear();
  c->OnCancel(base::BindLambdaForTesting([&](CancelMode) { run2 = true; }));
  c->Cancel();
  EXPECT_FALSE(run1);
  EXPECT_TRUE(run2);
}

TEST(CancellationTest, CancelWithModePassesFirstMode) {
  auto c = base::MakeRefCounted<Cancellation>();
  std::vector<CancelMode> modes;
  c->OnCancel(
      base::BindLambdaForTesting([&](CancelMode m) { modes.push_back(m); }));
  c->CancelWithMode(CancelMode::kKeepPartialDownload);
  c->Cancel();
  c->CancelWithMode(CancelMode::kDeletePartialDownload);
  EXPECT_THAT(modes, ::testing::ElementsAre(CancelMode::kKeepPartialDownload));

  c->OnCancel(
      base::BindLambdaForTesting([&](CancelMode m) { modes.push_back(m); }));
  EXPECT_THAT(modes, ::testing::ElementsAre(CancelMode::kKeepPartialDownload,
                                            CancelMode::kKeepPartialDownload));
}

TEST(CancellationTest, CancelTwiceRunsCallbackOnce) {
  auto c = base::MakeRefCounted<Cancellation>();
  int calls = 0;
  c->OnCancel(base::BindLambdaForTesting([&](CancelMode) { ++calls; }));
  c->Cancel();
  c->Cancel();
  EXPECT_EQ(calls, 1);
}

TEST(CancellationTest, NullCallbackIsNoOp) {
  auto c = base::MakeRefCounted<Cancellation>();
  c->OnCancel(CancelCallback());
  c->Cancel();
  EXPECT_TRUE(c->IsCancelled());
  c->OnCancel(CancelCallback());
}

}  // namespace update_client
