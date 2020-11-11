// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/content_password_manager_driver.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/core/browser/logging/stub_log_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/password_manager/core/browser/password_form_filling.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

using autofill::ParsingResult;
using autofill::PasswordFormFillData;
using base::ASCIIToUTF16;
using testing::_;
using testing::Return;

namespace password_manager {

namespace {

class MockLogManager : public autofill::StubLogManager {
 public:
  MOCK_METHOD(bool, IsLoggingActive, (), (const override));
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;
  ~MockPasswordManagerClient() override = default;

  MOCK_METHOD(const autofill::LogManager*, GetLogManager, (), (const override));
#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)
  MOCK_METHOD(void,
              CheckSafeBrowsingReputation,
              (const GURL&, const GURL&),
              (override));
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordManagerClient);
};

class FakePasswordAutofillAgent
    : public autofill::mojom::PasswordAutofillAgent {
 public:
  void BindPendingReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<autofill::mojom::PasswordAutofillAgent>(
            std::move(handle)));
  }

  bool called_set_logging_state() { return called_set_logging_state_; }

  bool logging_state_active() { return logging_state_active_; }

  void reset_data() {
    called_set_logging_state_ = false;
    logging_state_active_ = false;
  }

  // autofill::mojom::PasswordAutofillAgent:
  MOCK_METHOD(void,
              FillPasswordForm,
              (const PasswordFormFillData&),
              (override));
  MOCK_METHOD(void, InformNoSavedCredentials, (bool), (override));
  MOCK_METHOD(void,
              FillIntoFocusedField,
              (bool, const base::string16&),
              (override));
  MOCK_METHOD(void, TouchToFillClosed, (bool), (override));
  MOCK_METHOD(void,
              AnnotateFieldsWithParsingResult,
              (const ParsingResult&),
              (override));

 private:
  void SetLoggingState(bool active) override {
    called_set_logging_state_ = true;
    logging_state_active_ = active;
  }

  // Records whether SetLoggingState() gets called.
  bool called_set_logging_state_ = false;
  // Records data received via SetLoggingState() call.
  bool logging_state_active_ = false;

  mojo::AssociatedReceiver<autofill::mojom::PasswordAutofillAgent> receiver_{
      this};
};

PasswordFormFillData GetTestPasswordFormFillData() {
  // Create the current form on the page.
  PasswordForm form_on_page;
  form_on_page.url = GURL("https://foo.com/");
  form_on_page.action = GURL("https://foo.com/login");
  form_on_page.signon_realm = "https://foo.com/";
  form_on_page.scheme = PasswordForm::Scheme::kHtml;

  // Create an exact match in the database.
  PasswordForm preferred_match = form_on_page;
  preferred_match.username_element = ASCIIToUTF16("username");
  preferred_match.username_value = ASCIIToUTF16("test@gmail.com");
  preferred_match.password_element = ASCIIToUTF16("password");
  preferred_match.password_value = ASCIIToUTF16("test");

  std::vector<const PasswordForm*> matches;
  PasswordForm non_preferred_match = preferred_match;
  non_preferred_match.username_value = ASCIIToUTF16("test1@gmail.com");
  non_preferred_match.password_value = ASCIIToUTF16("test1");
  matches.push_back(&non_preferred_match);

  return CreatePasswordFormFillData(form_on_page, matches, preferred_match,
                                    true);
}

MATCHER(WerePasswordsCleared, "Passwords not cleared") {
  if (!arg.password_field.value.empty())
    return false;

  for (auto& credentials : arg.additional_logins)
    if (!credentials.password.empty())
      return false;

  return true;
}

}  // namespace

class ContentPasswordManagerDriverTest
    : public content::RenderViewHostTestHarness,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    ON_CALL(password_manager_client_, GetLogManager())
        .WillByDefault(Return(&log_manager_));

    blink::AssociatedInterfaceProvider* remote_interfaces =
        web_contents()->GetMainFrame()->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        autofill::mojom::PasswordAutofillAgent::Name_,
        base::BindRepeating(&FakePasswordAutofillAgent::BindPendingReceiver,
                            base::Unretained(&fake_agent_)));
  }

  bool WasLoggingActivationMessageSent(bool* activation_flag) {
    base::RunLoop().RunUntilIdle();
    if (!fake_agent_.called_set_logging_state())
      return false;

    if (activation_flag)
      *activation_flag = fake_agent_.logging_state_active();
    fake_agent_.reset_data();
    return true;
  }

 protected:
  MockLogManager log_manager_;
  MockPasswordManagerClient password_manager_client_;
  autofill::TestAutofillClient autofill_client_;

  FakePasswordAutofillAgent fake_agent_;
};

TEST_P(ContentPasswordManagerDriverTest, SendLoggingStateInCtor) {
  const bool should_allow_logging = GetParam();
  EXPECT_CALL(log_manager_, IsLoggingActive())
      .WillRepeatedly(Return(should_allow_logging));
  std::unique_ptr<ContentPasswordManagerDriver> driver(
      new ContentPasswordManagerDriver(main_rfh(), &password_manager_client_,
                                       &autofill_client_));

  if (should_allow_logging) {
    bool logging_activated = false;
    EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_activated));
    EXPECT_TRUE(logging_activated);
  } else {
    bool logging_activated = true;
    EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_activated));
    EXPECT_FALSE(logging_activated);
  }
}

TEST_P(ContentPasswordManagerDriverTest, SendLoggingStateAfterLogManagerReady) {
  const bool should_allow_logging = GetParam();
  EXPECT_CALL(password_manager_client_, GetLogManager())
      .WillOnce(Return(nullptr));
  std::unique_ptr<ContentPasswordManagerDriver> driver(
      new ContentPasswordManagerDriver(main_rfh(), &password_manager_client_,
                                       &autofill_client_));
  // Because log manager is not ready yet, should have no logging state sent.
  EXPECT_FALSE(WasLoggingActivationMessageSent(nullptr));

  // Log manager is ready, send logging state actually.
  EXPECT_CALL(password_manager_client_, GetLogManager())
      .WillOnce(Return(&log_manager_));
  EXPECT_CALL(log_manager_, IsLoggingActive())
      .WillRepeatedly(Return(should_allow_logging));
  driver->SendLoggingAvailability();
  bool logging_activated = false;
  EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_activated));
  EXPECT_EQ(should_allow_logging, logging_activated);
}

TEST_F(ContentPasswordManagerDriverTest, ClearPasswordsOnAutofill) {
  std::unique_ptr<ContentPasswordManagerDriver> driver(
      new ContentPasswordManagerDriver(main_rfh(), &password_manager_client_,
                                       &autofill_client_));

  PasswordFormFillData fill_data = GetTestPasswordFormFillData();
  fill_data.wait_for_username = true;
  EXPECT_CALL(fake_agent_, FillPasswordForm(WerePasswordsCleared()));
  driver->FillPasswordForm(fill_data);
  base::RunLoop().RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(All,
                         ContentPasswordManagerDriverTest,
                         testing::Values(true, false));

}  // namespace password_manager
