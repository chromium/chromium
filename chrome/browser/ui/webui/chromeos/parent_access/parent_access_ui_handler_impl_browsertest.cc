// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_ui_handler_impl.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_browsertest_base.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_callback.pb.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_dialog.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_ui.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_test.h"

namespace {
parent_access_ui::mojom::ParentAccessParamsPtr GetParamsForWebApprovals() {
  return parent_access_ui::mojom::ParentAccessParams::New(
      parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess,
      parent_access_ui::mojom::FlowTypeParams::NewWebApprovalsParams(
          parent_access_ui::mojom::WebApprovalsParams::New()));
}
}  // namespace

namespace chromeos {

using ParentAccessUIHandlerImplBrowserTest =
    ParentAccessChildUserBrowserTestBase;

// Verify that the access token is successfully fetched.
IN_PROC_BROWSER_TEST_F(ParentAccessUIHandlerImplBrowserTest,
                       GetOAuthTokenSuccess) {
  // Show the parent access dialog.
  ParentAccessDialogProvider provider;
  ParentAccessDialogProvider::ShowError error =
      provider.Show(GetParamsForWebApprovals(), base::DoNothing());

  // Verify dialog is showing.
  ASSERT_EQ(error, ParentAccessDialogProvider::ShowError::kNone);

  EXPECT_TRUE(content::WaitForLoadStop(GetContents()));

  ParentAccessUIHandlerImpl* handler = static_cast<ParentAccessUIHandlerImpl*>(
      GetParentAccessUI()->GetHandlerForTest());

  // Make sure the handler isn't null.
  ASSERT_NE(handler, nullptr);

  handler->GetOAuthToken(
      base::BindOnce([](parent_access_ui::mojom::GetOAuthTokenStatus status,
                        const std::string& token) -> void {
        EXPECT_EQ(parent_access_ui::mojom::GetOAuthTokenStatus::kSuccess,
                  status);
      }));
}

// Verifies that access token fetch errors are recorded.
IN_PROC_BROWSER_TEST_F(ParentAccessUIHandlerImplBrowserTest,
                       GetOAuthTokenError) {
  // Show the parent access dialog.
  ParentAccessDialogProvider provider;
  ParentAccessDialogProvider::ShowError error =
      provider.Show(GetParamsForWebApprovals(), base::DoNothing());

  // Verify dialog is showing.
  ASSERT_EQ(error, ParentAccessDialogProvider::ShowError::kNone);

  EXPECT_TRUE(content::WaitForLoadStop(GetContents()));

  ParentAccessUIHandlerImpl* handler = static_cast<ParentAccessUIHandlerImpl*>(
      GetParentAccessUI()->GetHandlerForTest());

  // Make sure the handler isn't null.
  ASSERT_NE(handler, nullptr);

  // Trigger failure to issue access token.
  identity_test_env_->SetAutomaticIssueOfAccessTokens(false);

  handler->GetOAuthToken(
      base::BindOnce([](parent_access_ui::mojom::GetOAuthTokenStatus status,
                        const std::string& token) -> void {
        EXPECT_EQ(parent_access_ui::mojom::GetOAuthTokenStatus::kError, status);
      }));
}

// Verifies that only one access token fetch is possible at a time.
IN_PROC_BROWSER_TEST_F(ParentAccessUIHandlerImplBrowserTest,
                       GetOAuthTokenOnlyOneFetchAtATimeError) {
  // Show the parent access dialog.
  ParentAccessDialogProvider provider;
  ParentAccessDialogProvider::ShowError error =
      provider.Show(GetParamsForWebApprovals(), base::DoNothing());

  // Verify dialog is showing.
  ASSERT_EQ(error, ParentAccessDialogProvider::ShowError::kNone);

  EXPECT_TRUE(content::WaitForLoadStop(GetContents()));

  ParentAccessUIHandlerImpl* handler = static_cast<ParentAccessUIHandlerImpl*>(
      GetParentAccessUI()->GetHandlerForTest());

  // Make sure the handler isn't null.
  ASSERT_NE(handler, nullptr);

  handler->GetOAuthToken(
      base::BindOnce([](parent_access_ui::mojom::GetOAuthTokenStatus status,
                        const std::string& token) -> void {
        EXPECT_EQ(parent_access_ui::mojom::GetOAuthTokenStatus::kSuccess,
                  status);
      }));

  handler->GetOAuthToken(
      base::BindOnce([](parent_access_ui::mojom::GetOAuthTokenStatus status,
                        const std::string& token) -> void {
        EXPECT_EQ(
            parent_access_ui::mojom::GetOAuthTokenStatus::kOnlyOneFetchAtATime,
            status);
      }));
}

MATCHER_P(EqualsProto,
          message,
          "Match a proto Message equal to the matcher's argument.") {
  std::string expected_serialized = message.SerializeAsString();
  std::string actual_serialized = arg.SerializeAsString();
  return expected_serialized == actual_serialized;
}

// Verifies that the parent approvals sequence is handled correctly.
IN_PROC_BROWSER_TEST_F(ParentAccessUIHandlerImplBrowserTest,
                       OnParentVerifiedAndApproved) {
  // Construct the ParentAccessCallback
  kids::platform::parentaccess::client::proto::ParentAccessCallback
      parent_access_callback;
  kids::platform::parentaccess::client::proto::OnParentVerified*
      on_parent_verified = parent_access_callback.mutable_on_parent_verified();
  kids::platform::parentaccess::client::proto::ParentAccessToken* pat =
      on_parent_verified->mutable_parent_access_token();
  pat->set_token("TEST_TOKEN");
  kids::platform::parentaccess::client::proto::Timestamp* expire_time =
      pat->mutable_expire_time();
  expire_time->set_seconds(123456);
  // Nanoseconds will be ignored.
  expire_time->set_nanos(567890);

  // Show the parent access dialog.
  base::RunLoop show_dialog_run_loop;
  // Show the parent access dialog.
  ParentAccessDialogProvider provider;
  ParentAccessDialogProvider::ShowError error = provider.Show(
      GetParamsForWebApprovals(),
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::unique_ptr<chromeos::ParentAccessDialog::Result> result)
              -> void {
            // The dialog result should contain the test token and expire
            // timestamp.
            EXPECT_EQ("TEST_TOKEN", result->parent_access_token);
            EXPECT_EQ(base::Time::FromDoubleT(123456),
                      result->parent_access_token_expire_timestamp);
            std::move(quit_closure).Run();
          },
          show_dialog_run_loop.QuitClosure()));

  // Verify dialog is showing.
  ASSERT_EQ(error, ParentAccessDialogProvider::ShowError::kNone);

  EXPECT_TRUE(content::WaitForLoadStop(GetContents()));

  ParentAccessUIHandlerImpl* handler = static_cast<ParentAccessUIHandlerImpl*>(
      GetParentAccessUI()->GetHandlerForTest());

  // Make sure the handler isn't null.
  ASSERT_NE(handler, nullptr);

  // Encode the proto in base64.
  std::string encoded_parent_access_callback;
  base::Base64Encode(parent_access_callback.SerializeAsString(),
                     &encoded_parent_access_callback);

  base::RunLoop run_loop;
  handler->OnParentAccessCallbackReceived(
      encoded_parent_access_callback,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             parent_access_ui::mojom::ParentAccessServerMessagePtr message)
              -> void {
            // Verify the Parent Verified callback is parsed.
            EXPECT_EQ(parent_access_ui::mojom::ParentAccessServerMessageType::
                          kParentVerified,
                      message->type);
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  run_loop.Run();

  // Verify the Parent Access Token was stored.
  EXPECT_THAT(*pat, EqualsProto(*(handler->GetParentAccessTokenForTest())));

  // Send the approved result status.
  base::RunLoop parent_approved_run_loop;
  handler->OnParentAccessDone(
      parent_access_ui::mojom::ParentAccessResult::kApproved,
      base::BindOnce([](base::OnceClosure quit_closure)
                         -> void { std::move(quit_closure).Run(); },
                     parent_approved_run_loop.QuitClosure()));

  parent_approved_run_loop.Run();

  // Wait for the "Show Dialog" callback to complete, which wil test for the
  // expected result to be shown.
  show_dialog_run_loop.Run();

  // The dialog should have been closed
  EXPECT_EQ(nullptr, ParentAccessDialog::GetInstance());
}

