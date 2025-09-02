// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/dappnet/dappnet_settings_handler.h"

#include "base/test/bind.h"
#include "chrome/browser/dappnet/mojom/dappnet_settings.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

class DappnetSettingsHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    
    // Register the preference
    profile_->GetTestingPrefService()->registry()->RegisterListPref(
        "dappnet.rpc_endpoints");
    
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(
        content::WebContents::Create(
            content::WebContents::CreateParams(profile_.get())));
    
    handler_ = std::make_unique<DappnetSettingsHandler>();
    handler_->set_web_ui(web_ui_.get());
    handler_->RegisterMessages();
  }
  
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<DappnetSettingsHandler> handler_;
};

TEST_F(DappnetSettingsHandlerTest, GetRpcEndpointsEmpty) {
  base::RunLoop run_loop;
  
  handler_->GetRpcEndpoints(base::BindLambdaForTesting(
      [&](std::vector<dappnet::mojom::RpcEndpointPtr> endpoints) {
        EXPECT_TRUE(endpoints.empty());
        run_loop.Quit();
      }));
  
  run_loop.Run();
}

TEST_F(DappnetSettingsHandlerTest, AddValidRpcEndpoint) {
  auto endpoint = dappnet::mojom::RpcEndpoint::New();
  endpoint->id = "test-id";
  endpoint->url = "https://mainnet.infura.io/v3/key";
  endpoint->name = "Test Mainnet";
  endpoint->chain_id = 1;
  endpoint->is_default = false;
  
  base::RunLoop run_loop;
  
  handler_->AddRpcEndpoint(
      std::move(endpoint),
      base::BindLambdaForTesting(
          [&](bool success, const std::string& error) {
            EXPECT_TRUE(success);
            EXPECT_TRUE(error.empty());
            run_loop.Quit();
          }));
  
  run_loop.Run();
}

TEST_F(DappnetSettingsHandlerTest, RejectInvalidUrl) {
  auto endpoint = dappnet::mojom::RpcEndpoint::New();
  endpoint->id = "test-id";
  endpoint->url = "not-a-valid-url";
  endpoint->name = "Invalid Endpoint";
  endpoint->chain_id = 1;
  endpoint->is_default = false;
  
  base::RunLoop run_loop;
  
  handler_->AddRpcEndpoint(
      std::move(endpoint),
      base::BindLambdaForTesting(
          [&](bool success, const std::string& error) {
            EXPECT_FALSE(success);
            EXPECT_FALSE(error.empty());
            EXPECT_EQ("Invalid URL", error);
            run_loop.Quit();
          }));
  
  run_loop.Run();
}

TEST_F(DappnetSettingsHandlerTest, RejectInvalidChainId) {
  auto endpoint = dappnet::mojom::RpcEndpoint::New();
  endpoint->id = "test-id";
  endpoint->url = "https://mainnet.infura.io/v3/key";
  endpoint->name = "Invalid Chain";
  endpoint->chain_id = -1;  // Invalid chain ID
  endpoint->is_default = false;
  
  base::RunLoop run_loop;
  
  handler_->AddRpcEndpoint(
      std::move(endpoint),
      base::BindLambdaForTesting(
          [&](bool success, const std::string& error) {
            EXPECT_FALSE(success);
            EXPECT_FALSE(error.empty());
            EXPECT_EQ("Invalid chain ID", error);
            run_loop.Quit();
          }));
  
  run_loop.Run();
}

TEST_F(DappnetSettingsHandlerTest, GetGatewayStatusInitiallyNotRunning) {
  base::RunLoop run_loop;
  
  handler_->GetGatewayStatus(base::BindLambdaForTesting(
      [&](dappnet::mojom::GatewayStatusPtr status) {
        EXPECT_FALSE(status->is_running);
        EXPECT_EQ(0, status->pid);
        EXPECT_TRUE(status->error_message.empty());
        run_loop.Quit();
      }));
  
  run_loop.Run();
}

TEST_F(DappnetSettingsHandlerTest, GetIpfsStatusInitiallyNotRunning) {
  base::RunLoop run_loop;
  
  handler_->GetIpfsStatus(base::BindLambdaForTesting(
      [&](dappnet::mojom::IpfsStatusPtr status) {
        EXPECT_FALSE(status->is_running);
        EXPECT_EQ(0, status->peer_count);
        run_loop.Quit();
      }));
  
  run_loop.Run();
}

TEST_F(DappnetSettingsHandlerTest, TestRpcConnectionValidUrl) {
  base::RunLoop run_loop;
  
  handler_->TestRpcConnection(
      "https://mainnet.infura.io/v3/test-key",
      base::BindLambdaForTesting(
          [&](bool connected, const std::string& error) {
            EXPECT_TRUE(connected);
            EXPECT_TRUE(error.empty());
            run_loop.Quit();
          }));
  
  run_loop.Run();
}

TEST_F(DappnetSettingsHandlerTest, TestRpcConnectionInvalidUrl) {
  base::RunLoop run_loop;
  
  handler_->TestRpcConnection(
      "not-a-url",
      base::BindLambdaForTesting(
          [&](bool connected, const std::string& error) {
            EXPECT_FALSE(connected);
            EXPECT_FALSE(error.empty());
            run_loop.Quit();
          }));
  
  run_loop.Run();
}