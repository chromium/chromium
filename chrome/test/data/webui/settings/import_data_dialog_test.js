// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {dashToCamelCase, flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ImportDataBrowserProxyImpl, ImportDataStatus} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
// clang-format on

/** @implements {ImportDataBrowserProxy} */
class TestImportDataBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'initializeImportDialog',
      'importFromBookmarksFile',
      'importData',
    ]);

    /** @private {!Array<!BrowserProfile} */
    this.browserProfiles_ = [];
  }

  /** @param {!Array<!BrowserProfile} browserProfiles */
  setBrowserProfiles(browserProfiles) {
    this.browserProfiles_ = browserProfiles;
  }

  /** @override */
  initializeImportDialog() {
    this.methodCalled('initializeImportDialog');
    return Promise.resolve(this.browserProfiles_.slice());
  }

  /** @override */
  importFromBookmarksFile() {
    this.methodCalled('importFromBookmarksFile');
  }

  /** @override */
  importData(browserProfileIndex, types) {
    this.methodCalled('importData', [browserProfileIndex, types]);
  }
}

suite('ImportDataDialog', function() {
  /** @type {!Array<!BrowserProfile} */
  const browserProfiles = [
    {
      autofillFormData: true,
      favorites: true,
      history: true,
      index: 0,
      name: 'Mozilla Firefox',
      passwords: true,
      search: true
    },
    {
      autofillFormData: true,
      favorites: true,
      history: false,  // Emulate unsupported import option
      index: 1,
      name: 'Mozilla Firefox',
      passwords: true,
      profileName: 'My profile',
      search: true
    },
    {
      autofillFormData: false,
      favorites: true,
      history: false,
      index: 2,
      name: 'Bookmarks HTML File',
      passwords: false,
      search: false
    },
  ];

  function createBooleanPref(name) {
    return {
      key: name,
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    };
  }

  const prefs = {};
  ['import_dialog_history',
   'import_dialog_bookmarks',
   'import_dialog_saved_passwords',
   'import_dialog_search_engine',
   'import_dialog_autofill_form_data',
  ].forEach(function(name) {
    prefs[name] = createBooleanPref(name);
  });

  let dialog = null;

  let browserProxy = null;

  setup(function() {
    browserProxy = new TestImportDataBrowserProxy();
    browserProxy.setBrowserProfiles(browserProfiles);
    ImportDataBrowserProxyImpl.setInstance(browserProxy);
    PolymerTest.clearBody();
    dialog = document.createElement('settings-import-data-dialog');
    dialog.set('prefs', prefs);
    document.body.appendChild(dialog);
    return browserProxy.whenCalled('initializeImportDialog').then(function() {
      assertTrue(dialog.$.dialog.open);
      flush();
    });
  });

  function ensureSettingsCheckboxCheckedStatus(prefName, checked) {
    const settingsCheckbox =
        dialog.$[dashToCamelCase(prefName.replace(/_/g, '-'))];

    if (settingsCheckbox.checked !== checked) {
      // Use click operation to produce a 'change' event.
      settingsCheckbox.$.checkbox.click();
    }
  }

  function simulateBrowserProfileChange(index) {
    dialog.$.browserSelect.selectedIndex = index;
    dialog.$.browserSelect.dispatchEvent(new CustomEvent('change'));
  }

  test('Initialization', function() {
    assertFalse(dialog.$.import.hidden);
    assertFalse(dialog.$.import.disabled);
    assertFalse(dialog.$.cancel.hidden);
    assertFalse(dialog.$.cancel.disabled);
    assertTrue(dialog.$.done.hidden);
    assertTrue(dialog.$.successIcon.parentElement.hidden);

    // Check that the displayed text correctly combines browser name and profile
    // name (if any).
    const expectedText = [
      'Mozilla Firefox',
      'Mozilla Firefox - My profile',
      'Bookmarks HTML File',
    ];

    Array.from(dialog.$.browserSelect.options).forEach((option, i) => {
      assertEquals(expectedText[i], option.textContent.trim());
    });
  });

  test('ImportButton', function() {
    assertFalse(dialog.$.import.disabled);

    // Flip all prefs to false.
    Object.keys(prefs).forEach(function(prefName) {
      ensureSettingsCheckboxCheckedStatus(prefName, false);
    });
    assertTrue(dialog.$.import.disabled);

    // Change browser selection to "Import from Bookmarks HTML file".
    simulateBrowserProfileChange(2);
    assertTrue(dialog.$.import.disabled);

    // Ensure everything except |import_dialog_bookmarks| is ignored.
    ensureSettingsCheckboxCheckedStatus('import_dialog_history', true);
    assertTrue(dialog.$.import.disabled);

    ensureSettingsCheckboxCheckedStatus('import_dialog_bookmarks', true);
    assertFalse(dialog.$.import.disabled);
  });

  function assertInProgressButtons() {
    assertFalse(dialog.$.import.hidden);
    assertTrue(dialog.$.import.disabled);
    assertFalse(dialog.$.cancel.hidden);
    assertTrue(dialog.$.cancel.disabled);
    assertTrue(dialog.$.done.hidden);
    assertTrue(dialog.shadowRoot.querySelector('paper-spinner-lite').active);
    assertFalse(dialog.shadowRoot.querySelector('paper-spinner-lite').hidden);
  }

  function assertSucceededButtons() {
    assertTrue(dialog.$.import.hidden);
    assertTrue(dialog.$.cancel.hidden);
    assertFalse(dialog.$.done.hidden);
    assertFalse(dialog.shadowRoot.querySelector('paper-spinner-lite').active);
    assertTrue(dialog.shadowRoot.querySelector('paper-spinner-lite').hidden);
  }

  /** @param {!ImportDataStatus} status */
  function simulateImportStatusChange(status) {
    webUIListenerCallback('import-data-status-changed', status);
  }

  test('ImportFromBookmarksFile', function() {
    simulateBrowserProfileChange(2);
    dialog.$.import.click();
    browserProxy.whenCalled('importFromBookmarksFile').then(function() {
      simulateImportStatusChange(ImportDataStatus.IN_PROGRESS);
      assertInProgressButtons();

      simulateImportStatusChange(ImportDataStatus.SUCCEEDED);
      assertSucceededButtons();

      assertFalse(dialog.$.successIcon.parentElement.hidden);
      assertFalse(dialog.shadowRoot.querySelector('settings-toggle-button')
                      .parentElement.hidden);
    });
  });

  test('ImportFromBrowserProfile', function() {
    ensureSettingsCheckboxCheckedStatus('import_dialog_bookmarks', false);
    ensureSettingsCheckboxCheckedStatus('import_dialog_search_engine', true);

    const expectedIndex = 0;
    simulateBrowserProfileChange(expectedIndex);
    dialog.$.import.click();

    const importCalled = browserProxy.whenCalled('importData');
    importCalled.then(([actualIndex, types]) => {
      assertEquals(expectedIndex, actualIndex);
      assertFalse(types['import_dialog_bookmarks']);
      assertTrue(types['import_dialog_search_engine']);

      simulateImportStatusChange(ImportDataStatus.IN_PROGRESS);
      assertInProgressButtons();

      simulateImportStatusChange(ImportDataStatus.SUCCEEDED);
      assertSucceededButtons();

      assertFalse(dialog.$.successIcon.parentElement.hidden);
      assertTrue(dialog.shadowRoot.querySelector('settings-toggle-button')
                     .parentElement.hidden);
    });
  });

  test('ImportFromBrowserProfileWithUnsupportedOption', function() {
    // Flip all prefs to true.
    Object.keys(prefs).forEach(function(prefName) {
      ensureSettingsCheckboxCheckedStatus(prefName, true);
    });

    const expectedIndex = 1;
    simulateBrowserProfileChange(expectedIndex);
    dialog.$.import.click();

    const importCalled = browserProxy.whenCalled('importData');
    importCalled.then(([actualIndex, types]) => {
      assertEquals(expectedIndex, actualIndex);

      Object.keys(prefs).forEach(function(prefName) {
        // import_dialog_history is unsupported and hidden
        assertEquals(prefName !== 'import_dialog_history', types[prefName]);
      });
    });
  });

  test('ImportError', function() {
    simulateImportStatusChange(ImportDataStatus.FAILED);
    assertFalse(dialog.$.dialog.open);
  });
});
