// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {ContentSettingsTypes, SettingsSafetyHubPageElement, SettingsSafetyHubModuleElement} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse,assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestSafetyHubBrowserProxy} from './test_safety_hub_browser_proxy.js';
// clang-format on

suite('SafetyHubTests', function() {
  let testElement: SettingsSafetyHubPageElement;
  let safetyHubBrowserProxy: TestSafetyHubBrowserProxy;

  const unusedSitePermissionMockData = [{
    origin: 'www.example.com',
    permissions: [ContentSettingsTypes.CAMERA],
    expiration: '13317004800000000',  // Represents 2023-01-01T00:00:00.
  }];

  setup(function() {
    safetyHubBrowserProxy = new TestSafetyHubBrowserProxy();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-safety-hub-page');
    document.body.appendChild(testElement);
    return flushTasks();
  });

  test('DummyTest', function() {
    const container = testElement.shadowRoot!.querySelector('.tile-container');
    assertTrue(!!container);
  });

  test('Unused Site Permissions Module Visibility', async function() {
    // The element is not visible when there is nothing to review.
    safetyHubBrowserProxy.setUnusedSitePermissions([]);
    testElement = document.createElement('settings-safety-hub-page');
    document.body.appendChild(testElement);
    await flushTasks();
    assertFalse(
        isChildVisible(testElement, 'settings-unused-site-permissions'));

    // The element becomes visible if the list of permissions is no longer
    // empty.
    webUIListenerCallback(
        'unused-permission-review-list-maybe-changed',
        unusedSitePermissionMockData);
    await flushTasks();
    assertTrue(isChildVisible(
        testElement, 'settings-safety-hub-unused-site-permissions'));

    // Once visible, it remains visible regardless of list length.
    webUIListenerCallback('unused-permission-review-list-maybe-changed', []);
    await flushTasks();
    assertTrue(isChildVisible(
        testElement, 'settings-safety-hub-unused-site-permissions'));

    webUIListenerCallback(
        'unused-permission-review-list-maybe-changed',
        unusedSitePermissionMockData);
    await flushTasks();
    assertTrue(isChildVisible(
        testElement, 'settings-safety-hub-unused-site-permissions'));
  });
});

suite('SafetyHubModuleTests', function() {
  let testElement: SettingsSafetyHubModuleElement;

  const mockData = [1, 2, 3, 4].map(i => ({
                                      origin: `https://www.example${i}.com:443`,
                                      detail: `Detail ${i}`,
                                    }));

  function getEntries() {
    return testElement.shadowRoot!.querySelectorAll('.site-entry');
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-safety-hub-module');
    document.body.appendChild(testElement);
  });

  teardown(function() {
    testElement.remove();
  });

  test('testHeaderAndSubheaderText', function() {
    const headerText = 'Test header text';
    const subheaderText = 'Test subheader text';
    testElement.header = headerText;
    testElement.subheader = subheaderText;
    flush();

    function assertTextContent(query: string, text: string) {
      const element = testElement.shadowRoot!.querySelector(query);
      assertTrue(!!element);
      assertEquals(text, element.textContent!.trim());
    }

    assertTextContent('#header', headerText);
    assertTextContent('#subheader', subheaderText);
  });

  test('testItemButton', async function() {
    testElement.sites = mockData;
    testElement.buttonIcon = 'cr20:block';
    flush();

    // User clicks the button of the 2nd item in the list.
    const item = getEntries()[1]!;
    const button = item.querySelector('cr-icon-button');
    assertTrue(!!button);
    assertEquals('cr20:block', button!.ironIcon);

    const clickEventPromise =
        eventToPromise('sh-module-item-button-click', testElement);
    button.click();
    const e = await clickEventPromise;
    const clickedItem = e.detail;
    assertEquals(clickedItem.origin, mockData[1]!.origin);
    assertEquals(clickedItem.detail, mockData[1]!.detail);
  });

  test('testItemList', function() {
    testElement.sites = mockData;
    flush();

    const entries = getEntries();
    assertEquals(entries.length, mockData.length);

    // Check that the text describing the item is correct.
    for (let i = 0; i < mockData.length; i++) {
      assertEquals(
          mockData[i]!.origin,
          entries[i]!.querySelector(
                         '.site-representation')!.textContent!.trim());
      assertEquals(
          mockData[i]!.detail,
          entries[i]!.querySelector('.cr-secondary-text')!.textContent!.trim());
    }
  });
});
