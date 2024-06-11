// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/test/browser_test.h"

class PasswordManagerUITest : public WebUIMochaBrowserTest {
 protected:
  PasswordManagerUITest() {
    set_test_loader_host(password_manager::kChromeUIPasswordManagerHost);
  }
};

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, AddPasswordDialog) {
  RunTest("password_manager/add_password_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, App) {
  RunTest("password_manager/password_manager_app_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, CheckupSection) {
  RunTest("password_manager/checkup_section_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, CheckupDetailsSection) {
  RunTest("password_manager/checkup_details_section_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, CredentialField) {
  RunTest("password_manager/credential_field_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, CredentialNote) {
  RunTest("password_manager/credential_note_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, DeletePasskeyDialog) {
  RunTest("password_manager/delete_passkey_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, EditPasskeyDialog) {
  RunTest("password_manager/edit_passkey_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, EditPasswordDialog) {
  RunTest("password_manager/edit_password_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, FullDataReset) {
  RunTest("password_manager/full_data_reset_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, MovePasswordsDialog) {
  RunTest("password_manager/move_passwords_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, PasskeyDetailsCard) {
  RunTest("password_manager/passkey_details_card_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, PasswordDetailsCard) {
  RunTest("password_manager/password_details_card_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, PasswordDetailsSection) {
  RunTest("password_manager/password_details_section_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, PasswordsExporter) {
  RunTest("password_manager/passwords_exporter_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, PasswordsImporter) {
  RunTest("password_manager/passwords_importer_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, PasswordsSection) {
  RunTest("password_manager/passwords_section_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, Routing) {
  RunTest("password_manager/password_manager_routing_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, SettingsSection) {
  RunTest("password_manager/settings_section_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, SharePasswordFlow) {
  RunTest("password_manager/share_password_flow_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, SharePasswordHeader) {
  RunTest("password_manager/share_password_header_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, SharePasswordGroupAvatar) {
  RunTest("password_manager/share_password_group_avatar_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, SharePasswordLoadingDialog) {
  RunTest("password_manager/share_password_loading_dialog_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, SharePasswordConfirmationDialog) {
  RunTest("password_manager/share_password_confirmation_dialog_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, SharePasswordRecipient) {
  RunTest("password_manager/share_password_recipient_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, SharePasswordFamilyPickerDialog) {
  RunTest("password_manager/share_password_family_picker_dialog_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, SideBar) {
  RunTest("password_manager/password_manager_side_bar_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, SiteFavicon) {
  RunTest("password_manager/site_favicon_test.js", "mocha.run()");
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_F(PasswordManagerUITest, PromoCards) {
  RunTest("password_manager/promo_cards_test.js", "mocha.run()");
}
#endif
