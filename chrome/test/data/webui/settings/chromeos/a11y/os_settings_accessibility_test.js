// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Accessibility Settings tests. */

// Polymer BrowserTest fixture and aXe-core accessibility audit.
GEN_INCLUDE([
  '//chrome/test/data/webui/a11y/accessibility_test.js',
  '//chrome/test/data/webui/polymer_browser_test_base.js',
]);

/**
 * Test fixture for Accessibility of Chrome Settings.
 * @constructor
 * @extends {PolymerTest}
 */
function OSSettingsAccessibilityTest() {}

// Default accessibility audit options. Specify in test definition to use.
OSSettingsAccessibilityTest.axeOptions = {
  'rules': {
    // Disable 'skip-link' check since there are few tab stops before the main
    // content.
    'skip-link': {enabled: false},
    // TODO(crbug.com/761461): enable after addressing flaky tests.
    'color-contrast': {enabled: false},
  }
};

// TODO(crbug.com/1002627): This block prevents generation of a
// link-in-text-block browser-test. This can be removed once the bug is
// addressed, and usage should be replaced with
// OSSettingsAccessibilityTest.axeOptions
OSSettingsAccessibilityTest.axeOptionsExcludeLinkInTextBlock =
    Object.assign({}, OSSettingsAccessibilityTest.axeOptions, {
      'rules': Object.assign({}, OSSettingsAccessibilityTest.axeOptions.rules, {
        'link-in-text-block': {enabled: false},
      })
    });

// Default accessibility audit options. Specify in test definition to use.
OSSettingsAccessibilityTest.violationFilter = {
  'aria-valid-attr': function(nodeResult) {
    const attributeAllowlist = [
      'aria-active-attribute',  // Polymer components use aria-active-attribute.
      'aria-roledescription',   // This attribute is now widely supported.
    ];

    return attributeAllowlist.some(a => nodeResult.element.hasAttribute(a));
  },
  'aria-allowed-attr': function(nodeResult) {
    const attributeAllowlist = [
      'aria-roledescription',  // This attribute is now widely supported.
    ];
    return attributeAllowlist.some(a => nodeResult.element.hasAttribute(a));
  },
  'button-name': function(nodeResult) {
    if (nodeResult.element.classList.contains('icon-expand-more')) {
      return true;
    }

    // Ignore the <button> residing within cr-toggle and cr-checkbox, which has
    // tabindex -1 anyway.
    const parentNode = nodeResult.element.parentNode;
    return parentNode && parentNode.host &&
        (parentNode.host.tagName === 'CR-TOGGLE' ||
         parentNode.host.tagName === 'CR-CHECKBOX');
  },
};

OSSettingsAccessibilityTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload: 'chrome://os-settings/',
};
