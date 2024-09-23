// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrInputElement, SettingsSearchEngineEditDialogElement, SettingsSearchEnginesListElement, SettingsSearchEnginesPageElement} from 'chrome://settings/lazy_load.js';
import type {SearchEnginesInfo} from 'chrome://settings/settings.js';
import {SearchEnginesBrowserProxyImpl, SearchEnginesInteractions} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {createSampleOmniboxExtension, createSampleSearchEngine, TestSearchEnginesBrowserProxy} from './test_search_engines_browser_proxy.js';
// clang-format on

suite('AddSearchEngineDialogTests', function() {
  let dialog: SettingsSearchEngineEditDialogElement;
  let browserProxy: TestSearchEnginesBrowserProxy;

  setup(function() {
    browserProxy = new TestSearchEnginesBrowserProxy();
    SearchEnginesBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('settings-search-engine-edit-dialog');
    document.body.appendChild(dialog);
  });

  teardown(function() {
    dialog.remove();
  });

  // Tests that the dialog calls 'searchEngineEditStarted' and
  // 'searchEngineEditCancelled' when closed from the 'cancel' button.
  test('DialogOpenAndCancel', async function() {
    await browserProxy.whenCalled('searchEngineEditStarted');
    dialog.$.cancel.click();
    await browserProxy.whenCalled('searchEngineEditCancelled');
  });

  // Tests the dialog to add a new search engine. Specifically
  //  - cr-input elements are empty initially.
  //  - action button initially disabled.
  //  - validation is triggered on 'input' event.
  //  - action button is enabled when all fields are valid.
  //  - action button triggers appropriate browser signal when tapped.
  test('DialogAddSearchEngine', async function() {
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

    await browserProxy.whenCalled('searchEngineEditStarted');
    assertEquals('', dialog.$.searchEngine.value);
    assertEquals('', dialog.$.keyword.value);
    assertEquals('', dialog.$.queryUrl.value);
    assertFalse(dialog.$.queryUrl.readonly);
    assertFalse(dialog.$.cancel.disabled);
    assertTrue(actionButton.disabled);
    assertEquals(
        actionButton.textContent!.trim(), loadTimeData.getString('add'));
    await inputAndValidate('searchEngine');
    await inputAndValidate('keyword');
    await inputAndValidate('queryUrl');

    // Manually set the text to a non-empty string for all fields.
    dialog.$.searchEngine.value = 'foo';
    dialog.$.keyword.value = 'bar';
    dialog.$.queryUrl.value = 'baz';

    await inputAndValidate('searchEngine');
    // Assert that the action button has been enabled now that all
    // input is valid and non-empty.
    assertFalse(actionButton.disabled);
    actionButton.click();
    await browserProxy.whenCalled('searchEngineEditCompleted');
  });

  test('DialogCloseWhenEnginesChangedModelEngineNotFound', async function() {
    dialog.set('model', createSampleSearchEngine({id: 0, name: 'G'}));
    webUIListenerCallback('search-engines-changed', {
      defaults: [],
      actives: [],
      others: [createSampleSearchEngine({id: 1, name: 'H'})],
      extensions: [],
    });
    await browserProxy.whenCalled('searchEngineEditCancelled');
  });

  test('DialogValidateInputsWhenEnginesChanged', async function() {
    dialog.set('model', createSampleSearchEngine({name: 'G'}));
    dialog.set('keyword_', 'G');
    webUIListenerCallback('search-engines-changed', {
      defaults: [],
      actives: [],
      others: [createSampleSearchEngine({name: 'G'})],
      extensions: [],
    });
    await browserProxy.whenCalled('validateSearchEngineInput');
  });
});

