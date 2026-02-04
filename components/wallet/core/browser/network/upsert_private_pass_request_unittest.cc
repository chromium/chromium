// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/network/upsert_private_pass_request.h"

#include "base/json/json_reader.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "components/version_info/version_info.h"
#include "components/wallet/core/browser/data_models/wallet_pass.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace wallet {

namespace {

class UpsertPrivatePassRequestTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

using UpsertPassCallback = base::test::TestFuture<
    const base::expected<WalletPass, WalletHttpClient::WalletRequestError>&>;

// Tests that GetRequestUrlPath returns the correct URL path.
TEST_F(UpsertPrivatePassRequestTest, GetRequestUrlPath) {
  UpsertPassCallback callback;
  UpsertPrivatePassRequest request(WalletPass(), callback.GetCallback());

  EXPECT_EQ(request.GetRequestUrlPath(), "v1/e/privatePasses:upsert");
}

}  // namespace
}  // namespace wallet
