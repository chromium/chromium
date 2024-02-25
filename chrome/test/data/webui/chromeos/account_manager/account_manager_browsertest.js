// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs UI tests for account manager dialogs. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/chromeos/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

var AccountMigrationWelcomeTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://account-migration-welcome/test_loader.html?module=chromeos/account_manager/account_migration_welcome_test.js';
  }
};

TEST_F('AccountMigrationWelcomeTest', 'CloseDialog', () => {
  this.runMochaTest(
      account_migration_welcome_test.suiteName,
      account_migration_welcome_test.TestNames.CloseDialog);
});

TEST_F('AccountMigrationWelcomeTest', 'MigrateAccount', () => {
  this.runMochaTest(
      account_migration_welcome_test.suiteName,
      account_migration_welcome_test.TestNames.MigrateAccount);
});
