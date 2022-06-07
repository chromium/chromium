// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/action_delegate_util.h"

#include "base/guid.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/mock_website_login_manager.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/web/element_store.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace autofill_assistant {
namespace action_delegate_util {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;

class ActionDelegateUtilTest : public testing::Test {
 public:
  ActionDelegateUtilTest() {}

  void SetUp() override {
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        &browser_context_, nullptr);

    ON_CALL(mock_action_delegate_, GetUserData)
        .WillByDefault(Return(&user_data_));
    ON_CALL(mock_action_delegate_, GetUserModel)
        .WillByDefault(Return(&user_model_));
    ON_CALL(mock_action_delegate_, GetWebsiteLoginManager)
        .WillByDefault(Return(&mock_website_login_manager_));
  }

  MOCK_METHOD2(MockAction,
               void(const ElementFinderResult& element,
                    base::OnceCallback<void(const ClientStatus&)> done));

  MOCK_METHOD3(MockIndexedAction,
               void(int index,
                    const ElementFinderResult&,
                    base::OnceCallback<void(const ClientStatus&)> done));

  MOCK_METHOD1(MockDone, void(const ClientStatus& status));

  MOCK_METHOD2(MockGetAction,
               void(const ElementFinderResult& element,
                    base::OnceCallback<void(const ClientStatus&,
                                            const std::string&)> done));

  MOCK_METHOD2(MockDoneGet,
               void(const ClientStatus& status, const std::string& value));

  MOCK_METHOD3(MockValueAction,
               void(const std::string& value,
                    const ElementFinderResult& element,
                    base::OnceCallback<void(const ClientStatus&)> done));

  MOCK_METHOD3(MockElementAction,
               void(const ElementFinderResult& parameter_element,
                    const ElementFinderResult& element,
                    base::OnceCallback<void(const ClientStatus&)> done));

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  content::TestBrowserContext browser_context_;
  std::unique_ptr<content::WebContents> web_contents_;
  MockActionDelegate mock_action_delegate_;
  UserData user_data_;
  UserModel user_model_;
  MockWebsiteLoginManager mock_website_login_manager_;
};

TEST_F(ActionDelegateUtilTest, FindElementFails) {
  EXPECT_CALL(mock_action_delegate_, FindElement(_, _))
      .WillOnce(
          RunOnceCallback<1>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));
  EXPECT_CALL(*this, MockAction(_, _)).Times(0);
  EXPECT_CALL(*this,
              MockDone(EqualsStatus(ClientStatus(ELEMENT_RESOLUTION_FAILED))));

  FindElementAndPerform(&mock_action_delegate_, Selector({"#nothing"}),
                        base::BindOnce(&ActionDelegateUtilTest::MockAction,
                                       base::Unretained(this)),
                        base::BindOnce(&ActionDelegateUtilTest::MockDone,
                                       base::Unretained(this)));
}

TEST_F(ActionDelegateUtilTest, FindElementAndExecuteAction) {
  Selector expected_selector({"#element"});
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);

  EXPECT_CALL(*this, MockAction(EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));
  EXPECT_CALL(*this, MockDone(EqualsStatus(OkClientStatus())));

  FindElementAndPerform(&mock_action_delegate_, expected_selector,
                        base::BindOnce(&ActionDelegateUtilTest::MockAction,
                                       base::Unretained(this)),
                        base::BindOnce(&ActionDelegateUtilTest::MockDone,
                                       base::Unretained(this)));
}

