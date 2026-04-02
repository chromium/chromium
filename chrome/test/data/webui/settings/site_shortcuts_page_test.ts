// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {SiteShortcutsPageElement} from 'chrome://settings/settings.js';
import {SearchEnginesBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {loadTimeData} from 'chrome://settings/settings.js';
import {isVisible} from 'chrome://webui-test/test_util.js';
import type {CategorizedTemplateUrls} from 'chrome://settings/settings.js';

import {createSampleSearchEngine, TestSearchEnginesBrowserProxy} from './test_search_engines_browser_proxy.js';

// clang-format on

/**
 * Generates sample CategorizedTemplateUrls for testing the updated UI.
 */
function generateCategorizedTemplateUrls(): CategorizedTemplateUrls {
  return {
    activeSiteShortcuts: [
      createSampleSearchEngine({
        name: 'Prepopulated Site 1',
        id: 10,
        url: 'https://site1.com/{searchTerms}',
        keyword: 's1',
        isPrepopulated: true,
      }),
      createSampleSearchEngine({
        name: 'Prepopulated Site 2',
        id: 11,
        url: 'https://site1.com/{searchTerms}',
        keyword: 's1',
        isPrepopulated: true,
      }),
      createSampleSearchEngine({
        name: 'Active Site 1',
        id: 12,
        url: 'https://site1.com/{searchTerms}',
        keyword: 's1',
        default: true,
      }),
      createSampleSearchEngine({
        name: 'Active Site 2',
        id: 13,
        url: 'https://site2.com/{searchTerms}',
        keyword: 's2',
      }),
    ],
    inactiveSiteShortcuts: [
      createSampleSearchEngine({
        name: 'Inactive Site 1',
        id: 20,
        url: 'https://in1.com/{searchTerms}',
        keyword: 'in1',
      }),
    ],
    activeFeatureShortcuts: [],
    inactiveFeatureShortcuts: [],
  };
}

suite('SiteShortcutsPageTest', function() {
  let page: SiteShortcutsPageElement;
  let browserProxy: TestSearchEnginesBrowserProxy;
  const categorizedData = generateCategorizedTemplateUrls();

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestSearchEnginesBrowserProxy();
    browserProxy.setCategorizedTemplateUrls(categorizedData);
    SearchEnginesBrowserProxyImpl.setInstance(browserProxy);

    loadTimeData.overrideValues({
      searchSettingsUpdate: true,
    });

    page = document.createElement('settings-site-shortcuts-page');
    document.body.appendChild(page);

    return flushTasks();
  });

  test('SectionsVisibleAndInitialState', async function() {
    assertFalse(page.$.activeShortcutsRow.expanded);
    assertFalse(page.$.inactiveShortcutsRow.expanded);
    assertFalse(isVisible(page.$.activeShortcutsList));
    assertFalse(isVisible(page.$.inactiveShortcutsList));

    // The button is initially not visible because the section is collapsed.
    assertFalse(isVisible(page.$.addSearchEngine));

    // Expand the sections, which should show the add button and the lists.
    page.$.activeShortcutsRow.click();
    page.$.inactiveShortcutsRow.click();
    await flushTasks();

    assertTrue(page.$.activeShortcutsRow.expanded);
    assertTrue(page.$.inactiveShortcutsRow.expanded);
    assertTrue(isVisible(page.$.activeShortcutsList));
    assertTrue(isVisible(page.$.inactiveShortcutsList));

    assertTrue(isVisible(page.$.addSearchEngine));
  });

  test('SiteShortcutsListContent', function() {
    // Active site shortcuts
    assertEquals(
        categorizedData.activeSiteShortcuts.length,
        page.$.activeShortcutsList.engines.length);
    page.$.activeShortcutsList.engines.forEach((engine, i) => {
      assertEquals(categorizedData.activeSiteShortcuts[i]!.name, engine.name);
    });

    // Inactive site shortcuts
    assertEquals(
        categorizedData.inactiveSiteShortcuts.length,
        page.$.inactiveShortcutsList.engines.length);
    assertEquals(
        categorizedData.inactiveSiteShortcuts[0]!.name,
        page.$.inactiveShortcutsList.engines[0]!.name);
  });

  test('ReactToChangesFromBrowserProxy', async function() {
    // Initial state
    assertEquals(4, page.$.activeShortcutsList.engines.length);
    assertEquals(1, page.$.inactiveShortcutsList.engines.length);

    // Move the inactive site shortcut to the active ones.
    categorizedData.activeSiteShortcuts.push(
        ...categorizedData.inactiveSiteShortcuts);
    categorizedData.inactiveSiteShortcuts = [];
    browserProxy.setCategorizedTemplateUrls(categorizedData);
    webUIListenerCallback('search-engines-changed', categorizedData);
    await flushTasks();

    // Updated state
    assertEquals(5, page.$.activeShortcutsList.engines.length);
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

    // Expand the sections, which should show the active/inactive site shortcuts
    // messages.
    page.$.activeShortcutsRow.click();
    page.$.inactiveShortcutsRow.click();
    await flushTasks();

    assertTrue(isVisible(page.$.noActiveShortcutsFound));
    assertTrue(isVisible(page.$.noInactiveShortcutsFound));
  });

  test('AddSearchEngineOpensEditDialog', async function() {
    // Open the collapsed section.
    page.$.activeShortcutsRow.click();
    await flushTasks();

    // Click on "Add".
    page.$.addSearchEngine.click();
    await flushTasks();

    assertTrue(
        !!page.shadowRoot!.querySelector('settings-search-engine-edit-dialog'));
  });

  test('EditSearchEngineEventOpensEditDialog', async function() {
    const testEngine = createSampleSearchEngine({id: 10, name: 'Edit Me'});
    page.dispatchEvent(new CustomEvent('view-or-edit-search-engine', {
      bubbles: true,
      composed: true,
      detail: {engine: testEngine, anchorElement: page.$.activeShortcutsRow},
    }));
    await flushTasks();

    assertTrue(
        !!page.shadowRoot!.querySelector('settings-search-engine-edit-dialog'));
  });

  test('DeleteSearchEngineEventOpensConfirmationDialog', async function() {
    const testEngine = createSampleSearchEngine({id: 20, name: 'Delete Me'});
    page.dispatchEvent(new CustomEvent('delete-search-engine', {
      bubbles: true,
      composed: true,
      detail: {engine: testEngine, anchorElement: page.$.activeShortcutsRow},
    }));
    await flushTasks();

    assertTrue(!!page.shadowRoot!.querySelector(
        'settings-simple-confirmation-dialog'));
  });

  test('ConfirmDeleteCallsBrowserProxy', async function() {
    const testEngine = createSampleSearchEngine({id: 20, name: 'Delete Me'});
    page.dispatchEvent(new CustomEvent('delete-search-engine', {
      bubbles: true,
      composed: true,
      detail: {engine: testEngine, anchorElement: page},
    }));
    await flushTasks();

    // Accept the dialog.
    const deleteDialog =
        page.shadowRoot!.querySelector('settings-simple-confirmation-dialog');
    assertTrue(!!deleteDialog);
    deleteDialog.$.confirm.click();
    await flushTasks();

    const args = await browserProxy.whenCalled('removeSearchEngine');
    assertEquals(20, args);
  });

  test('CancelDeleteDoesNotCallBrowserProxy', async function() {
    const testEngine = createSampleSearchEngine({id: 20, name: 'Delete Me'});
    page.dispatchEvent(new CustomEvent('delete-search-engine', {
      bubbles: true,
      composed: true,
      detail: {engine: testEngine, anchorElement: page},
    }));
    await flushTasks();

    // Cancel the dialog.
    const deleteDialog =
        page.shadowRoot!.querySelector('settings-simple-confirmation-dialog');
    assertTrue(!!deleteDialog);
    deleteDialog.$.cancel.click();
    await flushTasks();

    assertEquals(0, browserProxy.getCallCount('removeSearchEngine'));
  });
});
