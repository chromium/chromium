// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Define accessibility tests for the EDIT_DICTIONARY route.  */

// Disable since the EDIT_DICTIONARY route does not exist on Mac.
// TODO(crbug.com/1012370) flaky on Linux b/c assertTrue(!!languagesPage);
GEN('#if !defined(OS_MACOSX) && !defined(OS_LINUX)');

// SettingsAccessibilityTest fixture.
GEN_INCLUDE([
  'settings_accessibility_test.js',
]);

// eslint-disable-next-line no-var
var EditDictionaryA11yTest = class extends SettingsAccessibilityTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../../fake_chrome_event.js',
      '../../test_browser_proxy.js',
      '../../test_util.js',
      '../fake_language_settings_private.js',
      '../fake_settings_private.js',
    ]);
  }
};

function getDictionaryPage() {
  const settingsUI = document.querySelector('settings-ui');
  assertTrue(!!settingsUI);
  const settingsMain = settingsUI.$.main;
  assertTrue(!!settingsMain);
  const settingsBasicPage = settingsMain.$$('settings-basic-page');
  assertTrue(!!settingsBasicPage);
  const languagesPage = settingsBasicPage.$$('settings-languages-page');
  assertTrue(!!languagesPage);
  const dictionaryPage = languagesPage.$$('settings-edit-dictionary-page');
  assertTrue(!!dictionaryPage);
  return dictionaryPage;
}

AccessibilityTest.define('EditDictionaryA11yTest', {
  /** @override */
  name: 'EDIT_DICTIONARY',

  /** @override */
  axeOptions: Object.assign({}, SettingsAccessibilityTest.axeOptions, {
    'rules': Object.assign({}, SettingsAccessibilityTest.axeOptions.rules, {
      // TODO(crbug.com/1012370): Disable because of timeout in CFI build.
      'hidden-content': {enabled: false},
    }),
  }),

  /** @type {settings.FakeLanguageSettingsPrivate} */
  languageSettingsPrivate_: null,

  /** @override */
  violationFilter:
      Object.assign({}, SettingsAccessibilityTest.violationFilter, {
        // Excuse custom input elements.
        'aria-valid-attr-value': function(nodeResult) {
          const describerId =
              nodeResult.element.getAttribute('aria-describedby');
          return describerId === '' && nodeResult.element.tagName == 'INPUT';
        },
        'tabindex': function(nodeResult) {
          // TODO(crbug.com/808276): remove this exception when bug is fixed.
          return nodeResult.element.getAttribute('tabindex') == '0';
        },
      }),

  /** @override */
  setup: async function() {
    // Don't replace b/c each test case needs to use the same fake.
    if (!this.languageSettingsPrivate_) {
      this.languageSettingsPrivate_ =
          new settings.FakeLanguageSettingsPrivate(),
      settings.languageSettingsPrivateApiForTest =
          this.languageSettingsPrivate_;
    }

    settings.navigateTo(settings.routes.EDIT_DICTIONARY);
    Polymer.dom.flush();
    await test_util.flushTasks();
  },

  /** @override */
  tests: {
    'Accessible with No Changes': function() {
      const dictionaryPage = getDictionaryPage();
      assertFalse(!!dictionaryPage.$$('#list'));
    },

    'Load data to list': function() {
      const dictionaryPage = getDictionaryPage();

      this.languageSettingsPrivate_.addSpellcheckWord('one');

      assertTrue(!!dictionaryPage.$$('#list'));
      assertEquals(1, dictionaryPage.$$('#list').items.length);
    },
  },
});

GEN('#endif  // !defined(OS_MACOSX) && !defined(OS_LINUX)');
