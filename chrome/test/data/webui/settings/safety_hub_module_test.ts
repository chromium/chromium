// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {SettingsSafetyHubModuleElement} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('SafetyHubModule', function() {
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
