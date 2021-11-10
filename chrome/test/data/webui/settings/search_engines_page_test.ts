// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrInputElement, SettingsOmniboxExtensionEntryElement, SettingsSearchEngineDialogElement, SettingsSearchEngineEntryElement, SettingsSearchEnginesPageElement} from 'chrome://settings/lazy_load.js';
import {ExtensionControlBrowserProxyImpl, loadTimeData, SearchEngine, SearchEnginesBrowserProxyImpl, SearchEnginesInfo, SearchEnginesInteractions} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestExtensionControlBrowserProxy} from './test_extension_control_browser_proxy.js';
import {createSampleSearchEngine, TestSearchEnginesBrowserProxy} from './test_search_engines_browser_proxy.js';

// clang-format on

function createSampleOmniboxExtension(): SearchEngine {
  return {
    canBeDefault: false,
    canBeEdited: false,
    canBeRemoved: false,
    canBeActivated: false,
    canBeDeactivated: false,
    default: false,
    displayName: 'Omnibox extension displayName',
    extension: {
      icon: 'chrome://extension-icon/some-extension-icon',
      id: 'dummyextensionid',
      name: 'Omnibox extension',
      canBeDisabled: false,
    },
    id: 0,
    isOmniboxExtension: true,
    keyword: 'oe',
    modelIndex: 6,
    name: 'Omnibox extension',
    url: 'chrome-extension://dummyextensionid/?q=%s',
    urlLocked: false
  };
}

suite('AddSearchEngineDialogTests', function() {
  let dialog: SettingsSearchEngineDialogElement;
  let browserProxy: TestSearchEnginesBrowserProxy;

  setup(function() {
    browserProxy = new TestSearchEnginesBrowserProxy();
    SearchEnginesBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = '';
    dialog = document.createElement('settings-search-engine-dialog');
    document.body.appendChild(dialog);
  });

  teardown(function() {
    dialog.remove();
  });

  // Tests that the dialog calls 'searchEngineEditStarted' and
  // 'searchEngineEditCancelled' when closed from the 'cancel' button.
  test('DialogOpenAndCancel', function() {
    return browserProxy.whenCalled('searchEngineEditStarted').then(function() {
      dialog.$.cancel.click();
      return browserProxy.whenCalled('searchEngineEditCancelled');
    });
  });

  // Tests the dialog to add a new search engine. Specifically
  //  - cr-input elements are empty initially.
  //  - action button initially disabled.
  //  - validation is triggered on 'input' event.
  //  - action button is enabled when all fields are valid.
  //  - action button triggers appropriate browser signal when tapped.
  test('DialogAddSearchEngine', function() {
    /**
     * Triggers an 'input' event on the cr-input element and checks that
     * validation is triggered.
     */
    function inputAndValidate(inputId: string): Promise<void> {
      const inputElement =
          dialog.shadowRoot!.querySelector<CrInputElement>(`#${inputId}`)!;
      browserProxy.resetResolver('validateSearchEngineInput');
      inputElement.dispatchEvent(
          new CustomEvent('input', {bubbles: true, composed: true}));
      return inputElement.value !== '' ?
          // Expecting validation only on non-empty values.
          browserProxy.whenCalled('validateSearchEngineInput') :
          Promise.resolve();
    }

    const actionButton = dialog.$.actionButton;

    return browserProxy.whenCalled('searchEngineEditStarted')
        .then(() => {
          assertEquals('', dialog.$.searchEngine.value);
          assertEquals('', dialog.$.keyword.value);
          assertEquals('', dialog.$.queryUrl.value);
          assertTrue(actionButton.disabled);
        })
        .then(() => inputAndValidate('searchEngine'))
        .then(() => inputAndValidate('keyword'))
        .then(() => inputAndValidate('queryUrl'))
        .then(() => {
          // Manually set the text to a non-empty string for all fields.
          dialog.$.searchEngine.value = 'foo';
          dialog.$.keyword.value = 'bar';
          dialog.$.queryUrl.value = 'baz';

          return inputAndValidate('searchEngine');
        })
        .then(() => {
          // Assert that the action button has been enabled now that all
          // input is valid and non-empty.
          assertFalse(actionButton.disabled);
          actionButton.click();
          return browserProxy.whenCalled('searchEngineEditCompleted');
        });
  });

  test('DialogCloseWhenEnginesChangedModelEngineNotFound', function() {
    dialog.set('model', createSampleSearchEngine({id: 0, name: 'G'}));
    webUIListenerCallback('search-engines-changed', {
      defaults: [],
      actives: [],
      others: [createSampleSearchEngine({id: 1, name: 'H'})],
      extensions: [],
    });
    return browserProxy.whenCalled('searchEngineEditCancelled');
  });

  test('DialogValidateInputsWhenEnginesChanged', function() {
    dialog.set('model', createSampleSearchEngine({name: 'G'}));
    dialog.set('keyword_', 'G');
    webUIListenerCallback('search-engines-changed', {
      defaults: [],
      actives: [],
      others: [createSampleSearchEngine({name: 'G'})],
      extensions: [],
    });
    return browserProxy.whenCalled('validateSearchEngineInput');
  });
});

