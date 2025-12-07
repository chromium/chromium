// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/saved_info_handler.h"

#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/autofill/valuables_data_manager_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/test_valuables_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager_test_api.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/test_utils/valuables_data_test_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/webauthn/core/browser/test_passkey_model.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace settings {

using autofill::LoyaltyCard;
using autofill::TestPersonalDataManager;
using autofill::TestValuablesDataManager;
using password_manager::PasswordForm;
using password_manager::TestPasswordStore;

class TestSavedInfoHandler : public SavedInfoHandler {
 public:
  explicit TestSavedInfoHandler(Profile* profile) : SavedInfoHandler(profile) {}
  ~TestSavedInfoHandler() override = default;

  // Make public for testing.
  using SavedInfoHandler::set_web_ui;
};

class SavedInfoHandlerTest : public testing::Test {
 public:
  SavedInfoHandlerTest() = default;
  ~SavedInfoHandlerTest() override = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    profile_store_ = CreateAndUseTestPasswordStore(profile_.get());
    AccountPasswordStoreFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindOnce(
            [](content::BrowserContext* context)
                -> scoped_refptr<RefcountedKeyedService> { return nullptr; }));
    PasskeyModelFactory::GetInstance()->SetTestingFactory(
        profile_.get(), base::BindOnce([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<webauthn::TestPasskeyModel>();
        }));
    AffiliationServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindOnce(&SavedInfoHandlerTest::CreateAfilliationService,
                       base::Unretained(this)));
    valuables_data_manager_ = static_cast<TestValuablesDataManager*>(
        autofill::ValuablesDataManagerFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile_.get(),
                base::BindOnce([](content::BrowserContext* context)
                                   -> std::unique_ptr<KeyedService> {
                  return std::make_unique<TestValuablesDataManager>();
                })));
  }

  void TearDown() override { profile_store_->ShutdownOnUIThread(); }

  content::TestWebUI* web_ui() { return &web_ui_; }
  TestingProfile* profile() { return profile_.get(); }
  TestPasswordStore* profile_store() { return profile_store_.get(); }
  webauthn::TestPasskeyModel* passkey_model() {
    return static_cast<webauthn::TestPasskeyModel*>(
        PasskeyModelFactory::GetForProfile(profile_.get()));
  }
  affiliations::FakeAffiliationService* affiliation_service() {
    return &affiliation_service_;
  }
  TestValuablesDataManager* valuables_data_manager() {
    return valuables_data_manager_;
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  std::unique_ptr<KeyedService> CreateAfilliationService(
      content::BrowserContext* context) {
    return std::make_unique<affiliations::FakeAffiliationService>();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  content::TestWebUI web_ui_;
  scoped_refptr<TestPasswordStore> profile_store_;
  affiliations::FakeAffiliationService affiliation_service_;
  raw_ptr<TestValuablesDataManager> valuables_data_manager_;
};

TEST_F(SavedInfoHandlerTest, HandleGetPasswordCount) {
  // Add 2 passwords.
  PasswordForm form;
  form.url = GURL("https://example.com");
  form.signon_realm = form.url.spec();
  form.username_value = u"username";
  form.password_value = u"password";
  form.in_store = PasswordForm::Store::kProfileStore;
  profile_store()->AddLogin(form);
  form.username_value = u"admin";
  form.password_value = u"hunter2";
  form.in_store = PasswordForm::Store::kProfileStore;
  profile_store()->AddLogin(form);

  // Add 1 passkey.
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_credential_id("credential_id");
  passkey.set_rp_id("rp_id");
  passkey.set_user_id("user_id");
  passkey_model()->AddNewPasskeyForTesting(passkey);

  auto handler = std::make_unique<TestSavedInfoHandler>(profile());
  handler->set_web_ui(web_ui());
  handler->RegisterMessages();
  handler->AllowJavascriptForTesting();
  RunUntilIdle();  // Allow presenter to initialize

  web_ui()->ClearTrackedCalls();

  base::Value::List args;
  args.Append("test_callback_id");
  handler->HandleGetPasswordCount(args);

  EXPECT_EQ(1U, web_ui()->call_data().size());
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", data.function_name());
  EXPECT_EQ("test_callback_id", data.arg1()->GetString());
  EXPECT_TRUE(data.arg2()->GetBool());

  const base::Value::Dict& dict = data.arg3()->GetDict();
  EXPECT_EQ(2, dict.FindInt("passwordCount"));
  EXPECT_EQ(1, dict.FindInt("passkeyCount"));
}

TEST_F(SavedInfoHandlerTest, HandleGetLoyaltyCardsCount) {
  // Add 2 loyalty cards.
  std::vector<LoyaltyCard> loyalty_cards;
  loyalty_cards.push_back(autofill::test::CreateLoyaltyCard());
  loyalty_cards.push_back(autofill::test::CreateLoyaltyCard2());
  autofill::test_api(*valuables_data_manager()).SetLoyaltyCards(loyalty_cards);

  auto handler = std::make_unique<TestSavedInfoHandler>(profile());
  handler->set_web_ui(web_ui());
  handler->RegisterMessages();
  handler->AllowJavascriptForTesting();
  RunUntilIdle();  // Allow presenter to initialize

  web_ui()->ClearTrackedCalls();

  base::Value::List args;
  args.Append("test_callback_id");
  handler->HandleGetLoyaltyCardsCount(args);

  EXPECT_EQ(1U, web_ui()->call_data().size());
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", data.function_name());
  EXPECT_EQ("test_callback_id", data.arg1()->GetString());
  EXPECT_TRUE(data.arg2()->GetBool());
  EXPECT_TRUE(data.arg3()->is_int());
  EXPECT_EQ(2, data.arg3()->GetInt());
}

}  // namespace settings
