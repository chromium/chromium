// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for storage-access-site-list-entry. */

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {StorageAccessSiteException, StorageAccessSiteListEntryElement, ContentSetting, ContentSettingsTypes, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertTrue, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
import {createStorageAccessSiteException, createStorageAccessEmbeddingException} from './test_util.js';
// clang-format on

/**
 * Origin used in |exceptionStorageAccessOrigin| and
 * |exceptionStorageAccessOriginWithIncognito|.
 */
const origin = 'https://example.com';

/**
 * An example of a |StorageAccessSiteException|.
 */
const exceptionStorageAccessOrigin: StorageAccessSiteException =
    createStorageAccessSiteException(origin, {
      setting: ContentSetting.BLOCK,
      description: 'description',
      exceptions: [
        createStorageAccessEmbeddingException(
            'https://foo.com', {description: 'embedding description'}),
        createStorageAccessEmbeddingException(
            'https://foo2.com', {description: 'embedding 2 description'}),
      ],
    });

/**
 * An example of a |StorageAccessSiteException| with an incognito exception.
 */
const exceptionStorageAccessOriginWithIncognito: StorageAccessSiteException =
    createStorageAccessSiteException(origin, {
      setting: ContentSetting.BLOCK,
      description: 'description',
      exceptions: [
        createStorageAccessEmbeddingException(
            'https://foo.com',
            {incognito: true, description: 'embedding description'}),
        createStorageAccessEmbeddingException(
            'https://foo2.com', {description: 'embedding 2 description'}),
      ],
    });

suite('StorageAccessSiteListEntry', function() {
  /**
   * A site list element created before each test.
   */
  let testElement: StorageAccessSiteListEntryElement;

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  // Initialize a storage-access-site-list-entry before each test.
  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('storage-access-site-list-entry');
    document.body.appendChild(testElement);
  });

  /**
   * Configures the test element for a particular category.
   * @param exceptions The |StorageAccessSiteException| to use.
   */
  function setUpEntry(exception: StorageAccessSiteException): Promise<void> {
    testElement.model = exception;
    return flushTasks();
  }

  test('origin site name', async function() {
    await setUpEntry(exceptionStorageAccessOrigin);

    // Validate that the StorageAccess origin is displayed on the top level row.
    assertEquals(
        origin,
        testElement.$.displayName.querySelector('.site-representation')!
            .textContent!.trim());
  });

  test('origin site description', async function() {
    await setUpEntry(exceptionStorageAccessOrigin);

    const secondLine = testElement.$.displayName.querySelector('.second-line');
    assertTrue(!!secondLine);

    // Validate the row description.
    assertEquals(
        exceptionStorageAccessOrigin.description,
        secondLine.textContent!.trim());
  });

  test('embedding site rows', async function() {
    await setUpEntry(exceptionStorageAccessOrigin);

    const collapseChild = testElement.$.originList.get();
    assertTrue(!!collapseChild);
    flush();

    // Validate that the embedding origins are displayed on the collapsible
    // element.
    const siteListEntry = collapseChild.querySelectorAll('.list-item');

    assertEquals(
        exceptionStorageAccessOrigin.exceptions.length, siteListEntry.length);

    const firstNested = siteListEntry[0];
    assertTrue(!!firstNested);

    const firstUrl = firstNested.querySelector('.url-directionality');
    assertTrue(!!firstUrl);
    assertEquals(
        exceptionStorageAccessOrigin.exceptions[0]!.embeddingDisplayName,
        firstUrl.textContent!.trim());

    const firstDescription = firstNested.querySelector('.second-line');
    assertTrue(!!firstDescription);
    assertEquals(
        exceptionStorageAccessOrigin.exceptions[0]!.description,
        firstDescription.textContent!.trim());

    const secondNested = siteListEntry[1];
    assertTrue(!!secondNested);

    const secondUrl = secondNested.querySelector('.url-directionality');
    assertTrue(!!secondUrl);
    assertEquals(
        exceptionStorageAccessOrigin.exceptions[1]!.embeddingDisplayName,
        secondUrl.textContent!.trim());

    const secondDescription = secondNested.querySelector('.second-line');
    assertTrue(!!secondDescription);
    assertEquals(
        exceptionStorageAccessOrigin.exceptions[1]!.description,
        secondDescription.textContent!.trim());
  });

  test('embedding site rows with incognito', async function() {
    await setUpEntry(exceptionStorageAccessOriginWithIncognito);

    const collapseChild = testElement.$.originList.get();
    assertTrue(!!collapseChild);
    flush();

    // Validate that the embedding origins incognito symbol is shown correctly.
    const siteListEntry = collapseChild.querySelectorAll('.list-item');

    assertEquals(
        exceptionStorageAccessOriginWithIncognito.exceptions.length,
        siteListEntry.length);

    const firstEntry = siteListEntry[0];
    assertTrue(!!firstEntry);
    assertTrue(!!firstEntry.querySelector('#incognitoTooltip0'));

    const secondEntry = siteListEntry[1];
    assertTrue(!!secondEntry);
    assertFalse(!!secondEntry.querySelector('#incognitoTooltip1'));
  });

  test('reset all permissions with the same origin site', async function() {
    await setUpEntry(exceptionStorageAccessOrigin);

    testElement.$.resetAllButton.click();

    await browserProxy.whenCalled('resetCategoryPermissionForPattern');
    assertEquals(
        exceptionStorageAccessOrigin.exceptions.length,
        browserProxy.getCallCount('resetCategoryPermissionForPattern'));

    // Check if the calls were made with the correct arguments.
    const args = browserProxy.getArgs('resetCategoryPermissionForPattern');

    const argsFirstCall = args[0];
    assertEquals(origin, argsFirstCall[0]);
    assertEquals(
        exceptionStorageAccessOrigin.exceptions[0]!.embeddingOrigin,
        argsFirstCall[1]);
    assertEquals(ContentSettingsTypes.STORAGE_ACCESS, argsFirstCall[2]);

    const argsSecondCall = args[1];
    assertEquals(origin, argsSecondCall[0]);
    assertEquals(
        exceptionStorageAccessOrigin.exceptions[1]!.embeddingOrigin,
        argsSecondCall[1]);
    assertEquals(ContentSettingsTypes.STORAGE_ACCESS, argsSecondCall[2]);
  });

  test('reset single permissions', async function() {
    await setUpEntry(exceptionStorageAccessOrigin);

    const collapseChild = testElement.$.originList.get();
    assertTrue(!!collapseChild);
    flush();

    // Click on the reset button for the first inner child.
    const resetButton =
        collapseChild.querySelector<HTMLElement>('#resetButton0');
    assertTrue(!!resetButton);
    resetButton.click();

    const args =
        await browserProxy.whenCalled('resetCategoryPermissionForPattern');

    assertEquals(origin, args[0]);
    assertEquals(
        exceptionStorageAccessOrigin.exceptions[0]!.embeddingOrigin, args[1]);
    assertEquals(ContentSettingsTypes.STORAGE_ACCESS, args[2]);
  });
});
