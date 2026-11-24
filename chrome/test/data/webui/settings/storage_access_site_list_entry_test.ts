// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for storage-access-site-list-entry. */

import 'chrome://settings/lazy_load.js';

import type {CrExpandButtonElement, StorageAccessSiteException, StorageAccessSiteListEntryElement, StorageAccessStaticSiteListEntry} from 'chrome://settings/lazy_load.js';
import {ContentSetting, ContentSettingsTypes, SiteSettingsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {loadTimeData} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';
import {createStorageAccessEmbeddingException, createStorageAccessSiteException} from './test_util.js';

/**
 * Origin used in `storageAccessException` and
 * `storageAccessExceptionAppliesToEverything`.
 */
const origin = 'https://example.com';

/**
 * An example of a `StorageAccessSiteException`.
 */
const storageAccessException: StorageAccessSiteException =
    createStorageAccessSiteException(origin, {
      setting: ContentSetting.BLOCK,
      closeDescription: 'open description',
      openDescription: 'close description',
      exceptions: [
        createStorageAccessEmbeddingException(
            'https://foo.com', {description: 'embedding description'}),
        createStorageAccessEmbeddingException(
            'https://foo2.com', {description: '', incognito: true}),
      ],
    });

/**
 * An example of a `StorageAccessSiteException` with only one exception that
 * applies to every origin.
 */
const storageAccessExceptionAppliesToEverything: StorageAccessSiteException =
    createStorageAccessSiteException(origin, {
      setting: ContentSetting.BLOCK,
      description: 'description',
      incognito: false,
      exceptions: [],
    });

suite('StorageAccessSiteListEntry', function() {
  /**
   * A site list element created before each test.
   */
  let testElement: StorageAccessSiteListEntryElement;

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsBrowserProxy;

  // Initialize a storage-access-site-list-entry before each test.
  setup(function() {
    browserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('storage-access-site-list-entry');
    document.body.appendChild(testElement);
  });

  /**
   * Configures the test element for a particular category.
   */
  function setUpEntry(exception: StorageAccessSiteException): Promise<void> {
    testElement.model = exception;
    return microtasksFinished();
  }

  test('origin site name', async function() {
    await setUpEntry(storageAccessException);

    const displayName = testElement.shadowRoot.querySelector('#displayName');
    assertTrue(!!displayName);

    // Validate that the StorageAccess origin is displayed on the top level row.
    assertEquals(
        origin,
        displayName.querySelector('.site-representation')!.textContent.trim());
  });

  test('origin site description', async function() {
    await setUpEntry(storageAccessException);

    const displayName = testElement.shadowRoot.querySelector('#displayName');
    assertTrue(!!displayName);

    const secondLine = displayName.querySelector('.second-line');
    assertTrue(!!secondLine);

    // Validate the row description when closed.
    assertEquals(
        storageAccessException.closeDescription, secondLine.textContent.trim());

    const expandButton =
        testElement.shadowRoot.querySelector<CrExpandButtonElement>(
            '#expandButton');
    assertTrue(!!expandButton);
    expandButton.click();
    await microtasksFinished();

    // Validate the row description when opened.
    assertEquals(
        storageAccessException.openDescription, secondLine.textContent.trim());
  });

  test('nested site rows', async function() {
    await setUpEntry(storageAccessException);

    const expandButton =
        testElement.shadowRoot.querySelector<CrExpandButtonElement>(
            '#expandButton');
    assertTrue(!!expandButton);
    expandButton.click();
    await microtasksFinished();

    // Validate that the nested site entries are created on the collapsible
    // element.
    const siteListEntry = testElement.shadowRoot.querySelectorAll(
        'storage-access-static-site-list-entry');

    assertEquals(
        storageAccessException.exceptions.length, siteListEntry.length);

    // Validate the first nested site entry.
    const firstException = storageAccessException.exceptions[0]!;
    const expectedFirstReset = loadTimeData.getStringF(
        'storageAccessResetSite', origin, firstException.embeddingDisplayName);

    const expectedFirstModel: StorageAccessStaticSiteListEntry = {
      faviconOrigin: firstException.embeddingOrigin,
      displayName: firstException.embeddingDisplayName,
      description: firstException.description,
      resetAriaLabel: expectedFirstReset,
      origin: storageAccessException.origin,
      embeddingOrigin: firstException.embeddingOrigin,
      incognito: firstException.incognito,
    };

    assertDeepEquals(expectedFirstModel, siteListEntry[0]!.model);

    // Validate the second nested site entry.
    const secondException = storageAccessException.exceptions[1]!;
    const expectedSecondReset = loadTimeData.getStringF(
        'storageAccessResetSite', origin, secondException.embeddingDisplayName);

    const expectedSecondModel: StorageAccessStaticSiteListEntry = {
      faviconOrigin: secondException.embeddingOrigin,
      displayName: secondException.embeddingDisplayName,
      description: secondException.description,
      resetAriaLabel: expectedSecondReset,
      origin: storageAccessException.origin,
      embeddingOrigin: secondException.embeddingOrigin,
      incognito: secondException.incognito,
    };

    assertDeepEquals(expectedSecondModel, siteListEntry[1]!.model);
  });

  test('reset all permissions with the same origin site', async function() {
    await setUpEntry(storageAccessException);

    const resetAllButton =
        testElement.shadowRoot.querySelector<HTMLElement>('#resetAllButton');
    assertTrue(!!resetAllButton);
    resetAllButton.click();

    await browserProxy.whenCalled('resetCategoryPermissionForPattern');
    assertEquals(
        storageAccessException.exceptions.length,
        browserProxy.getCallCount('resetCategoryPermissionForPattern'));

    // Check if the calls were made with the correct arguments.
    const args = browserProxy.getArgs('resetCategoryPermissionForPattern');

    const argsFirstCall = args[0];
    assertEquals(origin, argsFirstCall[0]);
    assertEquals(
        storageAccessException.exceptions[0]!.embeddingOrigin,
        argsFirstCall[1]);
    assertEquals(ContentSettingsTypes.STORAGE_ACCESS, argsFirstCall[2]);
    assertEquals(
        storageAccessException.exceptions[0]!.incognito, argsFirstCall[3]);

    const argsSecondCall = args[1];
    assertEquals(origin, argsSecondCall[0]);
    assertEquals(
        storageAccessException.exceptions[1]!.embeddingOrigin,
        argsSecondCall[1]);
    assertEquals(ContentSettingsTypes.STORAGE_ACCESS, argsSecondCall[2]);
    assertEquals(
        storageAccessException.exceptions[1]!.incognito, argsSecondCall[3]);
  });

  test('reset all aria-label', async function() {
    await setUpEntry(storageAccessException);

    const resetAllButton =
        testElement.shadowRoot.querySelector<HTMLElement>('#resetAllButton');
    assertTrue(!!resetAllButton);
    resetAllButton.click();

    // Validate reset button aria-label for top-row.
    const expectedResetAllLabel =
        loadTimeData.getStringF('storageAccessResetAll', origin);
    assertEquals(
        expectedResetAllLabel, resetAllButton.getAttribute('aria-label'));
  });

  test('expand aria-label', async function() {
    await setUpEntry(storageAccessException);

    const expandButton =
        testElement.shadowRoot.querySelector<CrExpandButtonElement>(
            '#expandButton');
    assertTrue(!!expandButton);

    // Validate expand button aria-label when closed.
    const expectedExpandOpenArialLabel =
        loadTimeData.getString('storageAccessOpenExpand');
    assertEquals(
        expectedExpandOpenArialLabel, expandButton.getAttribute('aria-label'));

    expandButton.click();
    await microtasksFinished();

    // Validate expand button aria-label when opened.
    const expectedExpandCloseArialLabel =
        loadTimeData.getString('storageAccessCloseExpand');
    assertEquals(
        expectedExpandCloseArialLabel, expandButton.getAttribute('aria-label'));
  });

  test(
      'static entry for exception that applies to everything',
      async function() {
        await setUpEntry(storageAccessExceptionAppliesToEverything);

        const staticRow = testElement.shadowRoot.querySelector(
            'storage-access-static-site-list-entry');
        assertTrue(!!staticRow);

        const expectedResetAllLabel =
            loadTimeData.getStringF('storageAccessResetAll', origin);

        const expectedModel: StorageAccessStaticSiteListEntry = {
          faviconOrigin: storageAccessExceptionAppliesToEverything.origin,
          displayName: storageAccessExceptionAppliesToEverything.displayName,
          description: storageAccessExceptionAppliesToEverything.description,
          resetAriaLabel: expectedResetAllLabel,
          origin: storageAccessExceptionAppliesToEverything.origin,
          embeddingOrigin: '',
          incognito:
              storageAccessExceptionAppliesToEverything.incognito || false,
        };
        assertDeepEquals(expectedModel, staticRow.model);
      });
});
