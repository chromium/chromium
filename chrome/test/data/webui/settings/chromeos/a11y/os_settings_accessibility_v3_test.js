// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Accessibility Settings tests. */

// Polymer BrowserTest fixture and aXe-core accessibility audit.
GEN_INCLUDE([
  '//chrome/test/data/webui/a11y/accessibility_test.js',
  '//chrome/test/data/webui/polymer_browser_test_base.js',
]);

/**
 * Test fixture for Accessibility of Chrome OS Settings.
 * @constructor
 * @extends {PolymerTest}
 */
function OSSettingsAccessibilityV3Test() {}

// Default accessibility audit options. Specify in test definition to use.
OSSettingsAccessibilityV3Test.axeOptions = {
  'rules': {
    // Disable 'skip-link' check since there are few tab stops before the main
    // content.
    'skip-link': {enabled: false},
    // TODO(crbug.com/761461): enable after addressing flaky tests.
    'color-contrast': {enabled: false},
    // The HTML language attribute isn't set by the test_loader.html dummy file.
    'html-has-lang': {enabled: false},
  },
};

// Default accessibility audit options. Specify in test definition to use.
OSSettingsAccessibilityV3Test.violationFilter = {
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

OSSettingsAccessibilityV3Test.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload:
      'chrome://os-settings/test_loader.html?module=settings/a11y/basic_a11y_v3_test.js',

  // Include files that define the mocha tests.
  extraLibraries: [
    '//third_party/axe-core/axe.js',
  ],

  setUp: function() {
    PolymerTest.prototype.setUp.call(this);

    return new Promise(resolve => {
      document.addEventListener('a11y-setup-complete', resolve);
    });
  },
};