TEST_F(ActionDelegateUtilTest, ActionDelegateDeletedDuringExecution) {
  InSequence sequence;

  auto mock_delegate = std::make_unique<MockActionDelegate>();

  Selector expected_selector({"#element"});
  auto expected_element =
      test_util::MockFindElement(*mock_delegate, expected_selector);

  EXPECT_CALL(*mock_delegate, WaitUntilDocumentIsInReadyState(_, _, _, _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus(), base::Seconds(0)));
  // No second call to WaitUntilDocumentIsInReadyState.
  EXPECT_CALL(*this, MockDone(_)).Times(0);

  auto actions = std::make_unique<element_action_util::ElementActionVector>();

  AddStepIgnoreTiming(
      base::BindOnce(&ActionDelegate::WaitUntilDocumentIsInReadyState,
                     mock_delegate->GetWeakPtr(), base::Milliseconds(0),
                     DOCUMENT_COMPLETE),
      actions.get());
  actions->emplace_back(base::BindOnce(
      [](base::OnceCallback<void()> destroy_delegate,
         const ElementFinderResult& element,
         base::OnceCallback<void(const ClientStatus&)> done) {
        std::move(destroy_delegate).Run();
        std::move(done).Run(OkClientStatus());
      },
      base::BindLambdaForTesting([&]() { mock_delegate.reset(); })));
  AddStepIgnoreTiming(
      base::BindOnce(&ActionDelegate::WaitUntilDocumentIsInReadyState,
                     mock_delegate->GetWeakPtr(), base::Milliseconds(0),
                     DOCUMENT_COMPLETE),
      actions.get());

  FindElementAndPerform(
      mock_delegate.get(), expected_selector,
      base::BindOnce(&element_action_util::PerformAll, std::move(actions)),
      base::BindOnce(&ActionDelegateUtilTest::MockDone,
                     base::Unretained(this)));
}

