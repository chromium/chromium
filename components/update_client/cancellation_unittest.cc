// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/cancellation.h"

#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {

TEST(CancellationTest, Cancels) {
  auto c = base::MakeRefCounted<Cancellation>();
  bool run = false;
  c->OnCancel(base::BindLambdaForTesting([&] { run = true; }));
  c->Cancel();
  ASSERT_TRUE(run);
}

TEST(CancellationTest, CancelsIfAlreadyCancelled) {
  auto c = base::MakeRefCounted<Cancellation>();
  bool run = false;
  c->Cancel();
  c->OnCancel(base::BindLambdaForTesting([&] { run = true; }));
  ASSERT_TRUE(run);
}

TEST(CancellationTest, Clears) {
  auto c = base::MakeRefCounted<Cancellation>();
  bool run = false;
  c->OnCancel(base::BindLambdaForTesting([&] { run = true; }));
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
  c->OnCancel(base::BindLambdaForTesting([&] { run1 = true; }));
  c->Clear();
  c->OnCancel(base::BindLambdaForTesting([&] { run2 = true; }));
  c->Cancel();
  EXPECT_FALSE(run1);
  EXPECT_TRUE(run2);
}

}  // namespace update_client
