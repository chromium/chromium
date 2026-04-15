// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {CrInputElement, SettingsSearchEngineEditDialogElement} from 'chrome://settings/lazy_load.js';
import {loadTimeData, SearchEnginesBrowserProxyImpl} from 'chrome://settings/settings.js';
import type {SearchEngine} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {createSampleSearchEngine, TestSearchEnginesBrowserProxy} from './test_search_engines_browser_proxy.js';
// clang-format on

suite('SearchEngineEditDialog', function() {
  let dialog: SettingsSearchEngineEditDialogElement;
  let browserProxy: TestSearchEnginesBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({searchSettingsUpdate: true});

    browserProxy = new TestSearchEnginesBrowserProxy();
    SearchEnginesBrowserProxyImpl.setInstance(browserProxy);
    dialog = document.createElement('settings-search-engine-edit-dialog');
    document.body.appendChild(dialog);
  });

  function createEditDialog(engine: SearchEngine) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('settings-search-engine-edit-dialog');
    dialog.model = engine;
    document.body.appendChild(dialog);
    return flushTasks();
  }

  /**
   * A helper function to test the "edit search engine" dialog. It verifies
   * its initial state, including the title, pre-populated input fields, and
   * the state of the action buttons. It also checks whether the fields are
   * correctly marked as read-only and that validation is skipped for
   * read-only engines.
   */
  function testEditSearchEngineDialog(
      engine: SearchEngine, expectedDialogTitle: string,
      expectedReadonly: boolean) {
    const policySubtitleContainer =
        dialog.shadowRoot!.querySelector('#policySubtitleContainer');
    assertTrue(!!policySubtitleContainer);
    assertEquals(
        expectedDialogTitle,
        dialog.shadowRoot!.querySelector(
                              'div[slot="title"]')!.textContent.trim());

    // Check that the cr-input fields are pre-populated.
    assertEquals(engine.name, dialog.$.searchEngine.value);
    assertEquals(expectedReadonly, dialog.$.searchEngine.readonly);
    assertEquals(engine.keyword, dialog.$.keyword.value);
    assertEquals(expectedReadonly, dialog.$.keyword.readonly);
    assertEquals(engine.url, dialog.$.queryUrl.value);
    assertEquals(expectedReadonly, dialog.$.queryUrl.readonly);

    assertEquals(expectedReadonly, dialog.$.cancel.hidden);
    assertFalse(dialog.$.cancel.disabled);
    assertFalse(dialog.$.actionButton.hidden);
    assertFalse(dialog.$.actionButton.disabled);
    assertEquals(
        loadTimeData.getString(expectedReadonly ? 'done' : 'save'),
        dialog.$.actionButton.textContent.trim());

    // Ensures that field validation is not run for readonly search engines
    // created by policy (crbug.com/348165485).
    if (expectedReadonly) {
      browserProxy.resetResolver('validateSearchEngineInput');
      dialog.$.keyword.dispatchEvent(
          new CustomEvent('input', {bubbles: true, composed: true}));
      assertEquals(0, browserProxy.getCallCount('validateSearchEngineInput'));
    }
  }

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
        actionButton.textContent.trim(), loadTimeData.getString('add'));
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

  test('DialogCloseWhenEnginesChangedModelEngineNotFound', function() {
    dialog.model = createSampleSearchEngine({id: 0, name: 'G'});
    webUIListenerCallback('search-engines-changed', {
      activeSiteShortcuts: [],
      inactiveSiteShortcuts: [createSampleSearchEngine({id: 1, name: 'H'})],
      activeFeatureShortcuts: [],
      inactiveFeatureShortcuts: [],
    });
    return browserProxy.whenCalled('searchEngineEditCancelled');
  });

  test('DialogValidateInputsWhenEnginesChanged', function() {
    dialog.model = createSampleSearchEngine({name: 'G'});
    dialog.set('keyword_', 'G');
    webUIListenerCallback('search-engines-changed', {
      activeSiteShortcuts: [],
      inactiveSiteShortcuts: [createSampleSearchEngine({name: 'G'})],
      activeFeatureShortcuts: [],
      inactiveFeatureShortcuts: [],
    });
    return browserProxy.whenCalled('validateSearchEngineInput');
  });

  test('EditSearchEngineDialog_IsManaged', function() {
    const engine = createSampleSearchEngine({
      id: 1,
      name: 'search_engine_active_E',
      displayName: 'E displayName',
      keyword: 'active E',
      url: 'https://www.active_E.com/search?q=%s',
      canBeEdited: true,
      isManaged: true,
    });
    createEditDialog(engine);

    return testEditSearchEngineDialog(
        engine, loadTimeData.getString('searchEnginesEditSiteSearch'),
        /*expectedReadonly=*/ false);
  });

  test('EditSearchEngineDialog_IsManaged_Readonly', function() {
    const engine = createSampleSearchEngine({
      id: 2,
      name: 'search_engine_active_F',
      displayName: 'F displayName',
      keyword: 'active F',
      url: 'https://www.active_F.com/search?q=%s',
      canBeEdited: false,
      isManaged: true,
    });
    createEditDialog(engine);

    return testEditSearchEngineDialog(
        engine, loadTimeData.getString('searchEnginesViewSiteSearch'),
        /*expectedReadonly=*/ true);
  });

  test('EditSearchEngineDialog_Prepopulated_IsManaged', function() {
    const engine = createSampleSearchEngine({
      id: 3,
      name: 'search_engine_default_B',
      displayName: 'B displayName',
      keyword: 'default B',
      url: 'https://www.default_b.com/search?q=%s',
      canBeEdited: true,
      isManaged: true,
      isPrepopulated: true,
    });
    createEditDialog(engine);

    return testEditSearchEngineDialog(
        engine, loadTimeData.getString('searchEnginesEditSearchEngine'),
        /*expectedReadonly=*/ false);
  });

  test('EditSearchEngineDialog_Prepopulated_IsManaged_Readonly', function() {
    const engine = createSampleSearchEngine({
      id: 4,
      name: 'search_engine_default_D',
      displayName: 'D displayName',
      keyword: 'default D',
      url: 'https://www.default_d.com/search?q=%s',
      canBeEdited: false,
      isManaged: true,
      isPrepopulated: true,
    });
    createEditDialog(engine);

    return testEditSearchEngineDialog(
        engine, loadTimeData.getString('searchEnginesViewSearchEngine'),
        /*expectedReadonly=*/ true);
  });

  test('EditSearchEngineDialog_UrlLocked', function() {
    const engine = createSampleSearchEngine({
      id: 5,
      name: 'search_engine_default_B',
      displayName: 'B displayName',
      keyword: 'default B',
      url: 'https://www.default_b.com/search?q=%s',
      urlLocked: true,
    });
    createEditDialog(engine);

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
        dialog.$.actionButton.textContent.trim(),
        loadTimeData.getString('save'));
  });
});

// The settings-search-engines-page receives an `SearchEnginesInfo` object upon
// `search-engines-change`.
suite('SearchEngineEditDialogInSearchEnginesPage', function() {
  let dialog: SettingsSearchEngineEditDialogElement;
  let browserProxy: TestSearchEnginesBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({searchSettingsUpdate: false});

    browserProxy = new TestSearchEnginesBrowserProxy();
    SearchEnginesBrowserProxyImpl.setInstance(browserProxy);
    dialog = document.createElement('settings-search-engine-edit-dialog');
    document.body.appendChild(dialog);
  });

  test('DialogCloseWhenEnginesChangedModelEngineNotFound', function() {
    dialog.model = createSampleSearchEngine({id: 0, name: 'G'});
    webUIListenerCallback('search-engines-changed', {
      defaults: [],
      actives: [],
      others: [createSampleSearchEngine({id: 1, name: 'H'})],
      extensions: [],
    });
    return browserProxy.whenCalled('searchEngineEditCancelled');
  });

  test('DialogValidateInputsWhenEnginesChanged', function() {
    dialog.model = createSampleSearchEngine({name: 'G'});
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
