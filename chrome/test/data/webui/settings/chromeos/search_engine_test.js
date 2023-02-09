// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Router, SearchEnginesBrowserProxyImpl} from 'chrome://os-settings/chromeos/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

suite('SearchEngine', function() {
  /** @type {?SettingsSearchEngineElement} */
  let page = null;

  let browserProxy = null;

  let searchEngineInfo = null;

  function createSampleSearchEngine(
      canBeDefault, canBeEdited, canBeRemoved, name, modelIndex) {
    return {
      canBeDefault: canBeDefault,
      canBeEdited: canBeEdited,
      canBeRemoved: canBeRemoved,
      default: false,
      displayName: name,
      iconURL: 'http://www.google.com/favicon.ico',
      id: 0,
      isOmniboxExtension: false,
      keyword: 'google.com',
      modelIndex: modelIndex,
      name: name,
      url: 'https://search.foo.com/search?p=%s',
      urlLocked: false,
    };
  }

  function generateSearchEngineInfo() {
    const searchEngines0 =
        createSampleSearchEngine(true, false, false, 'SearchEngine0', 0);
    searchEngines0.default = true;
    const searchEngines1 =
        createSampleSearchEngine(true, false, false, 'SearchEngine1', 1);
    searchEngines1.default = false;
    const searchEngines2 =
        createSampleSearchEngine(true, false, false, 'SearchEngine2', 2);
    searchEngines2.default = false;

    return {
      defaults: [searchEngines0, searchEngines1, searchEngines2],
      others: [],
      extensions: [],
    };
  }

  setup(function() {
    loadTimeData.overrideValues({
      shouldShowQuickAnswersSettings: false,
    });

    searchEngineInfo = generateSearchEngineInfo();
    browserProxy = TestMock.fromClass(SearchEnginesBrowserProxyImpl);
    browserProxy.setResultMapperFor('getSearchEnginesList', async () => {
      return searchEngineInfo;
    });

    SearchEnginesBrowserProxyImpl.setInstanceForTesting(browserProxy);
    PolymerTest.clearBody();
    page = document.createElement('settings-search-engine');
    page.prefs = {
      default_search_provider_data: {
        template_url_data: {},
      },
    };
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  // Tests that the page is querying and displaying search engine info on
  // startup.
  test('Initialization', async () => {
    // Dialog should initially be hidden.
    assertFalse(
        !!page.shadowRoot.querySelector('os-settings-search-selection-dialog'));

    await browserProxy.whenCalled('getSearchEnginesList');
    flush();

    // Sublabel should initially display the first search engine's name.
    const searchEngineSubLabel =
        page.shadowRoot.querySelector('#currentSearchEngine');
    assertTrue(!!searchEngineSubLabel);
    assertEquals(
        searchEngineInfo.defaults[0].name,
        searchEngineSubLabel.innerHTML.trim());

    // Click the dialog button.
    const dialogButton =
        page.shadowRoot.querySelector('#searchSelectionDialogButton');
    assertTrue(!!dialogButton);
    dialogButton.click();
    flush();

    await browserProxy.whenCalled('getSearchEnginesList');
    flush();

    // Dialog should now be showing.
    const dialog =
        page.shadowRoot.querySelector('os-settings-search-selection-dialog');
    assertTrue(!!dialog);
    const selectElement = dialog.shadowRoot.querySelector('select');
    assertTrue(!!selectElement);
    assertEquals(0, selectElement.selectedIndex);

    // Simulate a user initiated change of the default search engine.
    selectElement.selectedIndex = 1;
    assertEquals(1, selectElement.selectedIndex);
    const doneButton = dialog.shadowRoot.querySelector('.action-button');
    assertTrue(!!doneButton);
    doneButton.click();

    // Simulate the change
    searchEngineInfo.defaults[0].default = false;
    searchEngineInfo.defaults[1].default = true;
    searchEngineInfo.defaults[2].default = false;
    await browserProxy.whenCalled('setDefaultSearchEngine');
    webUIListenerCallback('search-engines-changed', searchEngineInfo);
    flush();

    // The sublabel should now be updated to the new search engine.
    assertEquals(
        searchEngineInfo.defaults[1].name,
        searchEngineSubLabel.innerHTML.trim());

    // Simulate a change that happened in browser settings.
    searchEngineInfo.defaults[0].default = false;
    searchEngineInfo.defaults[1].default = false;
    searchEngineInfo.defaults[2].default = true;

    browserProxy.resetResolver('setDefaultSearchEngine');
    webUIListenerCallback('search-engines-changed', searchEngineInfo);
    flush();
    assertEquals(
        searchEngineInfo.defaults[2].name,
        searchEngineSubLabel.innerHTML.trim());

    browserProxy.whenCalled('setDefaultSearchEngine').then(function() {
      // Since the change happened in a different tab, there should be
      // no new call to |setDefaultSearchEngine|.
      assertNotReached('Should not call setDefaultSearchEngine again');
    });
  });

  test('ControlledByExtension', async () => {
    await browserProxy.whenCalled('getSearchEnginesList');
    const dialogButton =
        page.shadowRoot.querySelector('#searchSelectionDialogButton');
    assertFalse(dialogButton.disabled);
    assertFalse(
        !!page.shadowRoot.querySelector('extension-controlled-indicator'));

    page.set('prefs.default_search_provider_data.template_url_data', {
      controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
      controlledByName: 'fake extension name',
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      extensionId: 'fake extension id',
      extensionCanBeDisabled: true,
      value: {},
    });
    flush();

    assertTrue(dialogButton.disabled);
    assertTrue(
        !!page.shadowRoot.querySelector('extension-controlled-indicator'));
    assertFalse(!!page.shadowRoot.querySelector('cr-policy-pref-indicator'));
  });

  test('ControlledByPolicy', async () => {
    await browserProxy.whenCalled('getSearchEnginesList');
    const dialogButton =
        page.shadowRoot.querySelector('#searchSelectionDialogButton');
    assertFalse(dialogButton.disabled);
    assertFalse(
        !!page.shadowRoot.querySelector('extension-controlled-indicator'));

    page.set('prefs.default_search_provider_data.template_url_data', {
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      value: {},
    });
    flush();

    assertTrue(dialogButton.disabled);
    assertFalse(
        !!page.shadowRoot.querySelector('extension-controlled-indicator'));
    assertTrue(!!page.shadowRoot.querySelector('cr-policy-pref-indicator'));
  });
});