TEST_F(ActionDelegateUtilTest, PerformWithStringValue) {
  auto element = std::make_unique<ElementFinderResult>();

  EXPECT_CALL(*this, MockValueAction("Hello World", _, _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(*this, MockDone(EqualsStatus(OkClientStatus())));

  TextValue text_value;
  text_value.set_text("Hello World");

  PerformWithTextValue(&mock_action_delegate_, text_value,
                       base::BindOnce(&ActionDelegateUtilTest::MockValueAction,
                                      base::Unretained(this)),
                       *element,
                       base::BindOnce(&ActionDelegateUtilTest::MockDone,
                                      base::Unretained(this)));
}

TEST_F(ActionDelegateUtilTest, PerformWithAutofillValue) {
  auto element = std::make_unique<ElementFinderResult>();

  EXPECT_CALL(*this, MockValueAction("John", _, _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(*this, MockDone(EqualsStatus(OkClientStatus())));

  std::unique_ptr<autofill::AutofillProfile> contact =
      std::make_unique<autofill::AutofillProfile>(base::GenerateGUID(),
                                                  autofill::test::kEmptyOrigin);
  autofill::test::SetProfileInfo(contact.get(), "John", /* middle name */ "",
                                 "Doe", "", "", "", "", "", "", "", "", "");
  user_model_.SetSelectedAutofillProfile("contact", std::move(contact),
                                         &user_data_);

  TextValue text_value;
  text_value.mutable_autofill_value()->mutable_profile()->set_identifier(
      "contact");
  text_value.mutable_autofill_value()
      ->mutable_value_expression()
      ->add_chunk()
      ->set_key(static_cast<int>(autofill::ServerFieldType::NAME_FIRST));

  PerformWithTextValue(&mock_action_delegate_, text_value,
                       base::BindOnce(&ActionDelegateUtilTest::MockValueAction,
                                      base::Unretained(this)),
                       *element,
                       base::BindOnce(&ActionDelegateUtilTest::MockDone,
                                      base::Unretained(this)));
}

TEST_F(ActionDelegateUtilTest, PerformWithPasswordManagerValue) {
  auto element = std::make_unique<ElementFinderResult>();
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://www.example.com"));
  element->SetRenderFrameHost(web_contents_->GetPrimaryMainFrame());

  user_data_.selected_login_ = absl::make_optional<WebsiteLoginManager::Login>(
      GURL("https://www.example.com"), "username");

  EXPECT_CALL(*this, MockValueAction("username", _, _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(*this, MockDone(EqualsStatus(OkClientStatus())));

  TextValue text_value;
  auto* password_manager_value = text_value.mutable_password_manager_value();
  password_manager_value->set_credential_type(PasswordManagerValue::USERNAME);

  PerformWithTextValue(&mock_action_delegate_, text_value,
                       base::BindOnce(&ActionDelegateUtilTest::MockValueAction,
                                      base::Unretained(this)),
                       *element,
                       base::BindOnce(&ActionDelegateUtilTest::MockDone,
                                      base::Unretained(this)));
}

TEST_F(ActionDelegateUtilTest, PerformWithFailingPasswordManagerValue) {
  auto element = std::make_unique<ElementFinderResult>();
  element->SetRenderFrameHost(web_contents_->GetPrimaryMainFrame());

  user_data_.selected_login_ = absl::make_optional<WebsiteLoginManager::Login>(
      GURL("https://www.example.com"), "username");

  EXPECT_CALL(mock_website_login_manager_, GetPasswordForLogin(_, _))
      .WillOnce(RunOnceCallback<1>(false, std::string()));

  EXPECT_CALL(*this, MockValueAction(_, _, _)).Times(0);
  EXPECT_CALL(
      *this, MockDone(EqualsStatus(ClientStatus(AUTOFILL_INFO_NOT_AVAILABLE))));

  TextValue text_value;
  auto* password_manager_value = text_value.mutable_password_manager_value();
  password_manager_value->set_credential_type(PasswordManagerValue::PASSWORD);

  PerformWithTextValue(&mock_action_delegate_, text_value,
                       base::BindOnce(&ActionDelegateUtilTest::MockValueAction,
                                      base::Unretained(this)),
                       *element,
                       base::BindOnce(&ActionDelegateUtilTest::MockDone,
                                      base::Unretained(this)));
}

TEST_F(ActionDelegateUtilTest, PerformWithClientMemoryKey) {
  auto element = std::make_unique<ElementFinderResult>();

  ValueProto value_proto;
  value_proto.mutable_strings()->add_values("Hello World");
  user_data_.SetAdditionalValue("key", value_proto);

  EXPECT_CALL(*this, MockValueAction("Hello World", _, _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(*this, MockDone(EqualsStatus(OkClientStatus())));

  TextValue text_value;
  text_value.set_client_memory_key("key");

  PerformWithTextValue(&mock_action_delegate_, text_value,
                       base::BindOnce(&ActionDelegateUtilTest::MockValueAction,
                                      base::Unretained(this)),
                       *element,
                       base::BindOnce(&ActionDelegateUtilTest::MockDone,
                                      base::Unretained(this)));
}

TEST_F(ActionDelegateUtilTest, PerformWithExistingElementValue) {
  auto element = std::make_unique<ElementFinderResult>();

  ElementFinderResult option;
  option.SetObjectId("option");
  mock_action_delegate_.GetElementStore()->AddElement("o", option.dom_object());

  EXPECT_CALL(*this, MockElementAction(EqualsElement(option), _, _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(*this, MockDone(EqualsStatus(OkClientStatus())));

  ClientIdProto option_id;
  option_id.set_identifier("o");

  PerformWithElementValue(
      &mock_action_delegate_, option_id,
      base::BindOnce(&ActionDelegateUtilTest::MockElementAction,
                     base::Unretained(this)),
      *element,
      base::BindOnce(&ActionDelegateUtilTest::MockDone,
                     base::Unretained(this)));
}

TEST_F(ActionDelegateUtilTest, PerformWithMissingElementValue) {
  auto element = std::make_unique<ElementFinderResult>();

  EXPECT_CALL(*this, MockElementAction(_, _, _)).Times(0);
  EXPECT_CALL(
      *this, MockDone(EqualsStatus(ClientStatus(CLIENT_ID_RESOLUTION_FAILED))));

  ClientIdProto option_id;
  option_id.set_identifier("o");

  PerformWithElementValue(
      &mock_action_delegate_, option_id,
      base::BindOnce(&ActionDelegateUtilTest::MockElementAction,
                     base::Unretained(this)),
      *element,
      base::BindOnce(&ActionDelegateUtilTest::MockDone,
                     base::Unretained(this)));
}

}  // namespace
}  // namespace action_delegate_util
}  // namespace autofill_assistant
