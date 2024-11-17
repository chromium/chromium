// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsSearchEngineListDialogElement, SearchEnginesInfo, SettingsSearchPageElement} from 'chrome://settings/settings.js';
import type {CrCheckboxElement} from 'chrome://settings/lazy_load.js';
import {SearchEnginesBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';

import {createSampleSearchEngine, TestSearchEnginesBrowserProxy} from './test_search_engines_browser_proxy.js';

// clang-format on

function generateSearchEngineInfo(): SearchEnginesInfo {
  const searchEngines0 =
      createSampleSearchEngine({canBeDefault: true, default: true, id: 0});
  const searchEngines1 = createSampleSearchEngine({canBeDefault: true, id: 1});
  const searchEngines2 = createSampleSearchEngine({canBeDefault: true, id: 2});

  return {
    defaults: [searchEngines0, searchEngines1, searchEngines2],
    actives: [],
    others: [],
    extensions: [],
  };
}

suite('SearchPageTests', function() {
  let page: SettingsSearchPageElement;
  let browserProxy: TestSearchEnginesBrowserProxy;
  let metrics: MetricsTracker;

  setup(function() {
    metrics = fakeMetricsPrivate();
    browserProxy = new TestSearchEnginesBrowserProxy();
    browserProxy.setSearchEnginesInfo(generateSearchEngineInfo());
    SearchEnginesBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-search-page');
    page.prefs = {
      default_search_provider_data: {
        template_url_data: {},
      },
    };
    document.body.appendChild(page);
    return flushTasks();
  });

  teardown(function() {
    page.remove();
  });

  // Tests that the page is querying and displaying search engine info on
  // startup.
  test('Initialization', async function() {
    await browserProxy.whenCalled('getSearchEnginesList');
    flush();

    // Open the search engine list dialog.
    const openSearchEngineListButton =
        page.shadowRoot!.querySelector<HTMLButtonElement>('#openDialogButton')!;
    openSearchEngineListButton.click();
    assertEquals(metrics.count('ChooseDefaultSearchEngine'), 1);

    await flushTasks();

    const searchEngineListDialog =
        page.shadowRoot!.querySelector<SettingsSearchEngineListDialogElement>(
            'settings-search-engine-list-dialog')!;

    const radioGroupElement =
        searchEngineListDialog.shadowRoot!.querySelector('cr-radio-group')!;
    assertEquals('0', radioGroupElement.selected);

    const saveGuestChoiceCheckbox =
        searchEngineListDialog.shadowRoot!.querySelector(
            '#saveGuestChoiceCheckbox')!;
    assertFalse(!!saveGuestChoiceCheckbox);

    // Simulate a user initiated change of the default search engine.
    const radioButtons =
        searchEngineListDialog.shadowRoot!.querySelectorAll('cr-radio-button');
    const setAsDefaultButton =
        searchEngineListDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '#setAsDefaultButton')!;
    radioButtons[1]!.click();
    setAsDefaultButton.click();

    const [, , saveGuestChoice] =
        await browserProxy.whenCalled('setDefaultSearchEngine');
    assertEquals(saveGuestChoice, null);

    assertEquals('1', radioGroupElement.selected);

    // Simulate a change that happened in a different tab.
    const searchEnginesInfo = generateSearchEngineInfo();
    searchEnginesInfo.defaults[0]!.default = false;
    searchEnginesInfo.defaults[1]!.default = false;
    searchEnginesInfo.defaults[2]!.default = true;

    browserProxy.resetResolver('setDefaultSearchEngine');
    webUIListenerCallback('search-engines-changed', searchEnginesInfo);
    flush();
    assertEquals('2', radioGroupElement.selected);

    browserProxy.whenCalled('setDefaultSearchEngine').then(function() {
      // Since the change happened in a different tab, there should be
      // no new call to |setDefaultSearchEngine|.
      assertNotReached('Should not call setDefaultSearchEngine again');
    });
  });

  test('ControlledByExtension', async function() {
    await browserProxy.whenCalled('getSearchEnginesList');

    const openSearchEngineListButton =
        page.shadowRoot!.querySelector<HTMLButtonElement>('#openDialogButton')!;
    assertFalse(openSearchEngineListButton.disabled);
    assertFalse(
        !!page.shadowRoot!.querySelector('extension-controlled-indicator'));

    page.set('prefs.default_search_provider_data.template_url_data', {
      controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
      controlledByName: 'fake extension name',
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      extensionId: 'fake extension id',
      extensionCanBeDisabled: true,
      value: {},
    });
    flush();

    assertTrue(openSearchEngineListButton['disabled']);
    assertTrue(
        !!page.shadowRoot!.querySelector('extension-controlled-indicator'));
    assertFalse(!!page.shadowRoot!.querySelector('cr-policy-pref-indicator'));
  });

  test('ControlledByPolicy', async function() {
    await browserProxy.whenCalled('getSearchEnginesList');
    const openSearchEngineListButton =
        page.shadowRoot!.querySelector<HTMLButtonElement>('#openDialogButton')!;
    assertFalse(openSearchEngineListButton.disabled);
    assertFalse(
        !!page.shadowRoot!.querySelector('extension-controlled-indicator'));

    page.set('prefs.default_search_provider_data.template_url_data', {
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      value: {},
    });
    flush();

    assertTrue(openSearchEngineListButton.disabled);
    assertFalse(
        !!page.shadowRoot!.querySelector('extension-controlled-indicator'));
    assertTrue(!!page.shadowRoot!.querySelector('cr-policy-pref-indicator'));
  });

  test('ShowGuestSaveCheckbox', async function() {
    browserProxy.setSaveGuestChoice(true);
    await browserProxy.whenCalled('getSearchEnginesList');
    flush();

    // Open the search engine list dialog.
    const openSearchEngineListButton =
        page.shadowRoot!.querySelector<HTMLButtonElement>('#openDialogButton')!;
    openSearchEngineListButton.click();
    assertEquals(metrics.count('ChooseDefaultSearchEngine'), 1);

    await flushTasks();

    const searchEngineListDialog =
        page.shadowRoot!.querySelector<SettingsSearchEngineListDialogElement>(
            'settings-search-engine-list-dialog')!;

    const saveGuestChoiceCheckbox =
        searchEngineListDialog.shadowRoot!.querySelector<CrCheckboxElement>(
            '#saveGuestChoiceCheckbox')!;
    assertTrue(!!saveGuestChoiceCheckbox);
    assertTrue(saveGuestChoiceCheckbox.checked);

    saveGuestChoiceCheckbox.click();
    await flushTasks();
    assertFalse(saveGuestChoiceCheckbox.checked);

    const setAsDefaultButton =
        searchEngineListDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '#setAsDefaultButton')!;
    setAsDefaultButton.click();

    const [, , saveGuestChoice] =
        await browserProxy.whenCalled('setDefaultSearchEngine');
    assertFalse(saveGuestChoice);
  });
});
