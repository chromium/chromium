// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/input_list.js';

import {ConnectionType, KeyboardInfo, MechanicalLayout, NumberPadPresence, PhysicalLayout, TopRowKey, TouchDeviceInfo, TouchDeviceType} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeKeyboards, fakeTouchDevices} from 'chrome://diagnostics/fake_data.js';
import {FakeInputDataProvider} from 'chrome://diagnostics/fake_input_data_provider.js';
import {setInputDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function inputListTestSuite() {
  /** @type {?InputListElement} */
  let inputListElement = null;

  /** @type {?FakeInputDataProvider} */
  let provider = null;

  suiteSetup(() => {
    provider = new FakeInputDataProvider();
    setInputDataProviderForTesting(provider);
  });

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    inputListElement.remove();
    inputListElement = null;
    provider.reset();
  });

  function initializeInputList(
      keyboards = fakeKeyboards, touchDevices = fakeTouchDevices) {
    assertFalse(!!inputListElement);
    provider.setFakeConnectedDevices(keyboards, touchDevices);

    // Add the input list to the DOM.
    inputListElement =
        /** @type {!InputListElement} */ (document.createElement('input-list'));
    assertTrue(!!inputListElement);
    document.body.appendChild(inputListElement);

    return flushTasks();
  }

  /** @return {!InputCardElement} */
  function getCardByDeviceType(deviceType) {
    const card = inputListElement.$$(`input-card[device-type="${deviceType}"]`);
    return /** @type {!InputCardElement} */ (card);
  }

  /**
   * Returns whether the element both exists and is visible.
   * @param {?Element} element
   * @return {boolean}
   */
  function isVisible(element) {
    return !!element && element.style.display !== 'none';
  }

  test('InputListPopulated', () => {
    return initializeInputList().then(() => {
      assertEquals(
          fakeKeyboards[0].name,
          getCardByDeviceType('keyboard').devices[0].name);
      assertEquals(
          fakeTouchDevices[0].name,
          getCardByDeviceType('touchpad').devices[0].name);
      assertEquals(
          fakeTouchDevices[1].name,
          getCardByDeviceType('touchscreen').devices[0].name);
    });
  });

  test('KeyboardAddAndRemove', () => {
    /** @type {!KeyboardInfo} */
    const fakeKeyboard = {
      id: 4,
      connectionType: ConnectionType.kUsb,
      name: 'Sample USB keyboard',
      physicalLayout: PhysicalLayout.kUnknown,
      mechanicalLayout: MechanicalLayout.kUnknown,
      hasAssistantKey: false,
      numberPadPresent: NumberPadPresence.kUnknown,
      topRowKeys: [
        TopRowKey.kBack, TopRowKey.kForward, TopRowKey.kRefresh,
        TopRowKey.kFullscreen, TopRowKey.kOverview,
        TopRowKey.kScreenBrightnessDown, TopRowKey.kScreenBrightnessUp,
        TopRowKey.kVolumeMute, TopRowKey.kVolumeDown, TopRowKey.kVolumeUp
      ],
    };
    let keyboardCard;
    return initializeInputList()
        .then(() => {
          keyboardCard = getCardByDeviceType('keyboard');
          provider.addFakeConnectedKeyboard(fakeKeyboard);
          return flushTasks();
        })
        .then(() => {
          assertEquals(2, keyboardCard.devices.length);
          assertEquals(fakeKeyboards[0].name, keyboardCard.devices[0].name);
          assertEquals(fakeKeyboard.name, keyboardCard.devices[1].name);
          provider.removeFakeConnectedKeyboardById(fakeKeyboard.id);
          return flushTasks();
        })
        .then(() => {
          assertEquals(1, keyboardCard.devices.length);
          assertEquals(fakeKeyboards[0].name, keyboardCard.devices[0].name);
        });
  });

  test('KeyboardTesterShow', () => {
    return initializeInputList([fakeKeyboards[0]], [])
        .then(() => {
          const testButton = getCardByDeviceType('keyboard').$$('cr-button');
          assertTrue(!!testButton);
          testButton.click();
          return flushTasks();
        })
        .then(() => {
          const keyboardTester = inputListElement.$$('keyboard-tester');
          assertTrue(keyboardTester.isOpen());
        });
  });

  test('TouchpadAddAndRemove', () => {
    /** @type {!TouchDeviceInfo} */
    const fakeTouchpad = {
      id: 4,
      connectionType: ConnectionType.kUsb,
      type: TouchDeviceType.kPointer,
      name: 'Sample USB touchpad',
    };
    let touchpadCard;
    return initializeInputList()
        .then(() => {
          touchpadCard = getCardByDeviceType('touchpad');
          provider.addFakeConnectedTouchDevice(fakeTouchpad);
          return flushTasks();
        })
        .then(() => {
          assertEquals(2, touchpadCard.devices.length);
          assertEquals(fakeTouchDevices[0].name, touchpadCard.devices[0].name);
          assertEquals(fakeTouchpad.name, touchpadCard.devices[1].name);
          provider.removeFakeConnectedTouchDeviceById(fakeTouchpad.id);
          return flushTasks();
        })
        .then(() => {
          assertEquals(1, touchpadCard.devices.length);
          assertEquals(fakeTouchDevices[0].name, touchpadCard.devices[0].name);
        });
  });

  test('TouchscreenAddAndRemove', () => {
    /** @type {!TouchDeviceInfo} */
    const fakeTouchscreen = {
      id: 4,
      connectionType: ConnectionType.kUsb,
      type: TouchDeviceType.kDirect,
      name: 'Sample USB touchscreen',
    };
    let touchscreenCard;
    return initializeInputList()
        .then(() => {
          touchscreenCard = getCardByDeviceType('touchscreen');
          provider.addFakeConnectedTouchDevice(fakeTouchscreen);
          return flushTasks();
        })
        .then(() => {
          assertEquals(2, touchscreenCard.devices.length);
          assertEquals(
              fakeTouchDevices[1].name, touchscreenCard.devices[0].name);
          assertEquals(fakeTouchscreen.name, touchscreenCard.devices[1].name);
          provider.removeFakeConnectedTouchDeviceById(fakeTouchscreen.id);
          return flushTasks();
        })
        .then(() => {
          assertEquals(1, touchscreenCard.devices.length);
          assertEquals(
              fakeTouchDevices[1].name, touchscreenCard.devices[0].name);
        });
  });

  test('EmptySectionsHidden', async () => {
    await initializeInputList([], []);
    assertFalse(isVisible(getCardByDeviceType('keyboard')));
    assertFalse(isVisible(getCardByDeviceType('touchpad')));
    assertFalse(isVisible(getCardByDeviceType('touchscreen')));

    provider.addFakeConnectedTouchDevice(fakeTouchDevices[1]);
    await flushTasks();
    assertTrue(!!getCardByDeviceType('touchscreen'));
    assertFalse(isVisible(getCardByDeviceType('keyboard')));
    assertFalse(isVisible(getCardByDeviceType('touchpad')));
    assertTrue(isVisible(getCardByDeviceType('touchscreen')));

    provider.removeFakeConnectedTouchDeviceById(fakeTouchDevices[1].id);
    provider.addFakeConnectedTouchDevice(fakeTouchDevices[0]);
    await flushTasks();
    assertFalse(isVisible(getCardByDeviceType('keyboard')));
    assertTrue(isVisible(getCardByDeviceType('touchpad')));
    assertFalse(isVisible(getCardByDeviceType('touchscreen')));

    provider.removeFakeConnectedTouchDeviceById(fakeTouchDevices[0].id);
    provider.addFakeConnectedKeyboard(fakeKeyboards[0]);
    await flushTasks();
    assertTrue(isVisible(getCardByDeviceType('keyboard')));
    assertFalse(isVisible(getCardByDeviceType('touchpad')));
    assertFalse(isVisible(getCardByDeviceType('touchscreen')));

    provider.removeFakeConnectedKeyboardById(fakeKeyboards[0].id);
    await flushTasks();
    assertFalse(isVisible(getCardByDeviceType('keyboard')));
    assertFalse(isVisible(getCardByDeviceType('touchpad')));
    assertFalse(isVisible(getCardByDeviceType('touchscreen')));
  });
}
