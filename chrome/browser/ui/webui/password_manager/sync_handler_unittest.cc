// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/sync_handler.h"

#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/sync/test/mock_sync_service.h"
#include "content/public/test/test_web_ui.h"

using syncer::MockSyncService;
using ::testing::Return;

namespace password_manager {

namespace {

const char kTestCallbackId[] = "test-callback-id";

std::unique_ptr<KeyedService> BuildMockSyncService(
    content::BrowserContext* context) {
  return std::make_unique<testing::NiceMock<syncer::MockSyncService>>();
}

}  // namespace

class SyncHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    mock_sync_service_ = static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildMockSyncService)));

    auto handler = std::make_unique<SyncHandler>(profile());
    handler_ = handler.get();
    web_ui_.AddMessageHandler(std::move(handler));
    static_cast<content::WebUIMessageHandler*>(handler_)
        ->AllowJavascriptForTesting();
    web_ui_.set_web_contents(web_contents());
  }

  void TearDown() override {
    static_cast<content::WebUIMessageHandler*>(handler_)->DisallowJavascript();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void ExpectTrustedVaultBannerStateResponse(
      TrustedVaultBannerState expected_state) {
    auto& data = *web_ui_.call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());

    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ(kTestCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->is_bool());
    EXPECT_TRUE(data.arg2()->GetBool());
    ASSERT_TRUE(data.arg3()->is_int());
    EXPECT_EQ(static_cast<int>(expected_state), data.arg3()->GetInt());
  }

  std::vector<const base::Value*> GetAllFiredValuesForEventName(
      const std::string& event_name) {
    std::vector<const base::Value*> arguments;
    for (const std::unique_ptr<content::TestWebUI::CallData>& data :
         web_ui_.call_data()) {
      if (data->function_name() == "cr.webUIListenerCallback" &&
          data->arg1()->is_string() &&
          data->arg1()->GetString() == event_name) {
        arguments.push_back(data->arg2());
      }
    }
    return arguments;
  }

  content::TestWebUI* web_ui() { return &web_ui_; }
  MockSyncService* sync_service() { return mock_sync_service_; }
  SyncHandler* handler() { return handler_; }

 private:
  content::TestWebUI web_ui_;
  raw_ptr<MockSyncService> mock_sync_service_;
  SyncHandler* handler_;
};

TEST_F(SyncHandlerTest, HandleTrustedVaultBannerStateNotShown) {
  // Paused sync should prevent showing trusted vault banner.
  ON_CALL(*sync_service(), GetTransportState())
      .WillByDefault(Return(syncer::SyncService::TransportState::PAUSED));

  base::Value::List args;
  args.Append(kTestCallbackId);
  web_ui()->ProcessWebUIMessage(GURL(), "GetSyncTrustedVaultBannerState",
                                std::move(args));
  ExpectTrustedVaultBannerStateResponse(TrustedVaultBannerState::kNotShown);
}

TEST_F(SyncHandlerTest, HandleTrustedVaultBannerStateOptedIn) {
  ON_CALL(*sync_service()->GetMockUserSettings(), GetPassphraseType())
      .WillByDefault(Return(syncer::PassphraseType::kTrustedVaultPassphrase));

  base::Value::List args;
  args.Append(kTestCallbackId);
  web_ui()->ProcessWebUIMessage(GURL(), "GetSyncTrustedVaultBannerState",
                                std::move(args));
  ExpectTrustedVaultBannerStateResponse(TrustedVaultBannerState::kOptedIn);
}

TEST_F(SyncHandlerTest, HandleTrustedVaultBannerStateOfferOptIn) {
  ON_CALL(*sync_service(), GetTransportState())
      .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));
  ON_CALL(*sync_service(), GetActiveDataTypes())
      .WillByDefault(testing::Return(syncer::ModelTypeSet(syncer::PASSWORDS)));
  ON_CALL(*sync_service()->GetMockUserSettings(), GetEncryptedDataTypes())
      .WillByDefault(testing::Return(syncer::ModelTypeSet(syncer::PASSWORDS)));
  ON_CALL(*sync_service()->GetMockUserSettings(), GetPassphraseType())
      .WillByDefault(Return(syncer::PassphraseType::kKeystorePassphrase));
  ON_CALL(*sync_service()->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(false));
  ASSERT_TRUE(syncer::ShouldOfferTrustedVaultOptIn(
      static_cast<syncer::SyncService*>(sync_service())));

  base::Value::List args;
  args.Append(kTestCallbackId);
  web_ui()->ProcessWebUIMessage(GURL(), "GetSyncTrustedVaultBannerState",
                                std::move(args));
  ExpectTrustedVaultBannerStateResponse(TrustedVaultBannerState::kOfferOptIn);
}

TEST_F(SyncHandlerTest, TrustedVaultBannerStateChange) {
  std::vector<const base::Value*> args =
      GetAllFiredValuesForEventName("trusted-vault-banner-state-changed");
  ASSERT_EQ(0U, args.size());

  // Ensure the state is propagated.
  ON_CALL(*sync_service(), GetTransportState())
      .WillByDefault(Return(syncer::SyncService::TransportState::PAUSED));
  static_cast<syncer::SyncServiceObserver*>(handler())->OnStateChanged(
      static_cast<syncer::SyncService*>(sync_service()));
  args = GetAllFiredValuesForEventName("trusted-vault-banner-state-changed");
  ASSERT_EQ(1U, args.size());
  ASSERT_TRUE(args[0]->is_int());
  EXPECT_EQ(static_cast<int>(TrustedVaultBannerState::kNotShown),
            args[0]->GetInt());
}

}  // namespace password_manager
