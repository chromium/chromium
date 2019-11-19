// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_test.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/passwords_client_ui_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/stub_form_saver.h"
#include "content/public/test/test_utils.h"

using password_manager::PasswordFormManager;

namespace {
constexpr char kTestOrigin[] = "https://www.example.com";
}  // namespace

ManagePasswordsTest::ManagePasswordsTest() {
  fetcher_.Fetch();

  password_form_.signon_realm = kTestOrigin;
  password_form_.origin = GURL(kTestOrigin);
  password_form_.username_value = base::ASCIIToUTF16("test_username");
  password_form_.password_value = base::ASCIIToUTF16("test_password");

  federated_form_.signon_realm =
      "federation://example.com/somelongeroriginurl.com";
  federated_form_.origin = GURL(kTestOrigin);
  federated_form_.federation_origin =
      url::Origin::Create(GURL("https://somelongeroriginurl.com/"));
  federated_form_.username_value =
      base::ASCIIToUTF16("test_federation_username");

  // Create a simple sign-in form.
  observed_form_.url = password_form_.origin;
  autofill::FormFieldData field;
  field.form_control_type = "text";
  observed_form_.fields.push_back(field);
  field.form_control_type = "password";
  observed_form_.fields.push_back(field);

  // Turn off waiting for server predictions in order to avoid dealing with
  // posted tasks in PasswordFormManager.
  PasswordFormManager::set_wait_for_server_predictions_for_filling(false);
}

ManagePasswordsTest::~ManagePasswordsTest() = default;

void ManagePasswordsTest::SetUpOnMainThread() {
  AddTabAtIndex(0, GURL(kTestOrigin), ui::PAGE_TRANSITION_TYPED);
}

void ManagePasswordsTest::ExecuteManagePasswordsCommand() {
  // Show the window to ensure that it's active.
  browser()->window()->Show();

  CommandUpdater* updater = browser()->command_controller();
  EXPECT_TRUE(updater->IsCommandEnabled(IDC_MANAGE_PASSWORDS_FOR_PAGE));
  EXPECT_TRUE(updater->ExecuteCommand(IDC_MANAGE_PASSWORDS_FOR_PAGE));

  // Wait for the command execution to pop up the bubble.
  content::RunAllPendingInMessageLoop();
}

void ManagePasswordsTest::SetupManagingPasswords() {
  std::vector<const autofill::PasswordForm*> forms;
  for (auto* form : {&password_form_, &federated_form_}) {
    forms.push_back(form);
    GetController()->OnPasswordAutofilled(forms, form->origin, nullptr);
  }
}

void ManagePasswordsTest::SetupPendingPassword() {
  auto form_manager = std::make_unique<PasswordFormManager>(
      &client_, driver_.AsWeakPtr(), observed_form_, &fetcher_,
      base::WrapUnique(new password_manager::StubFormSaver),
      nullptr /*  metrics_recorder */);
  fetcher_.NotifyFetchCompleted();
  GetController()->OnPasswordSubmitted(std::move(form_manager));
}

void ManagePasswordsTest::SetupAutomaticPassword() {
  auto form_manager = std::make_unique<PasswordFormManager>(
      &client_, driver_.AsWeakPtr(), observed_form_, &fetcher_,
      base::WrapUnique(new password_manager::StubFormSaver),
      nullptr /*  metrics_recorder */);
  fetcher_.NotifyFetchCompleted();
  GetController()->OnAutomaticPasswordSave(std::move(form_manager));
}

void ManagePasswordsTest::SetupAutoSignin(
    std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials) {
  ASSERT_FALSE(local_credentials.empty());
  GURL origin = local_credentials[0]->origin;
  GetController()->OnAutoSignin(std::move(local_credentials), origin);
}

std::unique_ptr<base::HistogramSamples> ManagePasswordsTest::GetSamples(
    const char* histogram) {
  // Ensure that everything has been properly recorded before pulling samples.
  content::RunAllPendingInMessageLoop();
  return histogram_tester_.GetHistogramSamplesSinceCreation(histogram);
}

PasswordsClientUIDelegate* ManagePasswordsTest::GetController() {
  return PasswordsClientUIDelegateFromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
}
