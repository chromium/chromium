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
#include "content/public/test/navigation_simulator.h"
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

class ActionDelegateUtilTest : public content::RenderViewHostTestHarness {
 public:
  ActionDelegateUtilTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~ActionDelegateUtilTest() override {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    ON_CALL(mock_action_delegate_, GetUserData)
        .WillByDefault(Return(&user_data_));
    ON_CALL(mock_action_delegate_, GetWebsiteLoginManager)
        .WillByDefault(Return(&mock_website_login_manager_));
  }

  MOCK_METHOD2(MockAction,
               void(const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> done));

  MOCK_METHOD3(MockIndexedAction,
               void(int index,
                    const ElementFinder::Result&,
                    base::OnceCallback<void(const ClientStatus&)> done));

  MOCK_METHOD1(MockDone, void(const ClientStatus& status));

  MOCK_METHOD2(MockGetAction,
               void(const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&,
                                            const std::string&)> done));

  MOCK_METHOD2(MockDoneGet,
               void(const ClientStatus& status, const std::string& value));

  MOCK_METHOD3(MockValueAction,
               void(const std::string& value,
                    const ElementFinder::Result& element,
                    base::OnceCallback<void(const ClientStatus&)> done));

 protected:
  MockActionDelegate mock_action_delegate_;
  UserData user_data_;
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

