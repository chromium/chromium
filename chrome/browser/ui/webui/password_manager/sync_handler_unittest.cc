// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/sync_handler.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/sync/service/sync_service_utils.h"
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

bool CallbackReturnedSuccessfully(const content::TestWebUI::CallData& data) {
  return (data.function_name() == "cr.webUIResponse") &&
         data.arg1()->is_string() &&
         (data.arg1()->GetString() == kTestCallbackId) &&
         data.arg2()->is_bool() && data.arg2()->GetBool();
}

}  // namespace

class SyncHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    mock_sync_service_ = static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildMockSyncService)));
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    auto handler = std::make_unique<SyncHandler>(profile());
    handler_ = handler.get();
    web_ui_.AddMessageHandler(std::move(handler));
    static_cast<content::WebUIMessageHandler*>(handler_)
        ->AllowJavascriptForTesting();
    web_ui_.set_web_contents(web_contents());
  }

  void TearDown() override {
    static_cast<content::WebUIMessageHandler*>(handler_)->DisallowJavascript();
    identity_test_env_adaptor_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

  AccountInfo CreateTestSyncAccount() {
    auto account_info = identity_test_env()->MakePrimaryAccountAvailable(
        "user@gmail.com", signin::ConsentLevel::kSync);
    ON_CALL(*sync_service(), HasSyncConsent).WillByDefault(Return(true));
    ON_CALL(*sync_service()->GetMockUserSettings(),
            IsInitialSyncFeatureSetupComplete())
        .WillByDefault(Return(true));
    ON_CALL(*sync_service(), GetAccountInfo)
        .WillByDefault(Return(account_info));
    return account_info;
  }

  void FireSyncStateChange() {
    static_cast<syncer::SyncServiceObserver*>(handler())->OnStateChanged(
        static_cast<syncer::SyncService*>(sync_service()));
  }

  void ExpectTrustedVaultBannerStateResponse(
      TrustedVaultBannerState expected_state) {
    auto& data = *web_ui_.call_data().back();
    ASSERT_TRUE(CallbackReturnedSuccessfully(data));
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

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  content::TestWebUI* web_ui() { return &web_ui_; }
  MockSyncService* sync_service() { return mock_sync_service_; }
  SyncHandler* handler() { return handler_; }

 private:
  content::TestWebUI web_ui_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  raw_ptr<MockSyncService, DanglingUntriaged> mock_sync_service_;
  raw_ptr<SyncHandler> handler_;
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
      .WillByDefault(testing::Return(syncer::DataTypeSet({syncer::PASSWORDS})));
  ON_CALL(*sync_service()->GetMockUserSettings(), GetAllEncryptedDataTypes())
      .WillByDefault(testing::Return(syncer::DataTypeSet({syncer::PASSWORDS})));
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
  FireSyncStateChange();
  args = GetAllFiredValuesForEventName("trusted-vault-banner-state-changed");
  ASSERT_EQ(1U, args.size());
  ASSERT_TRUE(args[0]->is_int());
  EXPECT_EQ(static_cast<int>(TrustedVaultBannerState::kNotShown),
            args[0]->GetInt());
}

TEST_F(SyncHandlerTest, GetSyncInfo) {
  CreateTestSyncAccount();

  base::Value::List args;
  args.Append(kTestCallbackId);
  web_ui()->ProcessWebUIMessage(GURL(), "GetSyncInfo", std::move(args));

  auto& data = *web_ui()->call_data().back();
  ASSERT_TRUE(CallbackReturnedSuccessfully(data));
  ASSERT_TRUE(data.arg3()->is_dict());
  // A syncing user should not be eligible for account storage.
  EXPECT_FALSE(*data.arg3()->GetDict().FindBool("isEligibleForAccountStorage"));
}

TEST_F(SyncHandlerTest, GetSyncInfoOnSyncStateChange) {
  CreateTestSyncAccount();

  FireSyncStateChange();
  std::vector<const base::Value*> state_update_args =
      GetAllFiredValuesForEventName("sync-info-changed");

  ASSERT_EQ(1U, state_update_args.size());
  ASSERT_TRUE(state_update_args[0]->is_dict());
  // A syncing user should not be eligible for account storage.
  EXPECT_FALSE(
      *state_update_args[0]->GetDict().FindBool("isEligibleForAccountStorage"));
}

TEST_F(SyncHandlerTest, AccountInfo) {
  base::Value::List args;
  args.Append(kTestCallbackId);
  web_ui()->ProcessWebUIMessage(GURL(), "GetAccountInfo", std::move(args));
  auto& data = *web_ui()->call_data().back();
  ASSERT_TRUE(CallbackReturnedSuccessfully(data));

  // Expect no accounts initially.
  base::Value::List expected_accounts;
  ASSERT_TRUE(data.arg3()->is_dict());
  EXPECT_EQ("", *data.arg3()->GetDict().FindString("email"));

  auto account_info = CreateTestSyncAccount();
  // Creating an account with IdentityTestEnvironment::MakeAccountAvailable
  // triggers identity manager observer before stored accounts info
  // can be retrieved.
  size_t num_account_change_updates =
      GetAllFiredValuesForEventName("stored-accounts-changed").size();

  // Verify that stored accounts are propagated to WebUI.
  static_cast<signin::IdentityManager::Observer*>(handler())
      ->OnExtendedAccountInfoUpdated(account_info);
  std::vector<const base::Value*> update_args =
      GetAllFiredValuesForEventName("stored-accounts-changed");
  ASSERT_EQ(num_account_change_updates + 1, update_args.size());
  ASSERT_TRUE(update_args[1]->is_dict());
  EXPECT_EQ(account_info.email, *update_args[1]->GetDict().FindString("email"));
}

TEST_F(SyncHandlerTest, NotEligibleForAccountStorageWhenSetupNotComplete) {
  CreateTestSyncAccount();
  ON_CALL(*sync_service()->GetMockUserSettings(),
          IsInitialSyncFeatureSetupComplete())
      .WillByDefault(Return(false));

  base::Value::List args;
  args.Append(kTestCallbackId);
  web_ui()->ProcessWebUIMessage(GURL(), "GetSyncInfo", std::move(args));

  auto& data = *web_ui()->call_data().back();
  ASSERT_TRUE(CallbackReturnedSuccessfully(data));
  ASSERT_TRUE(data.arg3()->is_dict());
  EXPECT_FALSE(*data.arg3()->GetDict().FindBool("isEligibleForAccountStorage"));
}

}  // namespace password_manager
