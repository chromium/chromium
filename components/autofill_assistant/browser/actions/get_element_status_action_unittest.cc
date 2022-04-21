// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/get_element_status_action.h"

#include "base/guid.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/mock_website_login_manager.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/web/element_finder_result.h"
#include "components/autofill_assistant/browser/web/element_store.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::WithArgs;

const char kValue[] = "Some Value";

class GetElementStatusActionTest : public testing::Test {
 public:
  GetElementStatusActionTest() {}

  void SetUp() override {
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        &browser_context_, nullptr);

    ON_CALL(mock_action_delegate_, GetUserData)
        .WillByDefault(Return(&user_data_));
    ON_CALL(mock_action_delegate_, GetUserModel)
        .WillByDefault(Return(&user_model_));
    ON_CALL(mock_action_delegate_, GetWebController)
        .WillByDefault(Return(&mock_web_controller_));
    ON_CALL(mock_action_delegate_, GetWebsiteLoginManager)
        .WillByDefault(Return(&mock_website_login_manager_));
    ON_CALL(mock_action_delegate_, OnShortWaitForElement(_, _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus(), base::Seconds(0)));
    test_util::MockFindAnyElement(mock_action_delegate_);
    ON_CALL(mock_web_controller_, GetStringAttribute(_, _, _))
        .WillByDefault(RunOnceCallback<2>(OkClientStatus(), kValue));

    proto_.set_value_source(GetElementStatusProto::VALUE);
  }

 protected:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_get_element_status() = proto_;
    GetElementStatusAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  content::TestBrowserContext browser_context_;
  std::unique_ptr<content::WebContents> web_contents_;
  MockActionDelegate mock_action_delegate_;
  MockWebController mock_web_controller_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  GetElementStatusProto proto_;
  UserData user_data_;
  UserModel user_model_;
  MockWebsiteLoginManager mock_website_login_manager_;
};

TEST_F(GetElementStatusActionTest, NoElementSpecifiedFails) {
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run();
}