suite('SearchEngineEntryTests', function() {
  let entry: SettingsSearchEngineEntryElement;
  let browserProxy: TestSearchEnginesBrowserProxy;

  const searchEngine = createSampleSearchEngine(
      {canBeDefault: true, canBeEdited: true, canBeRemoved: true});

  setup(function() {
    browserProxy = new TestSearchEnginesBrowserProxy();
    SearchEnginesBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = '';
    entry = document.createElement('settings-search-engine-entry');
    entry.set('engine', searchEngine);
    document.body.appendChild(entry);
  });

  teardown(function() {
    entry.remove();
  });

  // Test that the <search-engine-entry> is populated according to its
  // underlying SearchEngine model.
  test('Initialization', function() {
    assertEquals(
        searchEngine.displayName,
        entry.shadowRoot!.querySelector('#name-column')!.textContent!.trim());
    assertEquals(
        searchEngine.keyword,
        entry.shadowRoot!.querySelector('#keyword-column')!.textContent);
    assertEquals(
        searchEngine.url,
        entry.shadowRoot!.querySelector('#url-column')!.textContent);
  });

  test('Remove_Enabled', function() {
    // Open action menu.
    entry.shadowRoot!.querySelector('cr-icon-button')!.click();
    const menu = entry.shadowRoot!.querySelector('cr-action-menu')!;
    assertTrue(menu.open);

    const deleteButton = entry.$.delete;
    assertTrue(!!deleteButton);
    assertFalse(deleteButton.hidden);
    deleteButton.click();
    return browserProxy.whenCalled('removeSearchEngine')
        .then(function(modelIndex) {
          assertFalse(menu.open);
          assertEquals(entry.engine.modelIndex, modelIndex);
        });
  });

  test('MakeDefault_Enabled', function() {
    // Open action menu.
    entry.shadowRoot!.querySelector('cr-icon-button')!.click();
    const menu = entry.shadowRoot!.querySelector('cr-action-menu')!;
    assertTrue(menu.open);

    const makeDefaultButton = entry.$.makeDefault;
    assertTrue(!!makeDefaultButton);
    makeDefaultButton.click();
    return browserProxy.whenCalled('setDefaultSearchEngine')
        .then(function(modelIndex) {
          assertFalse(menu.open);
          assertEquals(entry.engine.modelIndex, modelIndex);
        });
  });

  // Test that clicking the "edit" menu item fires an edit event.
  test('Edit_Enabled', function() {
    // Open action menu.
    entry.shadowRoot!
        .querySelector<HTMLElement>('cr-icon-button.icon-more-vert')!.click();
    const menu = entry.shadowRoot!.querySelector('cr-action-menu')!;
    assertTrue(menu.open);

    const engine = entry.engine;
    const editButton = entry.$.edit;
    assertTrue(!!editButton);
    assertFalse(editButton.hidden);

    const promise = eventToPromise('edit-search-engine', entry).then(e => {
      assertEquals(engine, e.detail.engine);
      assertEquals(
          entry.shadowRoot!.querySelector('cr-icon-button'),
          e.detail.anchorElement);
    });
    editButton.click();
    return promise;
  });

  /**
   * Checks that the given button is hidden for the given search engine.
   */
  function testButtonHidden(searchEngine: SearchEngine, buttonId: string) {
    entry.engine = searchEngine;
    const button =
        entry.shadowRoot!.querySelector<HTMLButtonElement>(`#${buttonId}`);
    assertTrue(!!button);
    assertTrue(button!.hidden);
  }

  test('Remove_Hidden', function() {
    testButtonHidden(createSampleSearchEngine({canBeRemoved: false}), 'delete');
  });

  /**
   * Checks that the given button is disabled for the given search engine.
   */
  function testButtonDisabled(searchEngine: SearchEngine, buttonId: string) {
    entry.engine = searchEngine;
    const button =
        entry.shadowRoot!.querySelector<HTMLButtonElement>(`#${buttonId}`);
    assertTrue(!!button);
    assertTrue(button!.disabled);
  }

  test('MakeDefault_Disabled', function() {
    testButtonDisabled(
        createSampleSearchEngine({canBeDefault: false}), 'makeDefault');
  });

  test('Edit_Disabled', function() {
    testButtonDisabled(createSampleSearchEngine({canBeEdited: false}), 'edit');
  });

  // Test that clicking the "activate" button fires an activate event.
  test('Activate', async function() {
    entry.set('isActiveSearchEnginesFlagEnabled', true);
    flush();
    entry.engine = createSampleSearchEngine({canBeActivated: true});

    const activateButton = entry.shadowRoot!.querySelector<HTMLButtonElement>(
        'cr-button.secondary-button')!;
    assertTrue(!!activateButton);
    assertFalse(activateButton.hidden);
    activateButton.click();

    // Ensure that the activate event is fired.
    const [modelIndex, isActive] =
        await browserProxy.whenCalled('setIsActiveSearchEngine');
    assertEquals(entry.engine.modelIndex, modelIndex);
    assertTrue(isActive);
  });

  // Test that clicking the "Deactivate" button fires a deactivate event.
  test('Deactivate', async function() {
    entry.set('isActiveSearchEnginesFlagEnabled', true);
    flush();
    entry.engine = createSampleSearchEngine({canBeDeactivated: true});

    // Open action menu.
    entry.shadowRoot!
        .querySelector<HTMLElement>('cr-icon-button.icon-more-vert')!.click();
    const menu = entry.shadowRoot!.querySelector('cr-action-menu')!;
    assertTrue(menu.open);

    const deactivateButton = entry.shadowRoot!.querySelector<HTMLButtonElement>(
        'button#deactivate.dropdown-item')!;
    assertTrue(!!deactivateButton);
    assertFalse(deactivateButton.hidden);
    deactivateButton.click();

    // Ensure that the deactivate event is fired.
    const [modelIndex, isActive] =
        await browserProxy.whenCalled('setIsActiveSearchEngine');
    assertEquals(entry.engine.modelIndex, modelIndex);
    assertFalse(isActive);
  });
});

