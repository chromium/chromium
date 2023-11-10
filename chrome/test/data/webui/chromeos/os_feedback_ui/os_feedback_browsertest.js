// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test fixture for chrome://os-feedback.
 * Testing suite for feedback tool.
 *
 * To run all tests:
 * `browser_tests --gtest_filter=OSFeedbackBrowserTest*``
 *
 */

GEN_INCLUDE(['//chrome/test/data/webui/chromeos/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "chromeos/constants/chromeos_features.h"');

this.OSFeedbackBrowserTest = class extends PolymerTest {
  /** @override */
  get featureList() {
    return {
      enabled: ['ash::features::kOsFeedbackJelly', 'chromeos::features::kJelly']
    };
  }
};

// Test suites for OS Feedback. To disable a test suite add 'DISABLED_All' as
// the case name.
// Ex. ['ConfirmationPage', 'confirmation_page_test.js', 'DISABLED_All']
// TODO(crbug.com/1401615): Flaky.
const tests = [
  ['ConfirmationPage', 'confirmation_page_test.js', 'DISABLED_All'],
  [
    'FakeHelpContentProvider', 'fake_help_content_provider_test.js',
    'DISABLED_All'
  ],
  ['MojoInterfaceProvider', 'mojo_interface_provider_test.js', 'DISABLED_All'],
  ['FeedbackFlow', 'feedback_flow_test.js', 'DISABLED_All'],
  ['FileAttachment', 'file_attachment_test.js', 'DISABLED_All'],
  ['HelpContent', 'help_content_test.js', 'DISABLED_All'],
  ['SearchPage', 'search_page_test.js', 'DISABLED_All'],
  ['ShareDataPage', 'share_data_page_test.js', 'DISABLED_All'],
];

for (const [suiteName, module, caseName] of tests) {
  const className = `OSFeedbackBrowserTest_${suiteName}`;
  this[className] = class extends OSFeedbackBrowserTest {
    /** @override */
    get browsePreload() {
      return 'chrome://os-feedback/test_loader.html?module=' +
          `chromeos/os_feedback_ui/${module}`;
    }
  }

  TEST_F(className, caseName || 'All', () => mocha.run());
}