TEST_F(GetElementStatusActionTest, EmptySelectorFails) {
  proto_.mutable_selector();
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_SELECTOR))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionFailsForSelectorNotFound) {
  Selector selector({"#element"});
  *proto_.mutable_selector() = selector.proto;
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_text_value()
      ->set_text(kValue);

  EXPECT_CALL(mock_action_delegate_, OnShortWaitForElement(selector, _))
      .WillOnce(RunOnceCallback<1>(ClientStatus(TIMED_OUT), base::Seconds(0)));

  EXPECT_CALL(callback_,
              Run(Pointee(Property(&ProcessedActionProto::status, TIMED_OUT))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionFailsForClientIdNotFound) {
  proto_.mutable_client_id()->set_identifier("element");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_text_value()
      ->set_text(kValue);

  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              CLIENT_ID_RESOLUTION_FAILED))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionReportsAllVariationsForSelector) {
  Selector selector({"#element"});
  *proto_.mutable_selector() = selector.proto;
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_text_value()
      ->set_text(kValue);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::reports,
                             SizeIs(4))))))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionReportsAllVariationsForClientId) {
  ElementFinderResult element;
  mock_action_delegate_.GetElementStore()->AddElement("element",
                                                      element.dom_object());
  proto_.mutable_client_id()->set_identifier("element");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_text_value()
      ->set_text(kValue);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::reports,
                             SizeIs(4))))))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionFailsForMismatchIfRequired) {
  Selector selector({"#element"});
  *proto_.mutable_selector() = selector.proto;
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_text_value()
      ->set_text("other");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_full_match(true);
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ELEMENT_MISMATCH),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             false)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionSucceedsForMismatchIfAllowed) {
  Selector selector({"#element"});
  *proto_.mutable_selector() = selector.proto;
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_text_value()
      ->set_text("other");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_full_match(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             false)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionSucceedsForNoExpectation) {
  Selector selector({"#element"});
  *proto_.mutable_selector() = selector.proto;
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_text_value()
      ->set_text("other");
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(&ProcessedActionProto::get_element_status_result,
                   AllOf(Property(&GetElementStatusProto::Result::not_empty,
                                  true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionSucceedsForCaseSensitiveFullMatch) {
  Selector selector({"#element"});
  *proto_.mutable_selector() = selector.proto;
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_text_value()
      ->set_text(kValue);
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->mutable_match_options()
      ->set_case_sensitive(true);
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_full_match(true);
  proto_.set_mismatch_should_fail(true);

  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, selector);
  EXPECT_CALL(mock_web_controller_,
              GetStringAttribute(ElementsAre("value"),
                                 EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus(), kValue));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionSucceedsForCaseSensitiveContains) {
  Selector selector({"#element"});
  *proto_.mutable_selector() = selector.proto;
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_text_value()
      ->set_text("me Va");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->mutable_match_options()
      ->set_case_sensitive(true);
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_contains(true);
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionSucceedsForCaseSensitiveStartsWith) {
  Selector selector({"#element"});
  *proto_.mutable_selector() = selector.proto;
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_text_value()
      ->set_text("Some");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->mutable_match_options()
      ->set_case_sensitive(true);
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_starts_with(true);
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionSucceedsForCaseSensitiveEndsWith) {
  Selector selector({"#element"});
  *proto_.mutable_selector() = selector.proto;
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_text_value()
      ->set_text("Value");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->mutable_match_options()
      ->set_case_sensitive(true);
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_ends_with(true);
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionSucceedsForCaseInsensitiveFullMatch) {
  Selector selector({"#element"});
  *proto_.mutable_selector() = selector.proto;
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_text_value()
      ->set_text("sOmE vAlUe");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->mutable_match_options()
      ->set_case_sensitive(false);
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_full_match(true);
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionSucceedsForFullMatchWithoutSpaces) {
  Selector selector({"#element"});
  *proto_.mutable_selector() = selector.proto;
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_text_value()
      ->set_text("S o m eV a l u e");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->mutable_match_options()
      ->set_case_sensitive(true);
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->mutable_match_options()
      ->set_remove_space(true);
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_full_match(true);
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, EmptyTextForEmptyValueIsSuccess) {
  ON_CALL(mock_web_controller_, GetStringAttribute(_, _, _))
      .WillByDefault(RunOnceCallback<2>(OkClientStatus(), std::string()));

  Selector selector({"#element"});
  *proto_.mutable_selector() = selector.proto;
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_text_value()
      ->set_text(std::string());
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_full_match(true);
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(
                  Property(&GetElementStatusProto::Result::not_empty, false),
                  Property(&GetElementStatusProto::Result::match_success, true),
                  Property(&GetElementStatusProto::Result::expected_empty_match,
                           true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, InnerTextLookupSuccess) {
  Selector selector({"#element"});
  *proto_.mutable_selector() = selector.proto;
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_text_value()
      ->set_text(kValue);
  proto_.set_value_source(GetElementStatusProto::INNER_TEXT);
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_full_match(true);
  proto_.set_mismatch_should_fail(true);

  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, selector);
  EXPECT_CALL(mock_web_controller_,
              GetStringAttribute(ElementsAre("innerText"),
                                 EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus(), kValue));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, MatchingValueWithRegexpCaseSensitive) {
  Selector selector({"#element"});
  *proto_.mutable_selector() = selector.proto;
  proto_.mutable_expected_value_match()->mutable_text_match()->set_re2("Valu.");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->mutable_match_options()
      ->set_case_sensitive(true);
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_ends_with(true);
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, MatchingValueWithRegexpCaseInsensitive) {
  Selector selector({"#element"});
  *proto_.mutable_selector() = selector.proto;
  proto_.mutable_expected_value_match()->mutable_text_match()->set_re2("vAlU.");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->mutable_match_options()
      ->set_case_sensitive(false);
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_ends_with(true);
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, ActionFailsForRegexMismatchIfRequired) {
  Selector selector({"#element"});
  *proto_.mutable_selector() = selector.proto;
  proto_.mutable_expected_value_match()->mutable_text_match()->set_re2("none");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_full_match(true);
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ELEMENT_MISMATCH),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             false)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, EmptyRegexpForEmptyValueIsSuccess) {
  ON_CALL(mock_web_controller_, GetStringAttribute(_, _, _))
      .WillByDefault(RunOnceCallback<2>(OkClientStatus(), std::string()));

  Selector selector({"#element"});
  *proto_.mutable_selector() = selector.proto;
  proto_.mutable_expected_value_match()->mutable_text_match()->set_re2("^$");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_full_match(true);
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(
                  Property(&GetElementStatusProto::Result::not_empty, false),
                  Property(&GetElementStatusProto::Result::match_success, true),
                  Property(&GetElementStatusProto::Result::expected_empty_match,
                           true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, BlankTextWithRemovingSpacesIsExpectedEmpty) {
  ON_CALL(mock_web_controller_, GetStringAttribute(_, _, _))
      .WillByDefault(RunOnceCallback<2>(OkClientStatus(), "   "));

  Selector selector({"#element"});
  *proto_.mutable_selector() = selector.proto;
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_text_value()
      ->set_text("   ");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->mutable_match_options()
      ->set_remove_space(true);
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_full_match(true);
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(
                  // The field is not empty (it is blank), but the match is
                  // still a success and expects to be empty given the
                  // configuration.
                  Property(&GetElementStatusProto::Result::not_empty, true),
                  Property(&GetElementStatusProto::Result::match_success, true),
                  Property(&GetElementStatusProto::Result::expected_empty_match,
                           true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, SucceedsWithAutofillValue) {
  autofill::AutofillProfile contact(base::GenerateGUID(),
                                    autofill::test::kEmptyOrigin);
  autofill::test::SetProfileInfo(&contact, "John", /* middle name */ "", "Doe",
                                 "", "", "", "", "", "", "", "", "");
  user_model_.SetSelectedAutofillProfile(
      "contact", std::make_unique<autofill::AutofillProfile>(contact),
      &user_data_);

  AutofillValue autofill_value;
  autofill_value.mutable_profile()->set_identifier("contact");
  autofill_value.mutable_value_expression()->add_chunk()->set_key(
      static_cast<int>(autofill::ServerFieldType::NAME_FIRST));
  autofill_value.mutable_value_expression()->add_chunk()->set_text(" ");
  autofill_value.mutable_value_expression()->add_chunk()->set_key(
      static_cast<int>(autofill::ServerFieldType::NAME_LAST));

  Selector selector({"#element"});
  *proto_.mutable_selector() = selector.proto;
  *proto_.mutable_expected_value_match()
       ->mutable_text_match()
       ->mutable_text_value()
       ->mutable_autofill_value() = autofill_value;
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation();
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(mock_web_controller_, GetStringAttribute(_, _, _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus(), "John Doe"));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, SucceedsWithPasswordManagerValue) {
  user_data_.selected_login_ = absl::make_optional<WebsiteLoginManager::Login>(
      GURL("https://www.example.com"), "username");

  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://www.example.com"));

  PasswordManagerValue password_manager_value;
  password_manager_value.set_credential_type(PasswordManagerValue::PASSWORD);

  Selector selector({"#element"});
  *proto_.mutable_selector() = selector.proto;
  *proto_.mutable_expected_value_match()
       ->mutable_text_match()
       ->mutable_text_value()
       ->mutable_password_manager_value() = password_manager_value;
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation();
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(mock_action_delegate_, FindElement(_, _))
      .WillOnce(WithArgs<1>([this](auto&& callback) {
        std::unique_ptr<ElementFinderResult> element =
            std::make_unique<ElementFinderResult>();
        element->SetRenderFrameHost(web_contents_->GetMainFrame());
        std::move(callback).Run(OkClientStatus(), std::move(element));
      }));
  EXPECT_CALL(mock_website_login_manager_, GetPasswordForLogin(_, _))
      .WillOnce(RunOnceCallback<1>(true, "password"));
  EXPECT_CALL(mock_web_controller_, GetStringAttribute(_, _, _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus(), "password"));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest, SucceedsWithClientMemoryValue) {
  ValueProto value_proto;
  value_proto.mutable_strings()->add_values("password");
  user_data_.SetAdditionalValue("__password__", value_proto);

  Selector selector({"#element"});
  *proto_.mutable_selector() = selector.proto;
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_text_value()
      ->set_client_memory_key("__password__");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation();
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(mock_web_controller_, GetStringAttribute(_, _, _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus(), "password"));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(Property(&GetElementStatusProto::Result::not_empty, true),
                    Property(&GetElementStatusProto::Result::match_success,
                             true)))))));
  Run();
}

TEST_F(GetElementStatusActionTest,
       ActionSucceedsForFullMatchAfterFindAndRemove) {
  // The website value added a leading + and contains spaces.
  ON_CALL(mock_web_controller_, GetStringAttribute(_, _, _))
      .WillByDefault(RunOnceCallback<2>(OkClientStatus(), "+1 111 222 333"));

  Selector selector({"#phone"});
  *proto_.mutable_selector() = selector.proto;
  // The input value contains a - as separator.
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_text_value()
      ->set_text("1-111-222-333");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->mutable_match_options()
      ->set_case_sensitive(true);
  // Remove all characters to end up with numbers only to compare.
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->mutable_match_options()
      ->set_find_and_remove_re2("\\+|\\s|\\-");
  proto_.mutable_expected_value_match()
      ->mutable_text_match()
      ->mutable_match_expectation()
      ->set_full_match(true);
  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::get_element_status_result,
              AllOf(
                  Property(&GetElementStatusProto::Result::not_empty, true),
                  Property(&GetElementStatusProto::Result::match_success, true),
                  Property(&GetElementStatusProto::Result::reports,
                           SizeIs(8))))))));
  Run();
}

}  // namespace
}  // namespace autofill_assistant
