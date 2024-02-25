// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for storage-access-site-list. */

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {StorageAccessSiteException, StorageAccessSiteListElement} from 'chrome://settings/lazy_load.js';
import {ContentSetting, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertTrue, assertDeepEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';
import {loadTimeData} from 'chrome://settings/settings.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
import {createStorageAccessSiteException, createStorageAccessEmbeddingException} from './test_util.js';
// clang-format on

/**
 * Origin used in |exceptionStorageAccessOrigin| and
 * |exceptionStorageAccessOriginWithIncognito|.
 */
const origin = 'https://example.com';

/**
 * Header used for the list of |StorageAccessSiteException|s.
 */
const categoryHeader = 'category header';

/**
 * An example of an array of a |StorageAccessSiteException|s of type
 * |ContentSetting.BLOCK|.
 */
const exceptionsStorageAccessOriginBlock: StorageAccessSiteException[] = [
  createStorageAccessSiteException(origin, {
    setting: ContentSetting.BLOCK,
    openDescription: 'open description',
    closeDescription: 'close description',
    exceptions: [
      createStorageAccessEmbeddingException(
          'https://foo.com', {description: 'embedding description'}),
    ],
  }),
  createStorageAccessSiteException(origin, {
    setting: ContentSetting.BLOCK,
    openDescription: 'open description 2',
    closeDescription: 'close description 2',
    exceptions: [
      createStorageAccessEmbeddingException(
          'https://foo2.com', {description: 'embedding 2 description'}),
    ],
  }),
];

suite('StorageAccessSiteList', function() {
  /**
   * A storage access site list element created before each test.
   */
  let testElement: StorageAccessSiteListElement;

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  // Initialize a storage-access-site-list before each test.
  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('storage-access-site-list');
    document.body.appendChild(testElement);
  });

  /**
   * Configures the test element for a particular category.
   * @param exceptions The |StorageAccessSiteException| to use.
   * @param categorySubtype Category (e.g. allow, block) to show exceptions for.
   */
  async function setUpEntry(
      exceptions: StorageAccessSiteException[],
      categorySubtype: ContentSetting): Promise<void> {
    browserProxy.setStorageAccessExceptionList(exceptions);
    testElement.categoryHeader = categoryHeader;
    testElement.categorySubtype = categorySubtype;
    await browserProxy.whenCalled('getStorageAccessExceptionList');
    return flushTasks();
  }

  test('category header', async function() {
    await setUpEntry(exceptionsStorageAccessOriginBlock, ContentSetting.BLOCK);

    // Validate that the StorageAccess origin is displayed on the top level row.
    assertTrue(!!testElement.shadowRoot);
    const headerRow =
        testElement.shadowRoot.querySelector('.cr-row .cr-secondary-text');
    assertTrue(!!headerRow);
    assertEquals(categoryHeader, headerRow.textContent!.trim());
  });

  test('storage access site list entries', async function() {
    await setUpEntry(exceptionsStorageAccessOriginBlock, ContentSetting.BLOCK);

    const entries = testElement.$.listContainer.querySelectorAll(
        'storage-access-site-list-entry');
    assertTrue(!!entries);
    assertEquals(exceptionsStorageAccessOriginBlock.length, entries.length);

    const firstEntry = entries[0];
    assertTrue(!!firstEntry);
    const secondEntry = entries[1];
    assertTrue(!!secondEntry);

    // Check if `StorageAccessSiteListEntry`s were build with the correct
    // params.
    assertEquals(exceptionsStorageAccessOriginBlock[0], firstEntry.model);
    assertEquals(exceptionsStorageAccessOriginBlock[1], secondEntry.model);
  });

  test('storage access site list empty', async function() {
    await setUpEntry(exceptionsStorageAccessOriginBlock, ContentSetting.ALLOW);

    const entries = testElement.$.listContainer.querySelectorAll(
        'storage-access-site-list-entry');
    assertTrue(!!entries);
    assertEquals(0, entries.length);

    const noSitesAddedString = loadTimeData.getString('noSitesAdded');
    assertTrue(!!testElement.shadowRoot);

    const listItems =
        testElement.shadowRoot.querySelectorAll<HTMLElement>('.list-item');
    assertTrue(!!listItems);
    const noSitesAddedElement = listItems[0];

    assertTrue(!!noSitesAddedElement);
    assertTrue(isVisible(noSitesAddedElement));
    assertEquals(noSitesAddedString, noSitesAddedElement.textContent!.trim());
  });

  test('storage access site list entries with empty filter', async function() {
    testElement.searchFilter = '';
    await setUpEntry(exceptionsStorageAccessOriginBlock, ContentSetting.BLOCK);

    const entries = testElement.$.listContainer.querySelectorAll(
        'storage-access-site-list-entry');
    assertTrue(!!entries);
    assertEquals(exceptionsStorageAccessOriginBlock.length, entries.length);

    const firstEntry = entries[0];
    assertTrue(!!firstEntry);
    const secondEntry = entries[1];
    assertTrue(!!secondEntry);

    // Check if `StorageAccessSiteListEntry`s were build with the correct
    // params.
    assertEquals(exceptionsStorageAccessOriginBlock[0], firstEntry.model);
    assertEquals(exceptionsStorageAccessOriginBlock[1], secondEntry.model);
  });

  test(
      'storage access site list entries with matching filter',
      async function() {
        testElement.searchFilter = origin;
        await setUpEntry(
            exceptionsStorageAccessOriginBlock, ContentSetting.BLOCK);

        const entries = testElement.$.listContainer.querySelectorAll(
            'storage-access-site-list-entry');
        assertTrue(!!entries);
        assertEquals(2, entries.length);

        const firstEntry = entries[0];
        assertTrue(!!firstEntry);
        const secondEntry = entries[1];
        assertTrue(!!secondEntry);

        // Check if `StorageAccessSiteListEntry`s were build with the correct
        // params.
        assertDeepEquals(
            exceptionsStorageAccessOriginBlock[0], firstEntry.model);
        assertDeepEquals(
            exceptionsStorageAccessOriginBlock[1], secondEntry.model);
      });

  test(
      'storage access site list entries with matching filter on embedding',
      async function() {
        testElement.searchFilter = 'foo2';
        await setUpEntry(
            exceptionsStorageAccessOriginBlock, ContentSetting.BLOCK);


        const entries = testElement.$.listContainer.querySelectorAll(
            'storage-access-site-list-entry');
        assertTrue(!!entries);
        assertEquals(1, entries.length);

        const entry = entries[0];
        assertTrue(!!entry);

        // Only the second embedding matches the filter.
        assertDeepEquals(exceptionsStorageAccessOriginBlock[1], entry.model);
      });

  test(
      'storage access site list entries with no matching filter',
      async function() {
        testElement.searchFilter = 'test';
        await setUpEntry(
            exceptionsStorageAccessOriginBlock, ContentSetting.BLOCK);

        const entries = testElement.$.listContainer.querySelectorAll(
            'storage-access-site-list-entry');
        assertTrue(!!entries);
        assertEquals(0, entries.length);

        const noSearchResults = loadTimeData.getString('searchNoResults');
        assertTrue(!!testElement.shadowRoot);

        const listItems =
            testElement.shadowRoot.querySelectorAll<HTMLElement>('.list-item');
        assertTrue(!!listItems);
        const noSearchResultsElement = listItems[1];

        assertTrue(!!noSearchResultsElement);
        assertTrue(isVisible(noSearchResultsElement));
        assertEquals(
            noSearchResults, noSearchResultsElement.textContent!.trim());
      });
});
