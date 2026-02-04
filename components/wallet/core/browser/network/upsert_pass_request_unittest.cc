// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/network/upsert_pass_request.h"

#include "base/json/json_reader.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/version_info/version_info.h"
#include "components/wallet/core/browser/data_models/wallet_pass.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace wallet {

namespace {

class UpsertPassRequestTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

using UpsertPassCallback = base::test::TestFuture<
    const base::expected<WalletPass, WalletHttpClient::WalletRequestError>&>;

// Tests that GetRequestUrlPath returns the correct URL path.
TEST_F(UpsertPassRequestTest, GetRequestUrlPath) {
  UpsertPassCallback callback;
  UpsertPassRequest request(WalletPass(), callback.GetCallback());

  EXPECT_EQ(request.GetRequestUrlPath(), "v1/passes:upsert");
}

// Tests that GetRequestContent generates the correct JSON request body
// for a LoyaltyCard pass.
TEST_F(UpsertPassRequestTest, GetRequestContent_LoyaltyCard) {
  LoyaltyCard loyalty_card;
  loyalty_card.plan_name = "p1";
  loyalty_card.issuer_name = "i1";
  loyalty_card.member_id = "m1";

  WalletPass pass;
  pass.pass_data = loyalty_card;

  UpsertPassCallback callback;
  UpsertPassRequest request(pass, callback.GetCallback());

  std::string request_body = request.GetRequestContent();
  std::optional<base::Value> root =
      base::JSONReader::Read(request_body, base::JSON_PARSE_RFC);
  ASSERT_TRUE(root.has_value());
  ASSERT_TRUE(root->is_dict());

  const base::DictValue& dict = root->GetDict();

  // Verify pass
  const base::DictValue* pass_dict = dict.FindDict("pass");
  ASSERT_TRUE(pass_dict);

  // Verify external_id
  const base::DictValue* external_id = pass_dict->FindDict("external_id");
  ASSERT_TRUE(external_id);
  ASSERT_EQ(external_id->FindInt("namespace"), 1);
  const std::string* uuid_str = external_id->FindString("external_id");
  ASSERT_TRUE(uuid_str);
  EXPECT_TRUE(base::Uuid::ParseLowercase(*uuid_str).is_valid());

  // Verify loyalty_card
  const base::DictValue* loyalty_card_dict =
      pass_dict->FindDict("loyalty_card");
  ASSERT_TRUE(loyalty_card_dict);
  EXPECT_EQ(*loyalty_card_dict->FindString("merchant_name"), "i1");
  EXPECT_EQ(*loyalty_card_dict->FindString("loyalty_number"), "m1");
  EXPECT_EQ(*loyalty_card_dict->FindString("program_name"), "p1");

  // Verify client_info
  const base::DictValue* client_info = dict.FindDict("client_info");
  ASSERT_TRUE(client_info);
  const base::DictValue* chrome_client_info =
      client_info->FindDict("chrome_client_info");
  ASSERT_TRUE(chrome_client_info);
  EXPECT_EQ(*chrome_client_info->FindString("version"),
            version_info::GetVersionNumber());
}

// Tests that GetRequestContent correctly includes the pass_id when
// available.
TEST_F(UpsertPassRequestTest, GetRequestContent_WithPassId) {
  WalletPass pass;
  pass.id = "pass-id";

  UpsertPassCallback callback;
  UpsertPassRequest request(pass, callback.GetCallback());

  std::string request_body = request.GetRequestContent();
  std::optional<base::Value> root =
      base::JSONReader::Read(request_body, base::JSON_PARSE_RFC);
  ASSERT_TRUE(root.has_value());
  ASSERT_TRUE(root->is_dict());

  const base::DictValue* pass_dict = root->GetDict().FindDict("pass");
  ASSERT_TRUE(pass_dict);
  EXPECT_EQ(*pass_dict->FindString("pass_id"), "pass-id");
}

// Tests that OnResponse handles a successful HTTP response.
TEST_F(UpsertPassRequestTest, OnResponse_Success) {
  UpsertPassCallback callback;
  UpsertPassRequest request(WalletPass(), callback.GetCallback());

  std::move(request).OnResponse("{}");

  ASSERT_TRUE(callback.Wait());
  EXPECT_TRUE(callback.Get().has_value());
}

// Tests that OnResponse handles an error HTTP response.
TEST_F(UpsertPassRequestTest, OnResponse_Error) {
  UpsertPassCallback callback;
  UpsertPassRequest request(WalletPass(), callback.GetCallback());

  std::move(request).OnResponse(base::unexpected(
      WalletHttpClient::WalletRequestError::kAccessTokenFetchFailed));

  ASSERT_TRUE(callback.Wait());
  ASSERT_FALSE(callback.Get().has_value());
  EXPECT_EQ(callback.Get().error(),
            WalletHttpClient::WalletRequestError::kAccessTokenFetchFailed);
}

}  // namespace
}  // namespace wallet
