// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SearchEngine, SettingsSearchEngineListDialogElement} from 'chrome://settings/settings.js';
import {loadTimeData, SearchEnginesBrowserProxyImpl, ChoiceMadeLocation} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {createSampleSearchEngine, TestSearchEnginesBrowserProxy} from './test_search_engines_browser_proxy.js';
// clang-format on

function generateActiveSiteShortcuts(): SearchEngine[] {
  const searchEngines0 = createSampleSearchEngine(
      {canBeDefault: true, isPrepopulated: true, id: 0});
  const searchEngines1 = createSampleSearchEngine(
      {canBeDefault: true, isPrepopulated: true, id: 1});
  const searchEngines2 = createSampleSearchEngine(
      {canBeDefault: true, isPrepopulated: false, default: true, id: 2});

  return [searchEngines0, searchEngines1, searchEngines2];
}

suite('SearchEngineListDialog', function() {
  let dialog: SettingsSearchEngineListDialogElement;
  let browserProxy: TestSearchEnginesBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({searchSettingsUpdate: true});

    browserProxy = new TestSearchEnginesBrowserProxy();
    SearchEnginesBrowserProxyImpl.setInstance(browserProxy);

    dialog = document.createElement('settings-search-engine-list-dialog');
    document.body.appendChild(dialog);
    dialog.searchEngines = generateActiveSiteShortcuts();

    return flushTasks();
  });

  test('Search engine list dialog shows engines', function() {
    const radioButtons = dialog.shadowRoot!.querySelectorAll('cr-radio-button');
    assertEquals(3, radioButtons.length);

    // The engine marked as default is selected.
    const radioGroupElement =
        dialog.shadowRoot!.querySelector('cr-radio-group');
    assertTrue(!!radioGroupElement);
    assertEquals('2', radioGroupElement.selected);
  });

  test('Search engine list dialog sets default engine', async function() {
    browserProxy.resetResolver('setDefaultSearchEngine');

    // Initially, the engine marked as default is selected.
    const radioGroupElement =
        dialog.shadowRoot!.querySelector('cr-radio-group');
    assertTrue(!!radioGroupElement);
    assertEquals('2', radioGroupElement.selected);

    // Simulate a user initiated change of the default search engine.
    const radioButtons = dialog.shadowRoot!.querySelectorAll('cr-radio-button');
    assertEquals(3, radioButtons.length);
    radioButtons[1]!.click();
    await flush();

    const setAsDefaultButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '#setAsDefaultButton');
    assertTrue(!!setAsDefaultButton);
    setAsDefaultButton.click();

    // The other search engine in the list is now selected as default.
    const [id, choiceMadeLocation] =
        await browserProxy.whenCalled('setDefaultSearchEngine');
    assertEquals(1, id);
    assertEquals(choiceMadeLocation, ChoiceMadeLocation.SEARCH_SETTINGS);
    assertEquals('1', radioGroupElement.selected);
  });

  test('Search engine list dialog reacts to changes', function() {
    // Initially, the engine marked as default is selected.
    const radioGroupElement =
        dialog.shadowRoot!.querySelector('cr-radio-group');
    assertTrue(!!radioGroupElement);
    assertEquals('2', radioGroupElement.selected);

    // Simulate setting a different default engine in a different tab.
    const activeSiteShortcuts = generateActiveSiteShortcuts();
    assertEquals(3, activeSiteShortcuts.length);
    activeSiteShortcuts[0]!.default = true;
    activeSiteShortcuts[1]!.default = false;
    activeSiteShortcuts[2]!.default = false;

    dialog.searchEngines = activeSiteShortcuts;
    flush();

    // The change in the other tab is reflected in the dialog.
    assertEquals('0', radioGroupElement.selected);
  });
});
