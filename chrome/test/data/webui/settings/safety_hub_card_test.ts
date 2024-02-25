// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {SettingsSafetyHubCardElement} from 'chrome://settings/lazy_load.js';
import {CardState} from 'chrome://settings/lazy_load.js';
import {assertEquals,assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
// clang-format on

suite('SafetyHubCard', function() {
  let testElement: SettingsSafetyHubCardElement;

  function getMockDataForState(state: CardState) {
    return {header: 'header', subheader: 'subheader', state: state};
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-safety-hub-card');
    document.body.appendChild(testElement);
  });

  teardown(function() {
    testElement.remove();
  });

  test('testHeaderAndSubheaderText', function() {
    const mockData = getMockDataForState(CardState.SAFE);
    testElement.data = mockData;
    flush();

    function assertTextContent(query: string, text: string) {
      const element = testElement.shadowRoot!.querySelector(query);
      assertTrue(!!element);
      assertEquals(text, element.textContent!.trim());
    }

    assertTextContent('#header', mockData.header);
    assertTextContent('#subheader', mockData.subheader);
  });

  test('testIcon', async function() {
    // Check icon for SAFE state.
    testElement.data = getMockDataForState(CardState.SAFE);
    flushTasks();
    assertEquals('cr:check-circle', testElement.$.icon.icon);
    assertTrue(testElement.$.icon.classList.contains('green'));

    // Check icon for INFO state.
    testElement.data = getMockDataForState(CardState.INFO);
    flushTasks();
    assertEquals('cr:info', testElement.$.icon.icon);
    assertTrue(testElement.$.icon.classList.contains('grey'));

    // Check icon for WEAK state.
    testElement.data = getMockDataForState(CardState.WEAK);
    flushTasks();
    assertEquals('cr:error', testElement.$.icon.icon);
    assertTrue(testElement.$.icon.classList.contains('yellow'));

    // Check icon for WARNING state.
    testElement.data = getMockDataForState(CardState.WARNING);
    flushTasks();
    assertEquals('cr:error', testElement.$.icon.icon);
    assertTrue(testElement.$.icon.classList.contains('red'));
  });
});
