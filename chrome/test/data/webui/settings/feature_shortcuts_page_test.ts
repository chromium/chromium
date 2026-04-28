// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {CategorizedTemplateUrls, FeatureShortcutsPageElement} from 'chrome://settings/settings.js';
import {SearchEnginesBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {loadTimeData} from 'chrome://settings/settings.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {createSampleSearchEngine, TestSearchEnginesBrowserProxy} from './test_search_engines_browser_proxy.js';

// clang-format on

/**
 * Generates sample CategorizedTemplateUrls for testing the UI.
 */
function generateCategorizedTemplateUrls(): CategorizedTemplateUrls {
  return {
    activeSiteShortcuts: [],
    inactiveSiteShortcuts: [],
    activeFeatureShortcuts: [
      createSampleSearchEngine({
        name: 'Feature 1',
        id: 30,
        url: 'chrome://feature1',
        keyword: 'f1',
      }),
    ],
    inactiveFeatureShortcuts: [
      createSampleSearchEngine({
        name: 'Inactive Feature 1',
        id: 40,
        url: 'chrome://infeature1',
        keyword: 'if1',
      }),
    ],
  };
}

suite('FeatureShortcutsPageTest', function() {
  let page: FeatureShortcutsPageElement;
  let browserProxy: TestSearchEnginesBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestSearchEnginesBrowserProxy();
    const categorizedData = generateCategorizedTemplateUrls();
    browserProxy.setCategorizedTemplateUrls(categorizedData);
    SearchEnginesBrowserProxyImpl.setInstance(browserProxy);

    loadTimeData.overrideValues({
      searchSettingsUpdate: true,
    });

    page = document.createElement('settings-feature-shortcuts-page');
    document.body.appendChild(page);

    return flushTasks();
  });

  test('SectionsVisibleAndInitialState', async function() {
    assertFalse(page.$.activeShortcutsRow.expanded);
    assertFalse(page.$.inactiveShortcutsRow.expanded);
    assertFalse(isVisible(page.$.activeShortcutsList));
    assertFalse(isVisible(page.$.activeShortcutsList));

    // Expand the sections, which should show the lists.
    page.$.activeShortcutsRow.click();
    page.$.inactiveShortcutsRow.click();
    await flushTasks();

    assertTrue(page.$.activeShortcutsRow.expanded);
    assertTrue(page.$.inactiveShortcutsRow.expanded);
    assertTrue(isVisible(page.$.activeShortcutsList));
    assertTrue(isVisible(page.$.activeShortcutsList));
  });

  test('ReactToChangesFromBrowserProxy', async function() {
    // Initial state
    assertEquals(1, page.$.activeShortcutsList.engines.length);
    assertEquals(1, page.$.inactiveShortcutsList.engines.length);

    // Move the inactive feature shortcut to the active ones.
    const categorizedData = generateCategorizedTemplateUrls();
    categorizedData.activeFeatureShortcuts.push(
        ...categorizedData.inactiveFeatureShortcuts);
    categorizedData.inactiveFeatureShortcuts = [];
    browserProxy.setCategorizedTemplateUrls(categorizedData);
    webUIListenerCallback('search-engines-changed', categorizedData);
    await flushTasks();

    // Updated state
    assertEquals(2, page.$.activeShortcutsList.engines.length);
    assertEquals(0, page.$.inactiveShortcutsList.engines.length);
  });

  // Tests that there is a message shown if there are no shortcuts present for
  // the section.
  test('NoResultsState', async function() {
    const emptyCategorizedData = {
      activeSiteShortcuts: [],
      inactiveSiteShortcuts: [],
      activeFeatureShortcuts: [],
      inactiveFeatureShortcuts: [],
    };
    browserProxy.setCategorizedTemplateUrls(emptyCategorizedData);
    webUIListenerCallback('search-engines-changed', emptyCategorizedData);
    await flushTasks();

    assertFalse(isVisible(page.$.noActiveShortcutsFound));
    assertFalse(isVisible(page.$.noInactiveShortcutsFound));

    // Expand the sections, which should show the active/inactive feature and
    // extension shortcuts messages.
    page.$.activeShortcutsRow.click();
    page.$.inactiveShortcutsRow.click();
    await flushTasks();

    assertTrue(isVisible(page.$.noActiveShortcutsFound));
    assertTrue(isVisible(page.$.noInactiveShortcutsFound));
  });
});
