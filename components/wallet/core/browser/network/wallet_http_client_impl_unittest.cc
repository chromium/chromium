// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/network/wallet_http_client_impl.h"

#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/uuid.h"
#include "components/version_info/version_info.h"
#include "components/wallet/core/browser/data_models/walletable_pass.h"
#include "components/wallet/core/common/wallet_features.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace wallet {
namespace {

using SavePassCallback = base::test::TestFuture<
    base::expected<WalletHttpClient::SavePassResult,
                   WalletHttpClient::WalletRequestError>>;

class WalletHttpClientImplTest : public testing::Test {
 public:
  WalletHttpClientImplTest()
      : client_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  ~WalletHttpClientImplTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kWalletablePassDetection,
        {{"walletable_pass_save_url", "https://test-wallet.com/"}});
  }

  GURL GetUpsertPassUrl() {
    return GURL(kWalletablePassSaveUrl.Get()).Resolve("v1/passes:upsert");
  }

  WalletHttpClientImpl* client() { return &client_; }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  WalletHttpClientImpl client_;
};

// Tests that SavePass successfully triggers a network request and invokes the
// callback with a success result when the server responds with success.
TEST_F(WalletHttpClientImplTest, SavePass_Success) {
  WalletablePass pass;
  SavePassCallback save_pass_callback;
  client()->SavePass(pass, save_pass_callback.GetCallback());

  GURL expected_url = GetUpsertPassUrl();
  EXPECT_TRUE(test_url_loader_factory()->IsPending(expected_url.spec()));
  test_url_loader_factory()->AddResponse(expected_url.spec(), "{}");

  ASSERT_TRUE(save_pass_callback.Wait());
}

// Tests that SavePass correctly handles server errors by invoking the callback
// with a failure result.
TEST_F(WalletHttpClientImplTest, SavePass_Failure) {
  WalletablePass pass;
  SavePassCallback save_pass_callback;
  client()->SavePass(pass, save_pass_callback.GetCallback());

  GURL expected_url = GetUpsertPassUrl();
  EXPECT_TRUE(test_url_loader_factory()->IsPending(expected_url.spec()));
  test_url_loader_factory()->AddResponse(
      expected_url, network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_FAILED));

  ASSERT_TRUE(save_pass_callback.Wait());
  EXPECT_EQ(save_pass_callback.Get().error(),
            WalletHttpClient::WalletRequestError::kGenericError);
}

// Tests that multiple SavePass requests can be in-flight simultaneously and all
// callbacks are invoked correctly upon completion.
TEST_F(WalletHttpClientImplTest, SavePass_ConcurrentRequests) {
  WalletablePass pass1;
  LoyaltyCard loyalty_card1;
  loyalty_card1.plan_name = "p1";
  pass1.pass_data = loyalty_card1;

  WalletablePass pass2;
  LoyaltyCard loyalty_card2;
  loyalty_card2.plan_name = "p2";
  pass2.pass_data = loyalty_card2;

  SavePassCallback save_pass_callback1;
  SavePassCallback save_pass_callback2;

  client()->SavePass(pass1, save_pass_callback1.GetCallback());
  client()->SavePass(pass2, save_pass_callback2.GetCallback());

  ASSERT_EQ(test_url_loader_factory()->NumPending(), 2);

  // Complete both requests.
  GURL expected_url = GetUpsertPassUrl();
  test_url_loader_factory()->SimulateResponseForPendingRequest(
      expected_url.spec(), "{}");
  test_url_loader_factory()->SimulateResponseForPendingRequest(
      expected_url.spec(), "{}");

  ASSERT_TRUE(save_pass_callback1.Wait());
  ASSERT_TRUE(save_pass_callback2.Wait());
}

// Tests that SavePass for a LoyaltyCard builds the correct JSON request body
// structure.
TEST_F(WalletHttpClientImplTest, SavePass_LoyaltyCard_RequestStructure) {
  LoyaltyCard loyalty_card;
  loyalty_card.plan_name = "p1";
  loyalty_card.issuer_name = "i1";
  loyalty_card.member_id = "m1";

  WalletablePass pass;
  pass.pass_data = loyalty_card;

  SavePassCallback save_pass_callback;
  client()->SavePass(pass, save_pass_callback.GetCallback());

  GURL expected_url = GetUpsertPassUrl();
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory()->GetPendingRequest(0);
  ASSERT_TRUE(pending_request);
  EXPECT_EQ(pending_request->request.url, expected_url);

  std::string request_body = network::GetUploadData(pending_request->request);
  std::optional<base::Value> root =
      base::JSONReader::Read(request_body, base::JSON_PARSE_RFC);
  ASSERT_TRUE(root.has_value());
  ASSERT_TRUE(root->is_dict());

  const base::Value::Dict& dict = root->GetDict();

  // Verify pass
  const base::Value::Dict* pass_dict = dict.FindDict("pass");
  ASSERT_TRUE(pass_dict);

  // Verify external_id
  const base::Value::Dict* external_id = pass_dict->FindDict("external_id");
  ASSERT_TRUE(external_id);
  EXPECT_EQ(external_id->FindInt("namespace"), 1);
  const std::string* uuid_str = external_id->FindString("external_id");
  ASSERT_TRUE(uuid_str);
  EXPECT_TRUE(base::Uuid::ParseLowercase(*uuid_str).is_valid());

  // Verify loyalty_card
  const base::Value::Dict* loyalty_card_dict =
      pass_dict->FindDict("loyalty_card");
  ASSERT_TRUE(loyalty_card_dict);
  EXPECT_EQ(*loyalty_card_dict->FindString("merchant_name"), "i1");
  EXPECT_EQ(*loyalty_card_dict->FindString("loyalty_number"), "m1");
  EXPECT_EQ(*loyalty_card_dict->FindString("program_name"), "p1");

  // Verify client_info
  const base::Value::Dict* client_info = dict.FindDict("client_info");
  ASSERT_TRUE(client_info);
  const base::Value::Dict* chrome_client_info =
      client_info->FindDict("chrome_client_info");
  ASSERT_TRUE(chrome_client_info);
  EXPECT_EQ(*chrome_client_info->FindString("version"),
            version_info::GetVersionNumber());
}

}  // namespace
}  // namespace wallet
