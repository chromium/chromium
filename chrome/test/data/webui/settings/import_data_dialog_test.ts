// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {dashToCamelCase, flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {BrowserProfile, ImportDataBrowserProxy, SettingsCheckboxElement, SettingsImportDataDialogElement} from 'chrome://settings/lazy_load.js';
import {ImportDataBrowserProxyImpl, ImportDataStatus} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

// clang-format on

class TestImportDataBrowserProxy extends TestBrowserProxy implements
    ImportDataBrowserProxy {
  private browserProfiles_: BrowserProfile[] = [];

  constructor() {
    super([
      'initializeImportDialog',
      'importFromBookmarksFile',
      'importData',
    ]);
  }

  setBrowserProfiles(browserProfiles: BrowserProfile[]) {
    this.browserProfiles_ = browserProfiles;
  }

  initializeImportDialog() {
    this.methodCalled('initializeImportDialog');
    return Promise.resolve(this.browserProfiles_.slice());
  }

  importFromBookmarksFile() {
    this.methodCalled('importFromBookmarksFile');
  }

  importData(browserProfileIndex: number, types: {[type: string]: boolean}) {
    this.methodCalled('importData', [browserProfileIndex, types]);
  }
}

suite('ImportDataDialog', function() {
  const browserProfiles: BrowserProfile[] = [
    {
      autofillFormData: true,
      favorites: true,
      history: true,
      index: 0,
      name: 'Mozilla Firefox',
      passwords: true,
      profileName: '',
      search: true,
    },
    {
      autofillFormData: true,
      favorites: true,
      history: false,  // Emulate unsupported import option
      index: 1,
      name: 'Mozilla Firefox',
      passwords: true,
      profileName: 'My profile',
      search: true,
    },
    {
      autofillFormData: false,
      favorites: true,
      history: false,
      index: 2,
      name: 'Bookmarks HTML File',
      passwords: false,
      profileName: '',
      search: false,
    },
  ];

  function createBooleanPref(name: string): chrome.settingsPrivate.PrefObject {
    return {
      key: name,
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    };
  }

  const prefs: {[key: string]: chrome.settingsPrivate.PrefObject} = {};
  ['import_dialog_history',
   'import_dialog_bookmarks',
   'import_dialog_saved_passwords',
   'import_dialog_search_engine',
   'import_dialog_autofill_form_data',
  ].forEach(function(name) {
    prefs[name] = createBooleanPref(name);
  });

  let dialog: SettingsImportDataDialogElement;
  let browserProxy: TestImportDataBrowserProxy;

  setup(async function() {
    browserProxy = new TestImportDataBrowserProxy();
    browserProxy.setBrowserProfiles(browserProfiles);
    ImportDataBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('settings-import-data-dialog');
    dialog.set('prefs', prefs);
    document.body.appendChild(dialog);
    await browserProxy.whenCalled('initializeImportDialog');
    assertTrue(dialog.$.dialog.open);
    flush();
  });

  async function ensureSettingsCheckboxCheckedStatus(
      prefName: string, checked: boolean) {
    const id = dashToCamelCase(prefName.replace(/_/g, '-'));
    const settingsCheckbox =
        dialog.shadowRoot!.querySelector<SettingsCheckboxElement>(`#${id}`)!;

    if (settingsCheckbox.checked !== checked) {
      // Use click operation to produce a 'change' event.
      settingsCheckbox.$.checkbox.click();
      await settingsCheckbox.$.checkbox.updateComplete;
    }
  }

  function simulateBrowserProfileChange(index: number) {
    dialog.$.browserSelect.selectedIndex = index;
    dialog.$.browserSelect.dispatchEvent(new CustomEvent('change'));
  }

  test('Initialization', function() {
    assertFalse(dialog.$.import.hidden);
    assertFalse(dialog.$.import.disabled);
    assertFalse(dialog.$.cancel.hidden);
    assertFalse(dialog.$.cancel.disabled);
    assertTrue(dialog.$.done.hidden);
    assertTrue(dialog.$.successIcon.parentElement!.hidden);

    // Check that the displayed text correctly combines browser name and profile
    // name (if any).
    const expectedText = [
      'Mozilla Firefox',
      'Mozilla Firefox - My profile',
      'Bookmarks HTML File',
    ];

    Array.from(dialog.$.browserSelect.options).forEach((option, i) => {
      assertEquals(expectedText[i], option.textContent!.trim());
    });
  });

  test('ImportButton', async function() {
    assertFalse(dialog.$.import.disabled);

    // Flip all prefs to false.
    for (const key of Object.keys(prefs)) {
      await ensureSettingsCheckboxCheckedStatus(key, false);
    }
    assertTrue(dialog.$.import.disabled);

    // Change browser selection to "Import from Bookmarks HTML file".
    simulateBrowserProfileChange(2);
    assertTrue(dialog.$.import.disabled);

    // Ensure everything except |import_dialog_bookmarks| is ignored.
    await ensureSettingsCheckboxCheckedStatus('import_dialog_history', true);
    assertTrue(dialog.$.import.disabled);

    await ensureSettingsCheckboxCheckedStatus('import_dialog_bookmarks', true);
    assertFalse(dialog.$.import.disabled);
  });

  function assertInProgressButtons() {
    assertFalse(dialog.$.import.hidden);
    assertTrue(dialog.$.import.disabled);
    assertFalse(dialog.$.cancel.hidden);
    assertTrue(dialog.$.cancel.disabled);
    assertTrue(dialog.$.done.hidden);
    const spinner = dialog.shadowRoot!.querySelector('paper-spinner-lite')!;
    assertTrue(spinner.active);
    assertFalse(spinner.hidden);
  }

  function assertSucceededButtons() {
    assertTrue(dialog.$.import.hidden);
    assertTrue(dialog.$.cancel.hidden);
    assertFalse(dialog.$.done.hidden);
    const spinner = dialog.shadowRoot!.querySelector('paper-spinner-lite')!;
    assertFalse(spinner.active);
    assertTrue(spinner.hidden);
  }

  function simulateImportStatusChange(status: ImportDataStatus) {
    webUIListenerCallback('import-data-status-changed', status);
  }

  test('ImportFromBookmarksFile', async function() {
    simulateBrowserProfileChange(2);
    dialog.$.import.click();
    await browserProxy.whenCalled('importFromBookmarksFile');
    simulateImportStatusChange(ImportDataStatus.IN_PROGRESS);
    assertInProgressButtons();

    simulateImportStatusChange(ImportDataStatus.SUCCEEDED);
    assertSucceededButtons();

    assertFalse(dialog.$.successIcon.parentElement!.hidden);
    assertFalse(
        dialog.shadowRoot!.querySelector(
                              'settings-toggle-button')!.parentElement!.hidden);
  });

  test('ImportFromBrowserProfile', async function() {
    await ensureSettingsCheckboxCheckedStatus('import_dialog_bookmarks', false);
    await ensureSettingsCheckboxCheckedStatus(
        'import_dialog_search_engine', true);

    const expectedIndex = 0;
    simulateBrowserProfileChange(expectedIndex);
    dialog.$.import.click();

    const [actualIndex, types] = await browserProxy.whenCalled('importData');

    assertEquals(expectedIndex, actualIndex);
    assertFalse(types['import_dialog_bookmarks']);
    assertTrue(types['import_dialog_search_engine']);

    simulateImportStatusChange(ImportDataStatus.IN_PROGRESS);
    assertInProgressButtons();

    simulateImportStatusChange(ImportDataStatus.SUCCEEDED);
    assertSucceededButtons();

    assertFalse(dialog.$.successIcon.parentElement!.hidden);
    assertTrue(
        dialog.shadowRoot!.querySelector(
                              'settings-toggle-button')!.parentElement!.hidden);
  });

  test('ImportFromBrowserProfileWithUnsupportedOption', async function() {
    // Flip all prefs to true.
    for (const key of Object.keys(prefs)) {
      await ensureSettingsCheckboxCheckedStatus(key, true);
    }

    const expectedIndex = 1;
    simulateBrowserProfileChange(expectedIndex);
    dialog.$.import.click();

    const [actualIndex, types] = await browserProxy.whenCalled('importData');
    assertEquals(expectedIndex, actualIndex);

    Object.keys(prefs).forEach(function(prefName) {
      // import_dialog_history is unsupported and hidden
      assertEquals(prefName !== 'import_dialog_history', types[prefName]);
    });
  });

  test('ImportError', function() {
    simulateImportStatusChange(ImportDataStatus.FAILED);
    assertFalse(dialog.$.dialog.open);
  });
});
