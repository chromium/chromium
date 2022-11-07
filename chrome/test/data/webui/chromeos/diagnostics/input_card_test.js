// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/strings.m.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {fakeTouchDevices} from 'chrome://diagnostics/fake_data.js';
import {InputCardElement, InputCardType} from 'chrome://diagnostics/input_card.js';
import {ConnectionType, KeyboardInfo, MechanicalLayout, NumberPadPresence, PhysicalLayout, TopRightKey, TopRowKey} from 'chrome://diagnostics/input_data_provider.mojom-webui.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

/** @type {!Array<!KeyboardInfo>} */
const keyboards = [
  {
    id: 3,
    connectionType: ConnectionType.kInternal,
    name: 'AT Translated Set 2 keyboard',
    physicalLayout: PhysicalLayout.kChromeOS,
    mechanicalLayout: MechanicalLayout.kAnsi,
    hasAssistantKey: true,
    numberPadPresent: NumberPadPresence.kPresent,
    topRowKeys: [
      TopRowKey.kBack,
      TopRowKey.kForward,
      TopRowKey.kRefresh,
      TopRowKey.kFullscreen,
      TopRowKey.kOverview,
      TopRowKey.kScreenBrightnessDown,
      TopRowKey.kScreenBrightnessUp,
      TopRowKey.kVolumeMute,
      TopRowKey.kVolumeDown,
      TopRowKey.kVolumeUp,
    ],
    topRightKey: TopRightKey.kLock,
  },
  {
    id: 10,
    connectionType: ConnectionType.kBluetooth,
    name: 'ACME SuperBoard 3000',
    physicalLayout: PhysicalLayout.kUnknown,
    mechanicalLayout: MechanicalLayout.kUnknown,
    hasAssistantKey: false,
    numberPadPresent: NumberPadPresence.kUnknown,
    topRowKeys: [],
    topRightKey: TopRightKey.kUnknown,
  },
];

suite('inputCardTestSuite', function() {
  /** @type {?InputCardElement} */
  let inputCardElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    inputCardElement.remove();
    inputCardElement = null;
  });

  /**
   * @param {!InputCardType} deviceType
   * @param {!Array<!KeyboardInfo>} devices
   */
  function initializeInputCard(deviceType, devices) {
    assertFalse(!!inputCardElement);
    inputCardElement =
        /** @type {!InputCardElement} */ (document.createElement('input-card'));
    assertTrue(!!inputCardElement);
    inputCardElement.deviceType = deviceType;
    inputCardElement.devices = devices;
    document.body.appendChild(inputCardElement);

    return flushTasks();
  }

  test('KeyboardsListedCorrectly', async () => {
    await initializeInputCard(InputCardType.KEYBOARD, keyboards);
    assertEquals(
        2,
        inputCardElement.shadowRoot.querySelector('dom-repeat').items.length);
    const elements = inputCardElement.root.querySelectorAll('.device');
    assertEquals(
        keyboards[0].name, elements[0].querySelector('.device-name').innerText);
    assertEquals(
        'Internal keyboard',
        elements[0].querySelector('.device-description').innerText);
    assertEquals(
        keyboards[1].name, elements[1].querySelector('.device-name').innerText);
    assertEquals(
        'Bluetooth keyboard',
        elements[1].querySelector('.device-description').innerText);
  });

  test('TestButtonClickEvent', async () => {
    await initializeInputCard(InputCardType.KEYBOARD, keyboards);
    let listenerCalled = false;
    inputCardElement.addEventListener('test-button-click', (e) => {
      listenerCalled = true;
      assertEquals(10, e.detail.evdevId);
    });
    inputCardElement.shadowRoot
        .querySelector('.device[data-evdev-id="10"] cr-button')
        .click();
    await flushTasks();
    assertTrue(listenerCalled);
  });

  test('TouchscreenTestability', async () => {
    await initializeInputCard(InputCardType.TOUCHSCREEN, fakeTouchDevices);
    assertEquals(
        3,
        inputCardElement.shadowRoot.querySelector('dom-repeat').items.length);
    const elements = inputCardElement.root.querySelectorAll('.device');

    // Check a testable touchscreen.
    assertEquals(
        fakeTouchDevices[1].name,
        elements[1].querySelector('.device-name').innerText);
    assertFalse(elements[1].querySelector('cr-button').disabled);
    assertTrue(elements[1].querySelector('#infoIcon').hidden);

    // Check an untestable touchscreen.
    assertEquals(
        fakeTouchDevices[2].name,
        elements[2].querySelector('.device-name').innerText);
    assertTrue(elements[2].querySelector('cr-button').disabled);
    assertFalse(elements[2].querySelector('#infoIcon').hidden);
  });
});
