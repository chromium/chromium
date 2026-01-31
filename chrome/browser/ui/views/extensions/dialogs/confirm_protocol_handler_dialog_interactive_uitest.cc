// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/custom_handlers/protocol_handler.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "ui/views/window/dialog_client_view.h"

class ConfirmProtocolHandlerDialogUITest : public InteractiveBrowserTest {
 public:
  ConfirmProtocolHandlerDialogUITest() = default;
  ~ConfirmProtocolHandlerDialogUITest() override = default;
  ConfirmProtocolHandlerDialogUITest(
      const ConfirmProtocolHandlerDialogUITest&) = delete;
  ConfirmProtocolHandlerDialogUITest& operator=(
      const ConfirmProtocolHandlerDialogUITest&) = delete;

  auto ShowConfirmProtocolHandlerDialog(
      const custom_handlers::ProtocolHandler& handler,
      const std::optional<url::Origin>& initiating_origin) {
    return Do([&]() {
      extensions::ShowConfirmProtocolHandlerDialog(
          browser()->tab_strip_model()->GetActiveWebContents(), handler,
          initiating_origin,
          base::BindOnce(
              &ConfirmProtocolHandlerDialogUITest::OnPermissionGranted,
              base::Unretained(this)),
          base::BindOnce(
              &ConfirmProtocolHandlerDialogUITest::OnPermissionDenied,
              base::Unretained(this)));
    });
  }

  void OnPermissionGranted(bool remember) {
    granted_ = true;
    remember_ = remember;
  }

  void OnPermissionDenied() {
    granted_ = false;
    remember_ = false;
  }

  custom_handlers::ProtocolHandler CreateUnconfirmedProtocolHandler(
      const std::string& protocol,
      const GURL& url,
      const extensions::ExtensionId& extension_id) {
    // Only handlers with extension id can be unconfirmed for now.
    return custom_handlers::ProtocolHandler::CreateExtensionProtocolHandler(
        protocol, url, extension_id);
  }

  extensions::ExtensionRegistrar* extension_registrar() {
    return extensions::ExtensionRegistrar::Get(browser()->profile());
  }

  // Installs programmatically (not through the UI) an extension for the given
  // user.
  void InstallExtension() {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder("An Extension").Build();

    extension_registrar()->AddExtension(extension);
    last_loaded_extension_id_ = extension->id();
  }

  extensions::ExtensionId last_loaded_extension_id() {
    return last_loaded_extension_id_;
  }

  bool granted() { return granted_; }
  bool remember() { return remember_; }

 private:
  extensions::ExtensionId last_loaded_extension_id_;
  bool granted_;
  bool remember_;
};

// Just checks that the dialog is shown properly.
IN_PROC_BROWSER_TEST_F(ConfirmProtocolHandlerDialogUITest, ShowDialog) {
  InstallExtension();
  custom_handlers::ProtocolHandler handler = CreateUnconfirmedProtocolHandler(
      "web+burger", GURL("https://test.com/url=%s"),
      last_loaded_extension_id());
  RunTestSequence(
      ShowConfirmProtocolHandlerDialog(handler, url::Origin()),
      WaitForShow(extensions::kConfirmProtocolHandlerDialogHandlerRedirection));
}

// Pressing the 'Allow' button invokes the OnPermissionGranted callback, then
// closes the dialog.
IN_PROC_BROWSER_TEST_F(ConfirmProtocolHandlerDialogUITest, AllowButtonPressed) {
  InstallExtension();
  custom_handlers::ProtocolHandler handler = CreateUnconfirmedProtocolHandler(
      "web+burger", GURL("https://test.com/url=%s"),
      last_loaded_extension_id());
  RunTestSequence(
      ShowConfirmProtocolHandlerDialog(handler, url::Origin()),
      WaitForShow(extensions::kConfirmProtocolHandlerDialogHandlerRedirection),

      // Clicking the Allow button closes the dialog.
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForHide(extensions::kConfirmProtocolHandlerDialogHandlerRedirection));
  ASSERT_TRUE(granted());
  ASSERT_FALSE(remember());
}

// Pressing the 'Allow' button, with the 'RememberMe' checkbox checked,
// invokes the OnPermissionGranted then closes the dialog.
IN_PROC_BROWSER_TEST_F(ConfirmProtocolHandlerDialogUITest,
                       AllowButtonPressedWithRememberChecked) {
  InstallExtension();
  custom_handlers::ProtocolHandler handler = CreateUnconfirmedProtocolHandler(
      "web+burger", GURL("https://test.com/url=%s"),
      last_loaded_extension_id());
  RunTestSequence(
      ShowConfirmProtocolHandlerDialog(handler, url::Origin()),
      WaitForShow(extensions::kConfirmProtocolHandlerDialogHandlerRedirection),
      PressButton(extensions::kConfirmProtocolHandlerDialogRememberMeCheckbox),

      // Clicking the Allow button closes the dialog.
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForHide(extensions::kConfirmProtocolHandlerDialogHandlerRedirection));
  ASSERT_TRUE(granted());
  ASSERT_TRUE(remember());
}

// Pressing the 'Deny' button invokes the OnPermissionDenied callback then
// closes the dialog.
IN_PROC_BROWSER_TEST_F(ConfirmProtocolHandlerDialogUITest, DenyButtonPressed) {
  InstallExtension();
  custom_handlers::ProtocolHandler handler = CreateUnconfirmedProtocolHandler(
      "web+burger", GURL("https://test.com/url=%s"),
      last_loaded_extension_id());
  RunTestSequence(
      ShowConfirmProtocolHandlerDialog(handler, url::Origin()),
      WaitForShow(extensions::kConfirmProtocolHandlerDialogHandlerRedirection),

      // Clicking the Deny button closes the dialog.
      PressButton(views::DialogClientView::kCancelButtonElementId),
      WaitForHide(extensions::kConfirmProtocolHandlerDialogHandlerRedirection));
  ASSERT_FALSE(granted());
  ASSERT_FALSE(remember());
}
