// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/input_list.js';

import {DiagnosticsBrowserProxyImpl} from 'chrome://diagnostics/diagnostics_browser_proxy.js';
import {ConnectionType, KeyboardInfo, MechanicalLayout, NavigationView, NumberPadPresence, PhysicalLayout, TopRightKey, TopRowKey, TouchDeviceInfo, TouchDeviceType} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeKeyboards, fakeTouchDevices} from 'chrome://diagnostics/fake_data.js';
import {FakeInputDataProvider} from 'chrome://diagnostics/fake_input_data_provider.js';
import {setInputDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';

import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.js';

import {TestDiagnosticsBrowserProxy} from './test_diagnostics_browser_proxy.js';

export function inputListTestSuite() {
  /** @type {?InputListElement} */
  let inputListElement = null;

  /** @type {?FakeInputDataProvider} */
  let provider = null;

  /** @type {?TestDiagnosticsBrowserProxy} */
  let diagnosticsBrowserProxy = null;

  suiteSetup(() => {
    provider = new FakeInputDataProvider();
    setInputDataProviderForTesting(provider);

    diagnosticsBrowserProxy = new TestDiagnosticsBrowserProxy();
    DiagnosticsBrowserProxyImpl.instance_ = diagnosticsBrowserProxy;
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

  test('InputListPopulated', async () => {
    await initializeInputList();
    assertEquals(
        fakeKeyboards[0].name, getCardByDeviceType('keyboard').devices[0].name);
    assertEquals(
        fakeTouchDevices[0].name,
        getCardByDeviceType('touchpad').devices[0].name);
    assertEquals(
        fakeTouchDevices[1].name,
        getCardByDeviceType('touchscreen').devices[0].name);
  });

  test('KeyboardAddAndRemove', async () => {
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
      topRightKey: TopRightKey.kUnknown,
    };
    await initializeInputList();
    const keyboardCard = getCardByDeviceType('keyboard');

    provider.addFakeConnectedKeyboard(fakeKeyboard);
    await flushTasks();
    assertEquals(2, keyboardCard.devices.length);
    assertEquals(fakeKeyboards[0].name, keyboardCard.devices[0].name);
    assertEquals(fakeKeyboard.name, keyboardCard.devices[1].name);

    provider.removeFakeConnectedKeyboardById(fakeKeyboard.id);
    await flushTasks();
    assertEquals(1, keyboardCard.devices.length);
    assertEquals(fakeKeyboards[0].name, keyboardCard.devices[0].name);
  });

  test('KeyboardTesterShow', async () => {
    await initializeInputList([fakeKeyboards[0]], []);
    const testButton = getCardByDeviceType('keyboard').$$('cr-button');
    assertTrue(!!testButton);
    testButton.click();
    await flushTasks();

    const keyboardTester = inputListElement.$$('keyboard-tester');
    assertTrue(keyboardTester.isOpen());
  });

  test('TouchpadAddAndRemove', async () => {
    /** @type {!TouchDeviceInfo} */
    const fakeTouchpad = {
      id: 4,
      connectionType: ConnectionType.kUsb,
      type: TouchDeviceType.kPointer,
      name: 'Sample USB touchpad',
    };
    await initializeInputList();
    const touchpadCard = getCardByDeviceType('touchpad');

    provider.addFakeConnectedTouchDevice(fakeTouchpad);
    await flushTasks();
    assertEquals(2, touchpadCard.devices.length);
    assertEquals(fakeTouchDevices[0].name, touchpadCard.devices[0].name);
    assertEquals(fakeTouchpad.name, touchpadCard.devices[1].name);

    provider.removeFakeConnectedTouchDeviceById(fakeTouchpad.id);
    await flushTasks();
    assertEquals(1, touchpadCard.devices.length);
    assertEquals(fakeTouchDevices[0].name, touchpadCard.devices[0].name);
  });

  test('TouchscreenAddAndRemove', async () => {
    /** @type {!TouchDeviceInfo} */
    const fakeTouchscreen = {
      id: 4,
      connectionType: ConnectionType.kUsb,
      type: TouchDeviceType.kDirect,
      name: 'Sample USB touchscreen',
    };
    await initializeInputList();
    const touchscreenCard = getCardByDeviceType('touchscreen');

    provider.addFakeConnectedTouchDevice(fakeTouchscreen);
    await flushTasks();
    assertEquals(2, touchscreenCard.devices.length);
    assertEquals(fakeTouchDevices[1].name, touchscreenCard.devices[0].name);
    assertEquals(fakeTouchscreen.name, touchscreenCard.devices[1].name);

    provider.removeFakeConnectedTouchDeviceById(fakeTouchscreen.id);
    await flushTasks();
    assertEquals(1, touchscreenCard.devices.length);
    assertEquals(fakeTouchDevices[1].name, touchscreenCard.devices[0].name);
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

  test('RecordNavigationCalled', async () => {
    await initializeInputList();
    inputListElement.onNavigationPageChanged({isActive: false});
    await flushTasks();

    assertEquals(0, diagnosticsBrowserProxy.getCallCount('recordNavigation'));

    diagnosticsBrowserProxy.setPreviousView(NavigationView.kSystem);
    inputListElement.onNavigationPageChanged({isActive: true});

    await flushTasks();
    assertEquals(1, diagnosticsBrowserProxy.getCallCount('recordNavigation'));
    assertArrayEquals(
        [NavigationView.kSystem, NavigationView.kInput],
        /** @type {!Array<!NavigationView>} */
        (diagnosticsBrowserProxy.getArgs('recordNavigation')[0]));
  });
}