// Verifies ParentDeclined is handled correctly.
IN_PROC_BROWSER_TEST_F(ParentAccessUIHandlerImplBrowserTest, OnParentDeclined) {
  // Show the parent access dialog.
  base::RunLoop show_dialog_run_loop;
  ParentAccessDialogProvider provider;
  ParentAccessDialogProvider::ShowError error = provider.Show(
      GetParamsForWebApprovals(),
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::unique_ptr<chromeos::ParentAccessDialog::Result> result)
              -> void {
            // The dialog result should contain the test token.
            EXPECT_EQ(chromeos::ParentAccessDialog::Result::kDeclined,
                      result->status);
            std::move(quit_closure).Run();
          },
          show_dialog_run_loop.QuitClosure()));

  // Verify dialog is showing.
  ASSERT_EQ(error, ParentAccessDialogProvider::ShowError::kNone);

  EXPECT_TRUE(content::WaitForLoadStop(GetContents()));

  ParentAccessUIHandlerImpl* handler = static_cast<ParentAccessUIHandlerImpl*>(
      GetParentAccessUI()->GetHandlerForTest());

  // Make sure the handler isn't null.
  ASSERT_NE(handler, nullptr);

  // Send the declined result status.
  base::RunLoop parent_denied_run_loop;
  handler->OnParentAccessDone(
      parent_access_ui::mojom::ParentAccessResult::kDeclined,
      base::BindOnce([](base::OnceClosure quit_closure)
                         -> void { std::move(quit_closure).Run(); },
                     parent_denied_run_loop.QuitClosure()));

  parent_denied_run_loop.Run();

  // Wait for the "Show Dialog" callback to complete, which will test for the
  // expected result to be shown.
  show_dialog_run_loop.Run();

  // The dialog should have been closed
  EXPECT_EQ(nullptr, ParentAccessDialog::GetInstance());
}

