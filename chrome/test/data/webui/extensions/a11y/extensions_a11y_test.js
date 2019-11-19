// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Polymer BrowserTest fixture and aXe-core accessibility audit.
GEN_INCLUDE([
  '//chrome/test/data/webui/a11y/accessibility_test.js',
  '//chrome/test/data/webui/polymer_browser_test_base.js',
]);
GEN('#include "chrome/browser/ui/webui/extensions/' +
    'extension_settings_browsertest.h"');

/**
 * Test fixture for Accessibility of Chrome Extensions.
 * @constructor
 * @extends {PolymerTest}
 */
// eslint-disable-next-line no-var
var CrExtensionsA11yTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/';
  }

  /** @override */
  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }

  // Default accessibility audit options. Specify in test definition to use.
  static get axeOptions() {
    return {
      'rules': {
        // Disable 'skip-link' check since there are few tab stops before the
        // main content.
        'skip-link': {enabled: false},
        // TODO(crbug.com/761461): enable after addressing flaky tests.
        'color-contrast': {enabled: false},
      },
    };
  }

  // Default accessibility violation filter. Specify in test definition to use.
  static get violationFilter() {
    return {
      // Different iron-iconset-svg can have children with the same id.
      'duplicate-id': function(nodeResult) {
        // Only safe to ignore if ALL dupe ids are children of iron-iconset-svg.
        return nodeResult.any.every(hit => {
          return hit.relatedNodes.every(node => {
            return CrExtensionsA11yTest.hasAncestor_(
                node.element, 'iron-iconset-svg');
          });
        });
      },
      'button-name': function(nodeResult) {
        const parentNode = nodeResult.element.parentNode;

        // Ignore the <button> residing within cr-toggle, which has tabindex -1
        // anyway.
        return parentNode && parentNode.host &&
            parentNode.host.tagName == 'CR-TOGGLE';
      },

      // TODO(crbug.com/1002620): this filter can be removed after
      // addressing the bug
      'heading-order': function(nodeResult) {
        // Filter out 'Heading levels do not increase by one' error when
        // enumerating extensions
        const expectedMarkup = '<div id="name" role="heading" aria-level="3" \
class="clippable-flex-text">My extension 1</div>';
        return nodeResult['html'] === expectedMarkup;
      },
    };
  }

  /** @override */
  get typedefCppFixture() {
    return 'ExtensionSettingsUIBrowserTest';
  }

  /**
   * @param {!HTMLNode} node
   * @param {string} type
   * @return {boolean} Whether any ancestor of |node| is a |type| element.
   * @private
   */
  static hasAncestor_(node, type) {
    if (!node.parentElement) {
      return false;
    }

    return (node.parentElement.tagName.toLocaleLowerCase() == type) ||
        CrExtensionsA11yTest.hasAncestor_(node.parentElement, type);
  }
};

AccessibilityTest.define('CrExtensionsA11yTest', {
  /** @override */
  name: 'NoExtensions',

  /** @override */
  // TODO(crbug.com/1002627): when bug is addressed, this should be replaced
  // with axeOptions: CrExtensionsA11yTest.axeOptions,
  axeOptions: Object.assign({}, CrExtensionsA11yTest.axeOptions, {
    'rules': Object.assign({}, CrExtensionsA11yTest.axeOptions.rules, {
      'link-in-text-block': {enabled: false},
    })
  }),

  /** @override */
  violationFilter: CrExtensionsA11yTest.violationFilter,

  /** @override */
  tests: {
    'Accessible with No Extensions': function() {
      let list = document.querySelector('extensions-manager').$$('#items-list');
      assertEquals(list.extensions.length, 0);
      assertEquals(list.apps.length, 0);
    }
  },
});

CrExtensionsA11yTestWithMultipleExensions = class extends CrExtensionsA11yTest {
  /** @override */
  testGenPreamble() {
    GEN('  InstallGoodExtension();');
    GEN('  InstallPackagedApp();');
    GEN('  InstallHostedApp();');
    GEN('  InstallPlatformApp();');
  }
};

AccessibilityTest.define('CrExtensionsA11yTestWithMultipleExensions', {
  /** @override */
  name: 'WithExtensions',

  /** @override */
  axeOptions: CrExtensionsA11yTest.axeOptions,

  /** @override */
  violationFilter: CrExtensionsA11yTest.violationFilter,

  /** @override */
  tests: {
    'Accessible with Extensions and Apps': function() {
      let list = document.querySelector('extensions-manager').$$('#items-list');
      assertEquals(list.extensions.length, 1);
      assertEquals(list.apps.length, 3);
    },
  },
});

CrExtensionsShortcutA11yTestWithNoExtensions =
    class extends CrExtensionsA11yTest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/shortcuts';
  }
};

AccessibilityTest.define('CrExtensionsShortcutA11yTestWithNoExtensions', {
  /** @override */
  name: 'ShortcutsWithNoExtensions',

  /** @override */
  axeOptions: CrExtensionsA11yTest.axeOptions,

  /** @override */
  violationFilter: CrExtensionsA11yTest.violationFilter,

  /** @override */
  tests: {
    'Accessible with No Extensions or Apps': function() {
      let list = document.querySelector('extensions-manager')
                     .$$('extensions-keyboard-shortcuts');
      assertEquals(list.items.length, 0);
    },
  },
});

CrExtensionsShortcutA11yTestWithExtensions =
    class extends CrExtensionsShortcutA11yTestWithNoExtensions {
  /** @override */
  testGenPreamble() {
    GEN('  InstallGoodExtension();');
  }
};

AccessibilityTest.define('CrExtensionsShortcutA11yTestWithExtensions', {
  /** @override */
  name: 'ShortcutsWithExtensions',

  /** @override */
  axeOptions: CrExtensionsA11yTest.axeOptions,

  /** @override */
  violationFilter: CrExtensionsA11yTest.violationFilter,

  /** @override */
  tests: {
    'Accessible with Extensions': function() {
      let list = document.querySelector('extensions-manager')
                     .$$('extensions-keyboard-shortcuts');
      assertEquals(list.items.length, 1);
    },
  },
});

CrExtensionsErrorConsoleA11yTest =
    class extends CrExtensionsShortcutA11yTestWithNoExtensions {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/?errors=pdlpifnclfacjobnmbpngemkalkjamnf';
  }

  /** @override */
  testGenPreamble() {
    GEN('  SetDevModeEnabled(true);');
    GEN('  EnableErrorConsole();');
    GEN('  InstallErrorsExtension();');
  }

  /** @override */
  testGenPostamble() {
    GEN('  SetDevModeEnabled(false);');  // Return this to default.
  }
};

AccessibilityTest.define('CrExtensionsErrorConsoleA11yTest', {
  /** @override */
  name: 'ErrorConsole',

  /** @override */
  axeOptions: CrExtensionsA11yTest.axeOptions,

  /** @override */
  violationFilter: CrExtensionsA11yTest.violationFilter,

  /** @override */
  tests: {
    'Accessible Error Console': function() {
      assertTrue(!!document.querySelector('extensions-manager')
                       .$$('extensions-error-page')
                       .$$('#errorsList'));
    },
  },
});