suite('SearchEnginePageTests', function() {
  let page: SettingsSearchEnginesPageElement;
  let browserProxy: TestSearchEnginesBrowserProxy;

  const searchEnginesInfo: SearchEnginesInfo = {
    defaults: [createSampleSearchEngine({
      id: 0,
      name: 'search_engine_G',
      displayName: 'search_engine_G displayName',
      keyword: 'search_engine_G'
    })],
    actives: [],
    others: [
      createSampleSearchEngine({
        id: 1,
        name: 'search_engine_B',
        displayName: 'search_engine_B displayName',
        keyword: 'search_engine_B'
      }),
      createSampleSearchEngine({
        id: 2,
        name: 'search_engine_A',
        displayName: 'search_engine_A displayName',
        keyword: 'search_engine_A'
      }),
    ],
    extensions: [createSampleOmniboxExtension()],
  };

  setup(function() {
    browserProxy = new TestSearchEnginesBrowserProxy();

    // Purposefully pass a clone of |searchEnginesInfo| to avoid any
    // mutations on ground truth data.
    browserProxy.setSearchEnginesInfo({
      defaults: searchEnginesInfo.defaults.slice(),
      actives: searchEnginesInfo.actives.slice(),
      others: searchEnginesInfo.others.slice(),
      extensions: searchEnginesInfo.extensions.slice(),
    });
    loadTimeData.overrideValues({'showKeywordTriggerSetting': true});
    SearchEnginesBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = '';
    page = document.createElement('settings-search-engines-page');
    page.set('prefs.omnibox.keyword_space_triggering_enabled', {
      key: 'prefs.omnibox.keyword_space_triggering_enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    });
    document.body.appendChild(page);
    return browserProxy.whenCalled('getSearchEnginesList');
  });

  teardown(function() {
    page.remove();
  });

  // Tests that the page is querying and displaying search engine info on
  // startup.
  test('Initialization', function() {
    const searchEnginesLists =
        page.shadowRoot!.querySelectorAll('settings-search-engines-list');
    assertEquals(2, searchEnginesLists.length);

    flush();
    const defaultsList = searchEnginesLists[0]!;
    const defaultsEntries = defaultsList.shadowRoot!.querySelectorAll(
        'settings-search-engine-entry');
    assertEquals(searchEnginesInfo.defaults.length, defaultsEntries.length);

    const othersList = searchEnginesLists[1]!;
    const othersEntries =
        othersList.shadowRoot!.querySelectorAll('settings-search-engine-entry');
    assertEquals(searchEnginesInfo.others.length, othersEntries.length);

    // Ensure that the search engines have reverse alphabetical order in the
    // model.
    assertTrue(
        searchEnginesInfo.others[0]!.name > searchEnginesInfo.others[1]!.name);

    // Ensure that they are displayed in alphabetical order.
    assertEquals(
        searchEnginesInfo.others[1]!.name, othersEntries[0]!.engine.name);
    assertEquals(
        searchEnginesInfo.others[0]!.name, othersEntries[1]!.engine.name);

    const extensionEntries = page.shadowRoot!.querySelector('iron-list')!.items;
    assertEquals(searchEnginesInfo.extensions.length, extensionEntries!.length);
  });

  // Test that the keyboard shortcut radio buttons are shown as expected, and
  // toggling them fires the appropriate events.
  test('KeyboardShortcutSettingToggle', async function() {
    const radioGroup = page.$.keyboardShortcutSettingGroup;
    assertTrue(!!radioGroup);
    assertFalse(radioGroup.hidden);

    const radioButtons =
        page.shadowRoot!.querySelectorAll('controlled-radio-button')!;
    assertEquals(2, radioButtons.length);
    assertEquals('true', radioButtons.item(0)!.name);
    assertEquals('false', radioButtons.item(1)!.name);

    // Check behavior when switching space triggering off.
    radioButtons.item(1)!.click();
    flush();
    assertEquals('false', radioGroup.selected);
    let result =
        await browserProxy.whenCalled('recordSearchEnginesPageHistogram');
    assertEquals(SearchEnginesInteractions.KEYBOARD_SHORTCUT_TAB, result);
    browserProxy.reset();

    // Check behavior when switching space triggering on.
    radioButtons.item(0).click();
    flush();
    assertEquals('true', radioGroup.selected);
    result = await browserProxy.whenCalled('recordSearchEnginesPageHistogram');
    assertEquals(
        SearchEnginesInteractions.KEYBOARD_SHORTCUT_SPACE_OR_TAB, result);
  });

  // Test that the "no other search engines" message is shown/hidden as
  // expected.
  test('NoOtherSearchEnginesMessage', function() {
    webUIListenerCallback('search-engines-changed', {
      defaults: [],
      actives: [],
      others: [],
      extensions: [],
    });

    const message = page.shadowRoot!.querySelector('#noOtherEngines');
    assertTrue(!!message);
    assertFalse(message!.hasAttribute('hidden'));

    webUIListenerCallback('search-engines-changed', {
      defaults: [],
      actives: [],
      others: [createSampleSearchEngine()],
      extensions: [],
    });
    assertTrue(message!.hasAttribute('hidden'));
  });

  // Tests that the add search engine dialog opens when the corresponding
  // button is tapped.
  test('AddSearchEngineDialog', function() {
    assertFalse(
        !!page.shadowRoot!.querySelector('settings-search-engine-dialog'));
    const addSearchEngineButton = page.$.addSearchEngine;
    assertTrue(!!addSearchEngineButton);

    addSearchEngineButton.click();
    flush();
    assertTrue(
        !!page.shadowRoot!.querySelector('settings-search-engine-dialog'));
  });

  test('EditSearchEngineDialog', function() {
    const engine = searchEnginesInfo.others[0]!;
    page.dispatchEvent(new CustomEvent('edit-search-engine', {
      bubbles: true,
      composed: true,
      detail: {engine, anchorElement: page.$.addSearchEngine}
    }));
    return browserProxy.whenCalled('searchEngineEditStarted')
        .then(modelIndex => {
          assertEquals(engine.modelIndex, modelIndex);
          const dialog =
              page.shadowRoot!.querySelector('settings-search-engine-dialog')!;
          assertTrue(!!dialog);

          // Check that the cr-input fields are pre-populated.
          assertEquals(engine.name, dialog.$.searchEngine.value);
          assertEquals(engine.keyword, dialog.$.keyword.value);
          assertEquals(engine.url, dialog.$.queryUrl.value);

          assertFalse(dialog.$.actionButton.disabled);
        });
  });

  // Tests that filtering the three search engines lists works, and that the
  // "no search results" message is shown as expected.
  test('FilterSearchEngines', function() {
    flush();

    function getListItems(listIndex: number) {
      const list = listIndex === 2 /* extensions */ ?
          page.shadowRoot!.querySelector('iron-list')!.items :
          page.shadowRoot!
              .querySelectorAll('settings-search-engines-list')[listIndex]!
              .shadowRoot!.querySelectorAll('settings-search-engine-entry');

      return list;
    }

    function assertSearchResults(
        defaultsCount: number, othersCount: number, extensionsCount: number) {
      assertEquals(defaultsCount, getListItems(0)!.length);
      assertEquals(othersCount, getListItems(1)!.length);
      assertEquals(extensionsCount, getListItems(2)!.length);

      const noResultsElements = Array.from(
          page.shadowRoot!.querySelectorAll<HTMLElement>('.no-search-results'));
      assertEquals(defaultsCount > 0, noResultsElements[0]!.hidden);
      assertEquals(othersCount > 0, noResultsElements[1]!.hidden);
      assertEquals(extensionsCount > 0, noResultsElements[2]!.hidden);
    }

    assertSearchResults(1, 2, 1);

    // Search by name
    page.filter = searchEnginesInfo.defaults[0]!.name;
    flush();
    assertSearchResults(1, 0, 0);

    // Search by displayName
    page.filter = searchEnginesInfo.others[0]!.displayName;
    flush();
    assertSearchResults(0, 1, 0);

    // Search by keyword
    page.filter = searchEnginesInfo.others[1]!.keyword;
    flush();
    assertSearchResults(0, 1, 0);

    // Search by URL
    page.filter = 'search?';
    flush();
    assertSearchResults(1, 2, 0);

    // Test case where none of the sublists have results.
    page.filter = 'does not exist';
    flush();
    assertSearchResults(0, 0, 0);

    // Test case where an 'extension' search engine matches.
    page.filter = 'extension';
    flush();
    assertSearchResults(0, 0, 1);
  });
});

suite('OmniboxExtensionEntryTests', function() {
  let entry: SettingsOmniboxExtensionEntryElement;
  let browserProxy: TestExtensionControlBrowserProxy;

  setup(function() {
    browserProxy = new TestExtensionControlBrowserProxy();
    ExtensionControlBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = '';
    entry = document.createElement('settings-omnibox-extension-entry');
    entry.set('engine', createSampleOmniboxExtension());
    document.body.appendChild(entry);

    // Open action menu.
    entry.shadowRoot!.querySelector('cr-icon-button')!.click();
  });

  teardown(function() {
    entry.remove();
  });

  test('Manage', function() {
    const manageButton = entry.$.manage;
    assertTrue(!!manageButton);
    manageButton.click();
    return browserProxy.whenCalled('manageExtension')
        .then(function(extensionId) {
          assertEquals(entry.engine.extension!.id, extensionId);
        });
  });

  test('Disable', function() {
    const disableButton = entry.$.disable;
    assertTrue(!!disableButton);
    disableButton.click();
    return browserProxy.whenCalled('disableExtension')
        .then(function(extensionId) {
          assertEquals(entry.engine.extension!.id, extensionId);
        });
  });
});