// Verifies that the ConsentDeclined status is ignored.
IN_PROC_BROWSER_TEST_F(ParentAccessUIHandlerImplBrowserTest,
                       ConsentDeclinedParsed) {
  // Show the parent access dialog.
  ParentAccessDialogProvider provider;
  ParentAccessDialogProvider::ShowError error =
      provider.Show(GetParamsForWebApprovals(), base::DoNothing());

  // Verify dialog is showing.
  ASSERT_EQ(error, ParentAccessDialogProvider::ShowError::kNone);

  EXPECT_TRUE(content::WaitForLoadStop(GetContents()));

  ParentAccessUIHandlerImpl* handler = static_cast<ParentAccessUIHandlerImpl*>(
      GetParentAccessUI()->GetHandlerForTest());

  // Make sure the handler isn't null.
  ASSERT_NE(handler, nullptr);

  // Construct the ParentAccessCallback
  kids::platform::parentaccess::client::proto::ParentAccessCallback
      parent_access_callback;
  parent_access_callback.mutable_on_consent_declined();

  // Encode the proto in base64.
  std::string encoded_parent_access_callback;
  base::Base64Encode(parent_access_callback.SerializeAsString(),
                     &encoded_parent_access_callback);

  handler->OnParentAccessCallbackReceived(
      encoded_parent_access_callback,
      base::BindOnce(
          [](parent_access_ui::mojom::ParentAccessServerMessagePtr message)
              -> void {
            // Verify that it is ignored.
            EXPECT_EQ(
                parent_access_ui::mojom::ParentAccessServerMessageType::kIgnore,
                message->type);
          }));
}

// Verifies that the OnPageSizeChanged status is ignored.
IN_PROC_BROWSER_TEST_F(ParentAccessUIHandlerImplBrowserTest,
                       OnPageSizeChangedIgnored) {
  // Show the parent access dialog.
  ParentAccessDialogProvider provider;
  ParentAccessDialogProvider::ShowError error =
      provider.Show(GetParamsForWebApprovals(), base::DoNothing());

  // Verify dialog is showing.
  ASSERT_EQ(error, ParentAccessDialogProvider::ShowError::kNone);

  EXPECT_TRUE(content::WaitForLoadStop(GetContents()));

  ParentAccessUIHandlerImpl* handler = static_cast<ParentAccessUIHandlerImpl*>(
      GetParentAccessUI()->GetHandlerForTest());

  // Make sure the handler isn't null.
  ASSERT_NE(handler, nullptr);

  // Construct the ParentAccessCallback
  kids::platform::parentaccess::client::proto::ParentAccessCallback
      parent_access_callback;
  parent_access_callback.mutable_on_page_size_changed();

  // Encode the proto in base64.
  std::string encoded_parent_access_callback;
  base::Base64Encode(parent_access_callback.SerializeAsString(),
                     &encoded_parent_access_callback);

  handler->OnParentAccessCallbackReceived(
      encoded_parent_access_callback,
      base::BindOnce(
          [](parent_access_ui::mojom::ParentAccessServerMessagePtr message)
              -> void {
            // Verify that it is ignored.
            EXPECT_EQ(
                parent_access_ui::mojom::ParentAccessServerMessageType::kIgnore,
                message->type);
          }));
}

// Verifies that the OnCommunicationEstablished status is ignored.
IN_PROC_BROWSER_TEST_F(ParentAccessUIHandlerImplBrowserTest,
                       OnCommunicationEstablishedIgnored) {
  // Show the parent access dialog.
  ParentAccessDialogProvider provider;
  ParentAccessDialogProvider::ShowError error =
      provider.Show(GetParamsForWebApprovals(), base::DoNothing());

  // Verify dialog is showing.
  ASSERT_EQ(error, ParentAccessDialogProvider::ShowError::kNone);
  EXPECT_TRUE(content::WaitForLoadStop(GetContents()));

  ParentAccessUIHandlerImpl* handler = static_cast<ParentAccessUIHandlerImpl*>(
      GetParentAccessUI()->GetHandlerForTest());

  // Make sure the handler isn't null.
  ASSERT_NE(handler, nullptr);

  // Construct the ParentAccessCallback
  kids::platform::parentaccess::client::proto::ParentAccessCallback
      parent_access_callback;
  parent_access_callback.mutable_on_communication_established();

  // Encode the proto in base64.
  std::string encoded_parent_access_callback;
  base::Base64Encode(parent_access_callback.SerializeAsString(),
                     &encoded_parent_access_callback);

  handler->OnParentAccessCallbackReceived(
      encoded_parent_access_callback,
      base::BindOnce(
          [](parent_access_ui::mojom::ParentAccessServerMessagePtr message)
              -> void {
            // Verify that it is ignored.
            EXPECT_EQ(
                parent_access_ui::mojom::ParentAccessServerMessageType::kIgnore,
                message->type);
          }));
}

}  // namespace chromeos
