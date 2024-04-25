// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {SettingsSafetyHubModuleElement} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {isVisible} from 'chrome://webui-test/test_util.js';
// clang-format on

function waitUntilVisible(element: HTMLElement, intervalMs: number = 10) {
  return new Promise<void>((resolve) => {
    const interval = setInterval(() => {
      if (isVisible(element)) {
        clearInterval(interval);
        resolve();
      }
    }, intervalMs);
  });
}

suite('SafetyHubModule', function() {
  let testElement: SettingsSafetyHubModuleElement;

  const mockData = [1, 2, 3, 4].map(i => ({
                                      origin: `https://www.example${i}.com:443`,
                                      detail: `Detail ${i}`,
                                    }));

  function getEntries() {
    return testElement.shadowRoot!.querySelectorAll('.site-entry');
  }

  function assignAndShowTestData() {
    testElement.sites = mockData;
    testElement.setModelUpdateDelayMsForTesting(0);

    let callback: Function;
    const promise = new Promise((resolve) => {
      callback = resolve;
    });

    testElement.animateShow(mockData.map(data => data.origin), function() {
      // The animation delay is set to zero, so the animation will take place
      // instantaneously; but also asynchronously. Postpone the callback
      // to the end of the thread.
      setTimeout(callback, 0);
    });

    return promise;
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
    await assignAndShowTestData();
    testElement.buttonIcon = 'cr20:block';
    testElement.buttonAriaLabelId =
        'safetyCheckNotificationPermissionReviewDontAllowAriaLabel';
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

  test('testItemList', async function() {
    // Check the item list is filled with the data.
    await assignAndShowTestData();
    flush();

    assertTrue(isVisible(testElement.shadowRoot!.querySelector('#line')));
    assertTrue(isVisible(testElement.shadowRoot!.querySelector('#siteList')));

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

    // Check the item list and line is hidden when there is no item.
    testElement.sites = [];
    flush();

    assertFalse(isVisible(testElement.shadowRoot!.querySelector('#line')));
    assertFalse(isVisible(testElement.shadowRoot!.querySelector('#siteList')));
  });

  test('testTooltip', async function() {
    // Check the item list is filled with the data.
    const text = 'Dummy tooltip text';
    await assignAndShowTestData();
    testElement.buttonIcon = 'cr20:block';
    testElement.buttonAriaLabelId =
        'safetyCheckNotificationPermissionReviewDontAllowAriaLabel';
    testElement.buttonTooltipText = text;
    flush();

    // Check that the tooltip is not visible.
    let tooltip = testElement.shadowRoot!.querySelector('cr-tooltip');
    assertTrue(!!tooltip);
    assertFalse(isVisible(tooltip));

    // User focuses the button of the 2nd item in the list.
    const item = getEntries()[1]!;
    const button = item.querySelector('cr-icon-button');
    assertTrue(!!button);
    button.focus();

    // Check that the tooltip gets visible with the correct text.
    tooltip = testElement.shadowRoot!.querySelector('cr-tooltip');
    assertTrue(!!tooltip);
    await waitUntilVisible(tooltip);
    assertTrue(isVisible(tooltip));
    assertEquals(text, tooltip!.textContent!.trim());
  });
});