TEST_F(ActionDelegateUtilTest, FindElementAndExecuteSingleAction) {
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

TEST_F(ActionDelegateUtilTest, FindElementAndExecuteMultipleActions) {
  InSequence sequence;

  Selector expected_selector({"#element"});
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);

  EXPECT_CALL(*this, MockIndexedAction(1, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(*this, MockIndexedAction(2, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(*this, MockIndexedAction(3, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(*this, MockDone(EqualsStatus(OkClientStatus())));

  auto actions = std::make_unique<ElementActionVector>();
  actions->emplace_back(base::BindOnce(
      &ActionDelegateUtilTest::MockIndexedAction, base::Unretained(this), 1));
  actions->emplace_back(base::BindOnce(
      &ActionDelegateUtilTest::MockIndexedAction, base::Unretained(this), 2));
  actions->emplace_back(base::BindOnce(
      &ActionDelegateUtilTest::MockIndexedAction, base::Unretained(this), 3));

  FindElementAndPerform(&mock_action_delegate_, expected_selector,
                        base::BindOnce(&PerformAll, std::move(actions)),
                        base::BindOnce(&ActionDelegateUtilTest::MockDone,
                                       base::Unretained(this)));
}

TEST_F(ActionDelegateUtilTest,
       FindElementAndExecuteMultipleActionsAbortsOnError) {
  InSequence sequence;

  Selector expected_selector({"#element"});
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);

  EXPECT_CALL(*this, MockIndexedAction(1, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(*this, MockIndexedAction(2, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(ClientStatus(UNEXPECTED_JS_ERROR)));
  EXPECT_CALL(*this, MockIndexedAction(3, EqualsElement(expected_element), _))
      .Times(0);
  EXPECT_CALL(*this, MockDone(EqualsStatus(ClientStatus(UNEXPECTED_JS_ERROR))));

  auto actions = std::make_unique<ElementActionVector>();
  actions->emplace_back(base::BindOnce(
      &ActionDelegateUtilTest::MockIndexedAction, base::Unretained(this), 1));
  actions->emplace_back(base::BindOnce(
      &ActionDelegateUtilTest::MockIndexedAction, base::Unretained(this), 2));
  actions->emplace_back(base::BindOnce(
      &ActionDelegateUtilTest::MockIndexedAction, base::Unretained(this), 3));

  FindElementAndPerform(&mock_action_delegate_, expected_selector,
                        base::BindOnce(&PerformAll, std::move(actions)),
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
      .WillOnce(RunOnceCallback<3>(OkClientStatus(),
                                   base::TimeDelta::FromSeconds(0)));
  EXPECT_CALL(*mock_delegate, ScrollToElementPosition(
                                  _, _, _, EqualsElement(expected_element), _))
      .Times(0);
  EXPECT_CALL(*this, MockDone(_)).Times(0);

  auto actions = std::make_unique<ElementActionVector>();

  AddStepIgnoreTiming(
      base::BindOnce(&ActionDelegate::WaitUntilDocumentIsInReadyState,
                     mock_delegate->GetWeakPtr(),
                     base::TimeDelta::FromMilliseconds(0), DOCUMENT_COMPLETE),
      actions.get());
  actions->emplace_back(base::BindOnce(
      [](base::OnceCallback<void()> destroy_delegate,
         const ElementFinder::Result& element,
         base::OnceCallback<void(const ClientStatus&)> done) {
        std::move(destroy_delegate).Run();
        std::move(done).Run(OkClientStatus());
      },
      base::BindLambdaForTesting([&]() { mock_delegate.reset(); })));
  actions->emplace_back(base::BindOnce(
      &ActionDelegate::ScrollToElementPosition, mock_delegate->GetWeakPtr(),
      Selector({"#element"}), TopPadding(), nullptr));

  FindElementAndPerform(mock_delegate.get(), expected_selector,
                        base::BindOnce(&PerformAll, std::move(actions)),
                        base::BindOnce(&ActionDelegateUtilTest::MockDone,
                                       base::Unretained(this)));
}

TEST_F(ActionDelegateUtilTest, TakeElementAndPerform) {
  auto expected_element = std::make_unique<ElementFinder::Result>();

  EXPECT_CALL(mock_action_delegate_, FindElement(_, _)).Times(0);

  EXPECT_CALL(*this, MockAction(EqualsElement(*expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));
  EXPECT_CALL(*this, MockDone(EqualsStatus(OkClientStatus())));

  TakeElementAndPerform(
      base::BindOnce(&ActionDelegateUtilTest::MockAction,
                     base::Unretained(this)),
      base::BindOnce(&ActionDelegateUtilTest::MockDone, base::Unretained(this)),
      OkClientStatus(), std::move(expected_element));
}

TEST_F(ActionDelegateUtilTest, TakeElementAndPerformWithFailedStatus) {
  auto expected_element = std::make_unique<ElementFinder::Result>();

  EXPECT_CALL(mock_action_delegate_, FindElement(_, _)).Times(0);

  EXPECT_CALL(*this, MockAction(_, _)).Times(0);
  EXPECT_CALL(*this,
              MockDone(EqualsStatus(ClientStatus(ELEMENT_RESOLUTION_FAILED))));

  TakeElementAndPerform(
      base::BindOnce(&ActionDelegateUtilTest::MockAction,
                     base::Unretained(this)),
      base::BindOnce(&ActionDelegateUtilTest::MockDone, base::Unretained(this)),
      ClientStatus(ELEMENT_RESOLUTION_FAILED), std::move(expected_element));
}

TEST_F(ActionDelegateUtilTest, TakeElementAndGetProperty) {
  auto expected_element = std::make_unique<ElementFinder::Result>();

  EXPECT_CALL(mock_action_delegate_, FindElement(_, _)).Times(0);

  EXPECT_CALL(*this, MockGetAction(EqualsElement(*expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "value"));
  EXPECT_CALL(*this, MockDoneGet(EqualsStatus(OkClientStatus()), "value"));

  TakeElementAndGetProperty<std::string>(
      base::BindOnce(&ActionDelegateUtilTest::MockGetAction,
                     base::Unretained(this)),
      base::BindOnce(&ActionDelegateUtilTest::MockDoneGet,
                     base::Unretained(this)),
      OkClientStatus(), std::move(expected_element));
}

TEST_F(ActionDelegateUtilTest, TakeElementAndGetPropertyWithFailedStatus) {
  auto expected_element = std::make_unique<ElementFinder::Result>();

  EXPECT_CALL(mock_action_delegate_, FindElement(_, _)).Times(0);

  EXPECT_CALL(*this, MockGetAction(_, _)).Times(0);
  EXPECT_CALL(*this,
              MockDoneGet(EqualsStatus(ClientStatus(ELEMENT_RESOLUTION_FAILED)),
                          std::string()));

  TakeElementAndGetProperty<std::string>(
      base::BindOnce(&ActionDelegateUtilTest::MockGetAction,
                     base::Unretained(this)),
      base::BindOnce(&ActionDelegateUtilTest::MockDoneGet,
                     base::Unretained(this)),
      ClientStatus(ELEMENT_RESOLUTION_FAILED), std::move(expected_element));
}

TEST_F(ActionDelegateUtilTest, PerformWithStringValue) {
  auto element = std::make_unique<ElementFinder::Result>();

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
  auto element = std::make_unique<ElementFinder::Result>();

  EXPECT_CALL(*this, MockValueAction("John", _, _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(*this, MockDone(EqualsStatus(OkClientStatus())));

  std::unique_ptr<autofill::AutofillProfile> contact =
      std::make_unique<autofill::AutofillProfile>(base::GenerateGUID(),
                                                  autofill::test::kEmptyOrigin);
  autofill::test::SetProfileInfo(contact.get(), "John", /* middle name */ "",
                                 "Doe", "", "", "", "", "", "", "", "", "");
  user_data_.selected_addresses_["contact"] = std::move(contact);

  TextValue text_value;
  auto* autofill_value = text_value.mutable_autofill_value();
  autofill_value->mutable_profile()->set_identifier("contact");
  autofill_value->set_value_expression(
      base::StrCat({"${",
                    base::NumberToString(static_cast<int>(
                        autofill::ServerFieldType::NAME_FIRST)),
                    "}"}));

  PerformWithTextValue(&mock_action_delegate_, text_value,
                       base::BindOnce(&ActionDelegateUtilTest::MockValueAction,
                                      base::Unretained(this)),
                       *element,
                       base::BindOnce(&ActionDelegateUtilTest::MockDone,
                                      base::Unretained(this)));
}

TEST_F(ActionDelegateUtilTest, PerformWithPasswordManagerValue) {
  auto element = std::make_unique<ElementFinder::Result>();
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://www.example.com"), web_contents()->GetMainFrame());
  element->container_frame_host = web_contents()->GetMainFrame();

  user_data_.selected_login_ = base::make_optional<WebsiteLoginManager::Login>(
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
  auto element = std::make_unique<ElementFinder::Result>();
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://www.other.com"), web_contents()->GetMainFrame());
  element->container_frame_host = web_contents()->GetMainFrame();

  user_data_.selected_login_ = base::make_optional<WebsiteLoginManager::Login>(
      GURL("https://www.example.com"), "username");

  EXPECT_CALL(*this, MockValueAction("username", _, _)).Times(0);
  EXPECT_CALL(*this, MockDone(_));

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

TEST_F(ActionDelegateUtilTest, PerformWithClientMemoryKey) {
  auto element = std::make_unique<ElementFinder::Result>();

  ValueProto value_proto;
  value_proto.mutable_strings()->add_values("Hello World");
  user_data_.additional_values_["key"] = value_proto;

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

}  // namespace
}  // namespace action_delegate_util
}  // namespace autofill_assistant
