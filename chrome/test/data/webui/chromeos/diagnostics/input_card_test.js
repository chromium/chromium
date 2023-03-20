// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {fakeTouchDevices} from 'chrome://diagnostics/fake_data.js';
import {FakeInputDataProvider} from 'chrome://diagnostics/fake_input_data_provider.js';
import {ConnectionType, KeyboardInfo, MechanicalLayout, NumberPadPresence, PhysicalLayout, TopRightKey, TopRowKey} from 'chrome://diagnostics/input.mojom-webui.js';
import {InputCardElement, InputCardType} from 'chrome://diagnostics/input_card.js';
import {setInputDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

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

  /** @type {?FakeInputDataProvider} */
  let provider = null;

  setup(() => {
    document.body.innerHTML = '';

    provider = new FakeInputDataProvider();
    setInputDataProviderForTesting(provider);

    provider.setStartWithLidOpen();
    provider.setStartTesterWithClamshellMode();
  });

  teardown(() => {
    inputCardElement.remove();
    inputCardElement = null;
    provider.reset();
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
    inputCardElement.hostDeviceStatus = {
      isLidOpen: true,
      isTabletMode: false,
    };
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
        'Built-in keyboard',
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

  test('KeyboardTestabilityLidState', async () => {
    await initializeInputCard(InputCardType.KEYBOARD, keyboards);
    assertEquals(
        2,
        inputCardElement.shadowRoot.querySelector('dom-repeat').items.length);
    const elements = inputCardElement.root.querySelectorAll('.device');

    inputCardElement.hostDeviceStatus = {
      ...inputCardElement.hostDeviceStatus,
      isLidOpen: true,
    };
    await flushTasks();

    // Check that all keyboards start testable.
    assertEquals(
        keyboards[0].name, elements[0].querySelector('.device-name').innerText);
    assertEquals(
        'Built-in keyboard',
        elements[0].querySelector('.device-description').innerText);
    assertFalse(elements[0].querySelector('cr-button').disabled);
    assertTrue(elements[0].querySelector('#infoIcon').hidden);

    assertEquals(
        keyboards[1].name, elements[1].querySelector('.device-name').innerText);
    assertEquals(
        'Bluetooth keyboard',
        elements[1].querySelector('.device-description').innerText);
    assertFalse(elements[1].querySelector('cr-button').disabled);
    assertTrue(elements[1].querySelector('#infoIcon').hidden);

    // Check that internal keyboard is no longer testable after lid closing.
    inputCardElement.hostDeviceStatus = {
      ...inputCardElement.hostDeviceStatus,
      isLidOpen: false,
    };
    await flushTasks();

    assertEquals(
        keyboards[0].name, elements[0].querySelector('.device-name').innerText);
    assertEquals(
        'Built-in keyboard',
        elements[0].querySelector('.device-description').innerText);
    assertTrue(elements[0].querySelector('cr-button').disabled);
    assertFalse(elements[0].querySelector('#infoIcon').hidden);
    assertEquals(
        loadTimeData.getString('inputKeyboardUntestableLidClosedNote'),
        elements[0].querySelector('#tooltipText').innerText.trim());

    assertEquals(
        keyboards[1].name, elements[1].querySelector('.device-name').innerText);
    assertEquals(
        'Bluetooth keyboard',
        elements[1].querySelector('.device-description').innerText);
    assertFalse(elements[1].querySelector('cr-button').disabled);
    assertTrue(elements[1].querySelector('#infoIcon').hidden);

    // Check internal keyboard tester is re-enabled after lid opening.
    inputCardElement.hostDeviceStatus = {
      ...inputCardElement.hostDeviceStatus,
      isLidOpen: true,
    };
    await flushTasks();

    assertFalse(elements[1].querySelector('cr-button').disabled);
    assertTrue(elements[1].querySelector('#infoIcon').hidden);
  });

  test('KeyboardTestabilityTabletMode', async () => {
    await initializeInputCard(InputCardType.KEYBOARD, keyboards);
    assertEquals(
        2,
        inputCardElement.shadowRoot.querySelector('dom-repeat').items.length);
    const elements = inputCardElement.root.querySelectorAll('.device');

    inputCardElement.hostDeviceStatus = {
      ...inputCardElement.hostDeviceStatus,
      isTabletMode: false,
    };
    await flushTasks();

    // Check that all keyboards start testable.
    assertEquals(
        keyboards[0].name, elements[0].querySelector('.device-name').innerText);
    assertEquals(
        'Built-in keyboard',
        elements[0].querySelector('.device-description').innerText);
    assertFalse(elements[0].querySelector('cr-button').disabled);
    assertTrue(elements[0].querySelector('#infoIcon').hidden);

    assertEquals(
        keyboards[1].name, elements[1].querySelector('.device-name').innerText);
    assertEquals(
        'Bluetooth keyboard',
        elements[1].querySelector('.device-description').innerText);
    assertFalse(elements[1].querySelector('cr-button').disabled);
    assertTrue(elements[1].querySelector('#infoIcon').hidden);

    inputCardElement.hostDeviceStatus = {
      ...inputCardElement.hostDeviceStatus,
      isTabletMode: true,
    };
    await flushTasks();

    // Check that internal keyboard is no longer testable after switching to
    // tablet mode.
    assertEquals(
        keyboards[0].name, elements[0].querySelector('.device-name').innerText);
    assertEquals(
        'Built-in keyboard',
        elements[0].querySelector('.device-description').innerText);
    assertTrue(elements[0].querySelector('cr-button').disabled);
    assertFalse(elements[0].querySelector('#infoIcon').hidden);
    assertEquals(
        loadTimeData.getString('inputKeyboardUntestableTabletModeNote'),
        elements[0].querySelector('#tooltipText').innerText.trim());

    assertEquals(
        keyboards[1].name, elements[1].querySelector('.device-name').innerText);
    assertEquals(
        'Bluetooth keyboard',
        elements[1].querySelector('.device-description').innerText);
    assertFalse(elements[1].querySelector('cr-button').disabled);
    assertTrue(elements[1].querySelector('#infoIcon').hidden);

    // Check internal keyboard tester is re-enabled after ending tablet mode.
    inputCardElement.hostDeviceStatus = {
      ...inputCardElement.hostDeviceStatus,
      isTabletMode: false,
    };
    await flushTasks();

    assertFalse(elements[1].querySelector('cr-button').disabled);
    assertTrue(elements[1].querySelector('#infoIcon').hidden);
  });
});
