// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the Material Design history page.
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "build/build_config.h"')
GEN('#include "build/branding_buildflags.h"');
GEN('#include "components/password_manager/core/common/password_manager_features.h"');
GEN('#include "content/public/test/browser_test.h"');

const PasswordManagerBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://password-manager/';
  }

  /** @override */
  get featureList() {
    return {enabled: ['password_manager::features::kPasswordManagerRedesign']};
  }
};

[['AddPassword', 'add_password_dialog_test.js'],
 ['App', 'password_manager_app_test.js'],
 ['Checkup', 'checkup_section_test.js'],
 ['CheckupDetails', 'checkup_details_section_test.js'],
 ['EditPassword', 'edit_password_dialog_test.js'],
 ['MovePasswordsDialog', 'move_passwords_dialog_test.js'],
 ['PasswordCard', 'password_details_card_test.js'],
 ['PasswordDetails', 'password_details_section_test.js'],
 ['PasswordsExporter', 'passwords_exporter_test.js'],
 ['PasswordsImporter', 'passwords_importer_test.js'],
 ['PasswordsSection', 'passwords_section_test.js'],
 ['Routing', 'password_manager_routing_test.js'],
 ['Settings', 'settings_section_test.js'],
 ['SideBar', 'password_manager_side_bar_test.js'],
 ['SiteFavicon', 'site_favicon_test.js'],
].forEach(test => registerTest(...test));

GEN('#if BUILDFLAG(GOOGLE_CHROME_BRANDING)');
registerTest('PromoCards', 'promo_cards_test.js');
GEN('#endif');

function registerTest(testName, module, caseName) {
  const className = `PasswordManagerUI${testName}Test`;
  this[className] = class extends PasswordManagerBrowserTest {
    /** @override */
    get browsePreload() {
      return `chrome://password-manager/test_loader.html?module=password_manager/${
          module}`;
    }
  };

  TEST_F(className, caseName || 'All', () => mocha.run());
}