suite('SearchEnginePageTests', function() {
  let page: SettingsSearchEnginesPageElement;
  let searchEnginesLists: NodeListOf<SettingsSearchEnginesListElement>;
  let browserProxy: TestSearchEnginesBrowserProxy;

  const searchEnginesInfo: SearchEnginesInfo = {
    defaults: [
      createSampleSearchEngine({
        id: 0,
        name: 'search_engine_default_A',
        displayName: 'A displayName',
        keyword: 'default A',
      }),
      createSampleSearchEngine({
        id: 1,
        name: 'search_engine_default_B',
        displayName: 'B displayName',
        isManaged: true,
        keyword: 'default B',
        url: 'https://www.default_b.com/search?q=%s',
      }),
      createSampleSearchEngine({
        id: 2,
        name: 'search_engine_default_C',
        displayName: 'C displayName',
        keyword: 'default C',
        url: 'https://www.default_c.com/search?q=%s',
        urlLocked: true,
      }),
      createSampleSearchEngine({
        id: 3,
        name: 'search_engine_default_D',
        displayName: 'D displayName',
        keyword: 'default D',
      }),
      createSampleSearchEngine({
        id: 4,
        name: 'search_engine_default_E',
        displayName: 'E displayName',
        keyword: 'default E',
      }),
      createSampleSearchEngine({
        id: 5,
        name: 'search_engine_default_F',
        displayName: 'F displayName',
        keyword: 'default F',
      }),
    ],
    actives: [createSampleSearchEngine({id: 6})],
    others: [
      createSampleSearchEngine({
        id: 7,
        name: 'search_engine_G',
        displayName: 'search_engine_G displayName',
      }),
      createSampleSearchEngine(
          {id: 8, name: 'search_engine_F', keyword: 'search_engine_F keyword'}),
      createSampleSearchEngine({id: 9, name: 'search_engine_E'}),
      createSampleSearchEngine({id: 10, name: 'search_engine_D'}),
      createSampleSearchEngine({id: 11, name: 'search_engine_C'}),
      createSampleSearchEngine({id: 12, name: 'search_engine_B'}),
      createSampleSearchEngine({id: 13, name: 'search_engine_A'}),
    ],
    extensions: [createSampleOmniboxExtension()],
  };

  setup(async function() {
    browserProxy = new TestSearchEnginesBrowserProxy();

    // Purposefully pass a clone of |searchEnginesInfo| to avoid any
    // mutations on ground truth data.
    browserProxy.setSearchEnginesInfo({
      defaults: searchEnginesInfo.defaults.slice(),
      actives: searchEnginesInfo.actives.slice(),
      others: searchEnginesInfo.others.slice(),
      extensions: searchEnginesInfo.extensions.slice(),
    });
    SearchEnginesBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-search-engines-page');
    page.set('prefs.omnibox.keyword_space_triggering_enabled', {
      key: 'prefs.omnibox.keyword_space_triggering_enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    });
    document.body.appendChild(page);
    await browserProxy.whenCalled('getSearchEnginesList');
  });

  teardown(function() {
    page.remove();
  });

  // Tests that the page is querying and displaying search engine info on
  // startup.
  test('Initialization', function() {
    searchEnginesLists =
        page.shadowRoot!.querySelectorAll('settings-search-engines-list');
    assertEquals(3, searchEnginesLists.length);

    flush();
  });

  test('testDefaultsList', function() {
    const defaultsListElement = searchEnginesLists[0]!;

    // The defaults list should only show the name and shortcut columns.
    assertFalse(
        defaultsListElement.shadowRoot!.querySelector('.name')!.hasAttribute(
            'hidden'));
    assertFalse(defaultsListElement.shadowRoot!.querySelector('.shortcut')!
                    .hasAttribute('hidden'));
    assertTrue(
        defaultsListElement.shadowRoot!.querySelector('.url')!.hasAttribute(
            'hidden'));

    // The default engines list should not collapse and should show all entries
    // in the list by default.
    const lists =
        defaultsListElement.shadowRoot!.querySelectorAll('dom-repeat');
    assertEquals(1, lists.length);
    const defaultsEntries = lists[0]!.items;
    assertEquals(searchEnginesInfo.defaults.length, defaultsEntries!.length);
  });

  test('testActivesList', function() {
    const activesListElement = searchEnginesLists[1]!;

    // The actives list should only show the name and shortcut columns.
    assertFalse(
        activesListElement.shadowRoot!.querySelector('.name')!.hasAttribute(
            'hidden'));
    assertFalse(
        activesListElement.shadowRoot!.querySelector('.shortcut')!.hasAttribute(
            'hidden'));
    assertTrue(
        activesListElement.shadowRoot!.querySelector('.url')!.hasAttribute(
            'hidden'));

    // With less than `visibleEnginesSize` elements in the list, all elements
    // should be visible and the collapsible section should not be present.
    const lists = activesListElement.shadowRoot!.querySelectorAll('dom-repeat');
    const visibleEntries = lists[0]!.items;
    const collapsedEntries = lists[1]!.items;
    assertEquals(searchEnginesInfo.actives.length, visibleEntries!.length);
    assertEquals(0, collapsedEntries!.length);

    const expandButton =
        activesListElement.shadowRoot!.querySelector('cr-expand-button');
    assertTrue(!!expandButton);
    assertTrue(expandButton!.hasAttribute('hidden'));
  });

  test('testOthersList', function() {
    const othersListElement = searchEnginesLists[2]!;

    // The others list should only show the name and url columns.
    assertFalse(
        othersListElement.shadowRoot!.querySelector('.name')!.hasAttribute(
            'hidden'));
    assertTrue(
        othersListElement.shadowRoot!.querySelector('.shortcut')!.hasAttribute(
            'hidden'));
    assertFalse(
        othersListElement.shadowRoot!.querySelector('.url')!.hasAttribute(
            'hidden'));

    // Any engines greater than `visibleEnginesSize` will be in a second list
    // under the collapsible section. The button to expand this section must be
    // visible.
    const visibleEnginesSize = othersListElement.visibleEnginesSize;
    const lists = othersListElement.shadowRoot!.querySelectorAll('dom-repeat');
    const visibleEntries = lists[0]!.items;
    const collapsedEntries = lists[1]!.items;
    assertEquals(visibleEnginesSize, visibleEntries!.length);
    assertEquals(
        searchEnginesInfo.others.length - visibleEnginesSize,
        collapsedEntries!.length);

    // Ensure that the search engines have reverse alphabetical order in the
    // model.
    for (let i = 0; i < searchEnginesInfo.others.length - 1; i++) {
      assertTrue(
          searchEnginesInfo.others[i]!.name >=
          searchEnginesInfo.others[i + 1]!.name);
    }

    const othersEntries = othersListElement!.shadowRoot!.querySelectorAll(
        'settings-search-engine-entry');

    // Ensure that they are displayed in alphabetical order.
    for (let i = 0; i < othersEntries!.length - 1; i++) {
      assertTrue(
          othersEntries[i]!.engine.name <= othersEntries[i + 1]!.engine.name);
    }

    const expandButton =
        othersListElement.shadowRoot!.querySelector('cr-expand-button');
    assertTrue(!!expandButton);
    assertFalse(expandButton!.hasAttribute('hidden'));
  });

  // Test that the keyboard shortcut radio buttons are shown as expected, and
  // toggling them fires the appropriate events.
  test('KeyboardShortcutSettingToggle', async function() {
    const radioGroup = page.$.keyboardShortcutSettingGroup;
    assertTrue(!!radioGroup);
    assertFalse(radioGroup.hidden);

    const radioButtons =
        page.shadowRoot!.querySelectorAll('controlled-radio-button');
    assertEquals(2, radioButtons.length);
    assertEquals('true', radioButtons.item(0)!.name);
    assertEquals('false', radioButtons.item(1)!.name);

    // Check behavior when switching space triggering off.
    radioButtons.item(1)!.click();
    await eventToPromise('selected-changed', radioGroup);
    assertEquals('false', radioGroup.selected);
    let result =
        await browserProxy.whenCalled('recordSearchEnginesPageHistogram');
    assertEquals(SearchEnginesInteractions.KEYBOARD_SHORTCUT_TAB, result);
    browserProxy.reset();

    // Check behavior when switching space triggering on.
    radioButtons.item(0).click();
    await eventToPromise('selected-changed', radioGroup);
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
        !!page.shadowRoot!.querySelector('settings-search-engine-edit-dialog'));
    const addSearchEngineButton =
        page.shadowRoot!.querySelector<HTMLButtonElement>('#addSearchEngine')!;
    assertTrue(!!addSearchEngineButton);

    addSearchEngineButton.click();
    flush();
    assertTrue(
        !!page.shadowRoot!.querySelector('settings-search-engine-edit-dialog'));
  });

  test('EditSearchEngineDialog', async function() {
    const engine = searchEnginesInfo.others[0]!;
    page.dispatchEvent(new CustomEvent('view-or-edit-search-engine', {
      bubbles: true,
      composed: true,
      detail: {
        engine,
        anchorElement: page.shadowRoot!.querySelector('#addSearchEngine')!,
      },
    }));
    const modelIndex = await browserProxy.whenCalled('searchEngineEditStarted');
    assertEquals(engine.modelIndex, modelIndex);
    const dialog =
        page.shadowRoot!.querySelector('settings-search-engine-edit-dialog')!;
    assertTrue(!!dialog);

    // Check that the cr-input fields are pre-populated.
    assertEquals(engine.name, dialog.$.searchEngine.value);
    assertEquals(engine.keyword, dialog.$.keyword.value);
    assertEquals(engine.url, dialog.$.queryUrl.value);

    assertFalse(dialog.$.cancel.hidden);
    assertFalse(dialog.$.cancel.disabled);
    assertFalse(dialog.$.actionButton.hidden);
    assertFalse(dialog.$.actionButton.disabled);
    assertEquals(
        dialog.$.actionButton.textContent!.trim(),
        loadTimeData.getString('save'));
  });

  test('EditSearchEngineDialog_IsManaged', async function() {
    const engine = searchEnginesInfo.defaults[1]!;
    page.dispatchEvent(new CustomEvent('view-or-edit-search-engine', {
      bubbles: true,
      composed: true,
      detail: {
        engine,
        anchorElement: page.shadowRoot!.querySelector('#addSearchEngine')!,
      },
    }));
    const modelIndex = await browserProxy.whenCalled('searchEngineEditStarted');
    assertEquals(engine.modelIndex, modelIndex);
    const dialog =
        page.shadowRoot!.querySelector('settings-search-engine-edit-dialog')!;
    assertTrue(!!dialog);

    // Check that the cr-input fields are pre-populated.
    assertEquals(engine.name, dialog.$.searchEngine.value);
    assertTrue(dialog.$.searchEngine.readonly);
    assertEquals(engine.keyword, dialog.$.keyword.value);
    assertTrue(dialog.$.keyword.readonly);
    assertEquals(engine.url, dialog.$.queryUrl.value);
    assertTrue(dialog.$.queryUrl.readonly);

    assertTrue(dialog.$.cancel.hidden);
    assertFalse(dialog.$.actionButton.hidden);
    assertFalse(dialog.$.actionButton.disabled);
    assertEquals(
        dialog.$.actionButton.textContent!.trim(),
        loadTimeData.getString('done'));

    // Ensures that field validation is not run for search engines created by
    // policy (b/348165485).
    browserProxy.resetResolver('validateSearchEngineInput');
    dialog.$.keyword.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    assertEquals(0, browserProxy.getCallCount('validateSearchEngineInput'));

    assertTrue(dialog.$.cancel.hidden);
    assertFalse(dialog.$.actionButton.hidden);
    assertFalse(dialog.$.actionButton.disabled);
  });

  test('EditSearchEngineDialog_UrlLocked', async function() {
    const engine = searchEnginesInfo.defaults[2]!;
    page.dispatchEvent(new CustomEvent('view-or-edit-search-engine', {
      bubbles: true,
      composed: true,
      detail: {
        engine,
        anchorElement: page.shadowRoot!.querySelector('#addSearchEngine')!,
      },
    }));
    const modelIndex = await browserProxy.whenCalled('searchEngineEditStarted');
    assertEquals(engine.modelIndex, modelIndex);
    const dialog =
        page.shadowRoot!.querySelector('settings-search-engine-edit-dialog')!;
    assertTrue(!!dialog);

    // Check that the cr-input fields are pre-populated.
    assertEquals(engine.name, dialog.$.searchEngine.value);
    assertEquals(engine.keyword, dialog.$.keyword.value);
    assertEquals(engine.url, dialog.$.queryUrl.value);
    assertTrue(dialog.$.queryUrl.readonly);

    assertFalse(dialog.$.cancel.hidden);
    assertFalse(dialog.$.cancel.disabled);
    assertFalse(dialog.$.actionButton.hidden);
    assertFalse(dialog.$.actionButton.disabled);
    assertEquals(
        dialog.$.actionButton.textContent!.trim(),
        loadTimeData.getString('save'));
  });

  // Tests that filtering the three search engines lists works, and that the
  // "no search results" message is shown as expected.
  test('FilterSearchEngines', function() {
    flush();

    // TODO: Lookup via array index  may not be the best approach, because
    // changing the order or number of settings-search-engines-list elements
    // can break this test. Maybe we can add an id to every relevant element and
    // use that for lookup.
    function getListItems(listIndex: number) {
      const list = listIndex === 3 /* extensions */ ?
          page.shadowRoot!.querySelector('iron-list')!.items :
          page.shadowRoot!
              .querySelectorAll('settings-search-engines-list')[listIndex]!
              .shadowRoot!.querySelectorAll('settings-search-engine-entry');

      return list;
    }

    function assertSearchResults(
        defaultsCount: number, othersCount: number, extensionsCount: number) {
      assertEquals(defaultsCount, getListItems(0)!.length);
      assertEquals(othersCount, getListItems(2)!.length);
      assertEquals(extensionsCount, getListItems(3)!.length);

      const noResultsElements = Array.from(
          page.shadowRoot!.querySelectorAll<HTMLElement>('.no-search-results'));
      assertEquals(defaultsCount > 0, noResultsElements[0]!.hidden);
      assertEquals(othersCount > 0, noResultsElements[2]!.hidden);
      assertEquals(extensionsCount > 0, noResultsElements[3]!.hidden);
    }

    assertSearchResults(6, 7, 1);

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
    assertSearchResults(6, 7, 0);

    // Test case where none of the sublists have results.
    page.filter = 'does not exist';
    flush();
    assertSearchResults(0, 0, 0);

    // Test case where an 'extension' search engine matches.
    page.filter = 'extension';
    flush();
    assertSearchResults(0, 0, 1);
  });

  // Test that the "no other search engines" message is shown/hidden as
  // expected.
  test('NoSearchEnginesMessages', function() {
    webUIListenerCallback('search-engines-changed', {
      defaults: [],
      actives: [],
      others: [],
      extensions: [],
    });

    const messageActive = page.shadowRoot!.querySelector('#noActiveEngines');
    assertTrue(!!messageActive);
    assertFalse(messageActive!.hasAttribute('hidden'));

    const messageOther = page.shadowRoot!.querySelector('#noOtherEngines');
    assertTrue(!!messageOther);
    assertFalse(messageOther!.hasAttribute('hidden'));

    webUIListenerCallback('search-engines-changed', {
      defaults: [],
      actives: [createSampleSearchEngine()],
      others: [createSampleSearchEngine()],
      extensions: [],
    });
    assertTrue(messageActive!.hasAttribute('hidden'));
    assertTrue(messageOther!.hasAttribute('hidden'));
  });
});
