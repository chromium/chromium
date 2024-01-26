// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for storage-access-static-site-list-entry.
 */

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {StorageAccessStaticSiteListEntry, StorageAccessStaticSiteListEntryElement} from 'chrome://settings/lazy_load.js';
import {ContentSettingsTypes, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertTrue, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
// clang-format on

const origin = 'https://example.com';
const embeddingOrigin = 'https://embedding-origin.com';
const displayName = 'example.com';
const embeddingDisplayName = 'embedding-origin.com';
const resetAriaLabel = 'Reset example.com';

/**
 * An example of a `StorageAccessStaticSiteListEntry`.
 */
const storageAccessException: StorageAccessStaticSiteListEntry = {
  faviconOrigin: origin,
  displayName: displayName,
  description: 'description',
  resetAriaLabel: resetAriaLabel,
  origin: origin,
  embeddingOrigin: '',
  incognito: false,
};

/**
 * An example of a `StorageAccessStaticSiteListEntry` in incognito for a nested
 * row.
 */
const incognitoStorageAccessException: StorageAccessStaticSiteListEntry = {
  faviconOrigin: embeddingOrigin,
  displayName: embeddingDisplayName,
  resetAriaLabel: resetAriaLabel,
  origin: origin,
  embeddingOrigin: embeddingDisplayName,
  incognito: true,
};

suite('StorageAccessStaticSiteListEntry', function() {
  /**
   * A site list element created before each test.
   */
  let testElement: StorageAccessStaticSiteListEntryElement;

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  // Initialize a storage-access-static-site-list-entry before each test.
  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement =
        document.createElement('storage-access-static-site-list-entry');
    document.body.appendChild(testElement);
  });

  /**
   * Configures the test element for a particular category.
   * @param exception The `StorageAccessStaticSiteListEntry` to use.
   */
  function setUpEntry(exception: StorageAccessStaticSiteListEntry):
      Promise<void> {
    testElement.model = exception;
    return flushTasks();
  }

  test('origin site name', async function() {
    await setUpEntry(storageAccessException);

    // Validate that the StorageAccess origin is displayed.
    assertEquals(
        storageAccessException.displayName,
        testElement.$.displayName.querySelector('.site-representation')!
            .textContent!.trim());
  });

  test('origin site description', async function() {
    await setUpEntry(storageAccessException);

    const secondLine = testElement.$.displayName.querySelector('.second-line');
    assertTrue(!!secondLine);

    // Validate the row description.
    assertEquals(
        storageAccessException.description, secondLine.textContent!.trim());
  });

  test('site incognito', async function() {
    await setUpEntry(incognitoStorageAccessException);

    assertTrue(!!testElement.shadowRoot);
    assertTrue(!!testElement.shadowRoot.querySelector('#incognitoTooltip'));
  });

  test('site not incognito', async function() {
    await setUpEntry(storageAccessException);

    assertTrue(!!testElement.shadowRoot);
    assertFalse(!!testElement.shadowRoot.querySelector('#incognitoTooltip'));
  });

  test('reset permission without embedding origin', async function() {
    await setUpEntry(storageAccessException);

    // Click on the reset button.
    testElement.$.resetButton.click();

    const args =
        await browserProxy.whenCalled('resetCategoryPermissionForPattern');

    assertEquals(origin, args[0]);
    assertEquals(storageAccessException.embeddingOrigin, args[1]);
    assertEquals(ContentSettingsTypes.STORAGE_ACCESS, args[2]);
    assertEquals(/*incognito=*/ false, args[3]);
  });

  test('reset incognito permission with embedding origin', async function() {
    await setUpEntry(incognitoStorageAccessException);

    // Click on the reset button.
    testElement.$.resetButton.click();

    const args =
        await browserProxy.whenCalled('resetCategoryPermissionForPattern');

    assertEquals(incognitoStorageAccessException.origin, args[0]);
    assertEquals(incognitoStorageAccessException.embeddingOrigin, args[1]);
    assertEquals(ContentSettingsTypes.STORAGE_ACCESS, args[2]);
    assertEquals(/*incognito=*/ true, args[3]);
  });

  test('reset site aria-label', async function() {
    await setUpEntry(storageAccessException);

    // Validate reset button aria-label for nested-rows.
    assertEquals(
        storageAccessException.resetAriaLabel,
        testElement.$.resetButton.getAttribute('aria-label'));
  });
});
