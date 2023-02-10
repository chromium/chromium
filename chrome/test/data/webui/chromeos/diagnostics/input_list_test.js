// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/input_list.js';
import 'chrome://diagnostics/strings.m.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {DiagnosticsBrowserProxyImpl} from 'chrome://diagnostics/diagnostics_browser_proxy.js';
import {NavigationView} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeKeyboards, fakeTouchDevices} from 'chrome://diagnostics/fake_data.js';
import {FakeInputDataProvider} from 'chrome://diagnostics/fake_input_data_provider.js';
import {ConnectionType, KeyboardInfo, MechanicalLayout, NumberPadPresence, PhysicalLayout, TopRightKey, TopRowKey} from 'chrome://diagnostics/input.mojom-webui.js';
import {InputCardElement} from 'chrome://diagnostics/input_card.js';
import {TouchDeviceInfo, TouchDeviceType} from 'chrome://diagnostics/input_data_provider.mojom-webui.js';
import {InputListElement} from 'chrome://diagnostics/input_list.js';
import {setInputDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {isVisible} from '../test_util.js';

import {TestDiagnosticsBrowserProxy} from './test_diagnostics_browser_proxy.js';

suite('inputListTestSuite', function() {
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
    DiagnosticsBrowserProxyImpl.setInstance(diagnosticsBrowserProxy);
  });

  setup(() => {
    document.body.innerHTML = '';

    provider.setStartTesterWithClamshellMode();
    provider.setStartWithLidOpen();
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
    const card = inputListElement.shadowRoot.querySelector(
        `input-card[device-type="${deviceType}"]`);
    return /** @type {!InputCardElement} */ (card);
  }

  /**
   * openTouchscreenTester is a helper function for some boilerplate code. It
   * clicks the test button of a touchscreen, then clicks the start testing
   * button and makes sure canvas dialog is open.
   * @param touchscreensToInitialize The touchscreens to initialize.
   * @param isClamshellMode If the tester is opened in clamshell mode.
   * @returns The touchscreenTester.
   */
  async function openTouchscreenTester(
      touchscreensToInitialize, isClamshellMode = true) {
    await initializeInputList([], touchscreensToInitialize);
    if (isClamshellMode) {
      provider.setStartTesterWithClamshellMode();
    } else {
      provider.setStartTesterWithTabletMode();
    }

    const resolver = new PromiseResolver();
    const touchscreenTester =
        inputListElement.shadowRoot.querySelector('touchscreen-tester');
    const introDialog = touchscreenTester.getDialog('intro-dialog');

    // Mock requestFullscreen function since this API can only be initiated by a
    // user gesture.
    introDialog.requestFullscreen = () => {
      resolver.resolve();
    };

    const testButton = getCardByDeviceType('touchscreen')
                           .shadowRoot.querySelector('cr-button');
    assertTrue(!!testButton);
    testButton.click();
    await flushTasks();
    assertTrue(introDialog.open);

    const getStartedButton = introDialog.querySelector('cr-button');
    getStartedButton.click();
    await flushTasks();
    const canvasDialog = touchscreenTester.getDialog('canvas-dialog');
    assertTrue(canvasDialog.open);

    return touchscreenTester;
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
    const testButton =
        getCardByDeviceType('keyboard').shadowRoot.querySelector('cr-button');
    assertTrue(!!testButton);
    testButton.click();
    await flushTasks();

    const keyboardTester =
        inputListElement.shadowRoot.querySelector('keyboard-tester');
    assertTrue(keyboardTester.isOpen());
  });

  test('KeyboardTesterCloseOnLidClosed', async () => {
    await initializeInputList([fakeKeyboards[0]], []);
    const testButton =
        getCardByDeviceType('keyboard').shadowRoot.querySelector('cr-button');
    assertTrue(!!testButton);
    testButton.click();
    await flushTasks();

    const keyboardTester =
        inputListElement.shadowRoot.querySelector('keyboard-tester');
    assertTrue(keyboardTester.isOpen());

    const showToastEvent = eventToPromise('show-toast', inputListElement);
    provider.setLidStateClosed();
    await flushTasks();
    assertFalse(keyboardTester.isOpen());

    const e = await showToastEvent;
    assertEquals(
        e.detail.message,
        loadTimeData.getString('inputKeyboardTesterClosedToastLidClosed'));
  });

  test('KeyboardTesterCloseOnTabletMode', async () => {
    await initializeInputList([fakeKeyboards[0]], []);
    const testButton =
        getCardByDeviceType('keyboard').shadowRoot.querySelector('cr-button');
    assertTrue(!!testButton);
    testButton.click();
    await flushTasks();

    const keyboardTester =
        inputListElement.shadowRoot.querySelector('keyboard-tester');
    assertTrue(keyboardTester.isOpen());

    const showToastEvent = eventToPromise('show-toast', inputListElement);
    provider.startTabletMode();
    await flushTasks();
    assertFalse(keyboardTester.isOpen());

    const e = await showToastEvent;
    assertEquals(
        e.detail.message,
        loadTimeData.getString('inputKeyboardTesterClosedToastTabletMode'));
  });

  test('ShowToastIfKeyboardDisconnectedDuringTest', async () => {
    await initializeInputList([fakeKeyboards[0]], []);
    const testButton =
        getCardByDeviceType('keyboard').shadowRoot.querySelector('cr-button');
    assertTrue(!!testButton);
    testButton.click();
    await flushTasks();

    const keyboardTester =
        inputListElement.shadowRoot.querySelector('keyboard-tester');
    assertTrue(keyboardTester.isOpen());
    const showToastEvent = eventToPromise('show-toast', inputListElement);
    // Remove keyboard while tester is open.
    provider.removeFakeConnectedKeyboardById(fakeKeyboards[0].id);
    await flushTasks();
    // Verify that the custom event was dispatched
    const e = await showToastEvent;
    assertEquals(
        e.detail.message, loadTimeData.getString('deviceDisconnected'));

    // Verify that tester is closed.
    assertFalse(keyboardTester.isOpen());

    // Verify that key events are no longer blocked by keyboard-tester.
    let keyEventReceived = false;
    inputListElement.addEventListener('keydown', () => keyEventReceived = true);
    const keyDownEvent = eventToPromise('keydown', inputListElement);
    keyboardTester.dispatchEvent(new KeyboardEvent(
        'keydown', {bubbles: true, key: 'A', composed: true}));
    await keyDownEvent;
    assertTrue(keyEventReceived);
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

  test('TouchpadTesterShowAndClose', async () => {
    await initializeInputList([], [fakeTouchDevices[0]]);
    const touchpadTester =
        inputListElement.shadowRoot.querySelector('touchpad-tester');
    assertTrue(!!touchpadTester);
    assertFalse(touchpadTester.isOpen());

    const testButton =
        getCardByDeviceType('touchpad').shadowRoot.querySelector('cr-button');
    assertTrue(!!testButton);
    testButton.click();
    await flushTasks();

    assertTrue(touchpadTester.isOpen());
    assertDeepEquals(fakeTouchDevices[0], touchpadTester.touchpad);
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
    assertEquals(3, touchscreenCard.devices.length);
    assertEquals(fakeTouchDevices[1].name, touchscreenCard.devices[0].name);
    assertEquals(fakeTouchscreen.name, touchscreenCard.devices[2].name);

    provider.removeFakeConnectedTouchDeviceById(fakeTouchscreen.id);
    await flushTasks();
    assertEquals(2, touchscreenCard.devices.length);
    assertEquals(fakeTouchDevices[1].name, touchscreenCard.devices[0].name);
  });

  test('TouchscreenTesterShowAndClose', async () => {
    await initializeInputList([], [fakeTouchDevices[1]]);
    provider.setStartTesterWithClamshellMode();

    const resolver = new PromiseResolver();
    let requestFullscreenCalled = 0;

    const touchscreenTester =
        inputListElement.shadowRoot.querySelector('touchscreen-tester');
    const introDialog = touchscreenTester.getDialog('intro-dialog');

    // Mock requestFullscreen function since this API can only be initiated by a
    // user gesture.
    introDialog.requestFullscreen = () => {
      requestFullscreenCalled++;
      resolver.resolve();
    };

    let eventTrackerRemoveAllCalled = 0;
    touchscreenTester.getEventTracker().removeAll = () => {
      eventTrackerRemoveAllCalled++;
      resolver.resolve();
    };

    // A11y touch passthrough state is false by default.
    assertFalse(provider.getA11yTouchPassthroughState());

    const testButton = getCardByDeviceType('touchscreen')
                           .shadowRoot.querySelector('cr-button');
    assertTrue(!!testButton);
    testButton.click();
    await flushTasks();
    assertEquals(1, requestFullscreenCalled);
    assertTrue(introDialog.open);
    assertEquals(
        /*expectedMoveAppToTestingScreenCalled=*/ 1,
        provider.getMoveAppToTestingScreenCalled());

    // Click get stated button to open canvas dialog.
    const getStartedButton = introDialog.querySelector('cr-button');
    getStartedButton.click();
    await flushTasks();
    assertFalse(introDialog.open);
    assertTrue(provider.getA11yTouchPassthroughState());

    const canvasDialog = touchscreenTester.getDialog('canvas-dialog');
    assertTrue(canvasDialog.open);

    const fullscreenChangeEvent = eventToPromise('fullscreenchange', document);
    document.dispatchEvent(new Event('fullscreenchange'));
    await fullscreenChangeEvent;

    assertFalse(canvasDialog.open);
    assertEquals(
        /*expectedMoveAppBackToPreviousScreenCalled=*/ 1,
        provider.getMoveAppBackToPreviousScreenCalled());
    assertEquals(
        /*expectedEventTrackerRemoveAllCalled=*/ 1,
        eventTrackerRemoveAllCalled);
    assertFalse(provider.getA11yTouchPassthroughState());
  });

  test('TouchscreenTesterShowAndCloseInTabletMode', async () => {
    await openTouchscreenTester(
        /*touchscreensToInitialize=*/[fakeTouchDevices[1]]);

    const resolver = new PromiseResolver();
    let exitFullscreenCalled = 0;
    // Mock exitFullscreen call.
    document.exitFullscreen = () => {
      exitFullscreenCalled++;
      resolver.resolve();
    };

    provider.startTabletMode();
    await flushTasks();

    const keyEvent = eventToPromise('keydown', window);
    window.dispatchEvent(new KeyboardEvent('keydown', {key: 'AudioVolumeUp'}));
    await keyEvent;
    assertEquals(1, exitFullscreenCalled);
  });

  test('StartTouchscreenTesterWithClamshellMode', async () => {
    const touchscreenTester = await openTouchscreenTester(
        /*touchscreensToInitialize=*/[fakeTouchDevices[1]]);

    assertFalse(touchscreenTester.getIsTabletMode());

    provider.startTabletMode();
    await flushTasks();

    assertTrue(touchscreenTester.getIsTabletMode());
    assertTrue(touchscreenTester.getDialog('canvas-dialog').open);
  });

  test('StartTouchscreenTesterWithTabletMode', async () => {
    const touchscreenTester = await openTouchscreenTester(
        /*touchscreensToInitialize=*/[fakeTouchDevices[1]],
        /*isClamshellMode=*/ false);

    assertTrue(touchscreenTester.getIsTabletMode());

    provider.endTabletMode();
    await flushTasks();

    assertFalse(touchscreenTester.getIsTabletMode());
    assertTrue(touchscreenTester.getDialog('canvas-dialog').open);
  });

  test('OnInternalDisplayPowerStateChanged', async () => {
    await initializeInputList([], [fakeTouchDevices[1]]);

    assertTrue(getCardByDeviceType('touchscreen').devices[0].testable);

    provider.setInternalDisplayPowerOff();
    await flushTasks();

    assertFalse(getCardByDeviceType('touchscreen').devices[0].testable);

    provider.setInternalDisplayPowerOn();
    await flushTasks();

    assertTrue(getCardByDeviceType('touchscreen').devices[0].testable);
  });

  test('InternalDisplayPowerOffWhileTouchscreenTesterIsRunning', async () => {
    const touchscreenTester = await openTouchscreenTester(
        /*touchscreensToInitialize=*/[fakeTouchDevices[1]]);

    // Internal display power off.
    provider.setInternalDisplayPowerOff();
    await flushTasks();

    // Tester is expecetd to exit.
    assertFalse(touchscreenTester.getDialog('canvas-dialog').open);
  });

  test('TouchscreenDisconnectedWhileTesterIsRunning', async () => {
    const touchscreenTester = await openTouchscreenTester(
        /*touchscreensToInitialize=*/[fakeTouchDevices[1]]);

    // Remove touchscreen.
    provider.removeFakeConnectedTouchDeviceById(fakeTouchDevices[1].id);
    await flushTasks();

    // Tester is expecetd to exit.
    assertFalse(touchscreenTester.getDialog('canvas-dialog').open);
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

    diagnosticsBrowserProxy.setPreviousView(NavigationView.SYSTEM);
    inputListElement.onNavigationPageChanged({isActive: true});

    await flushTasks();
    assertEquals(1, diagnosticsBrowserProxy.getCallCount('recordNavigation'));
    assertArrayEquals(
        [NavigationView.SYSTEM, NavigationView.INPUT],
        /** @type {!Array<!NavigationView>} */
        (diagnosticsBrowserProxy.getArgs('recordNavigation')[0]));
  });
});
