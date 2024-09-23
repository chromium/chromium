// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/input_list.js';
import 'chrome://diagnostics/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {DiagnosticsBrowserProxyImpl} from 'chrome://diagnostics/diagnostics_browser_proxy.js';
import {NavigationView} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeKeyboards, fakeTouchDevices} from 'chrome://diagnostics/fake_data.js';
import {FakeInputDataProvider} from 'chrome://diagnostics/fake_input_data_provider.js';
import {ConnectionType, KeyboardInfo, MechanicalLayout, NumberPadPresence, PhysicalLayout, TopRightKey, TopRowKey} from 'chrome://diagnostics/input.mojom-webui.js';
import {InputCardElement} from 'chrome://diagnostics/input_card.js';
import {TouchDeviceInfo, TouchDeviceType} from 'chrome://diagnostics/input_data_provider.mojom-webui.js';
import {InputListElement} from 'chrome://diagnostics/input_list.js';
import {KeyboardTesterElement} from 'chrome://diagnostics/keyboard_tester.js';
import {setInputDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertArrayEquals, assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestDiagnosticsBrowserProxy} from './test_diagnostics_browser_proxy.js';

suite('inputListTestSuite', function() {
  let inputListElement: InputListElement|null = null;

  const provider = new FakeInputDataProvider();

  const diagnosticsBrowserProxy = new TestDiagnosticsBrowserProxy();


  suiteSetup(() => {
    setInputDataProviderForTesting(provider);

    DiagnosticsBrowserProxyImpl.setInstance(diagnosticsBrowserProxy);
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    provider.setStartTesterWithClamshellMode();
    provider.setStartWithLidOpen();
  });

  teardown(() => {
    inputListElement?.remove();
    inputListElement = null;
    provider.reset();
  });

  function initializeInputList(
      keyboards = fakeKeyboards, touchDevices = fakeTouchDevices) {
    assertFalse(!!inputListElement);
    provider.setFakeConnectedDevices(keyboards, touchDevices);

    // Add the input list to the DOM.
    inputListElement = document.createElement('input-list');
    assert(inputListElement);
    document.body.appendChild(inputListElement);

    return flushTasks();
  }

  function getCardByDeviceType(deviceType: string): InputCardElement|null {
    assert(inputListElement);
    return inputListElement.shadowRoot!.querySelector<InputCardElement>(
        `input-card[device-type="${deviceType}"]`);
  }

  /**
   * openTouchscreenTester is a helper function for some boilerplate code. It
   * clicks the test button of a touchscreen, then clicks the start testing
   * button and makes sure canvas dialog is open.
   * @returns The touchscreenTester.
   */
  async function openTouchscreenTester(
      touchscreensToInitialize: TouchDeviceInfo[], isClamshellMode = true) {
    await initializeInputList([], touchscreensToInitialize);
    if (isClamshellMode) {
      provider.setStartTesterWithClamshellMode();
    } else {
      provider.setStartTesterWithTabletMode();
    }

    const resolver = new PromiseResolver();
    assert(inputListElement);
    const touchscreenTester =
        inputListElement!.shadowRoot!.querySelector('touchscreen-tester');
    assert(touchscreenTester);
    const introDialog =
        touchscreenTester.getDialog('intro-dialog') as CrDialogElement;

    // Mock requestFullscreen function since this API can only be initiated by a
    // user gesture.
    introDialog.requestFullscreen = (): any => {
      resolver.resolve(undefined);
    };

    const testButton = strictQuery(
        'cr-button', getCardByDeviceType('touchscreen')!.shadowRoot,
        CrButtonElement);
    assertTrue(!!testButton);
    testButton.click();
    await flushTasks();
    assertTrue(introDialog.open);

    const getStartedButton =
        strictQuery('cr-button', introDialog, CrButtonElement);
    getStartedButton.click();
    await flushTasks();
    assert(touchscreenTester);
    const canvasDialog = touchscreenTester.getDialog('canvas-dialog');
    assertTrue(canvasDialog.open);

    return touchscreenTester;
  }

  test('InputListPopulated', async () => {
    await initializeInputList();
    assertEquals(
        fakeKeyboards[0]!.name,
        getCardByDeviceType('keyboard')!.devices[0]!.name);
    assertEquals(
        fakeTouchDevices[0]!.name,
        getCardByDeviceType('touchpad')!.devices[0]!.name);
    assertEquals(
        fakeTouchDevices[1]!.name,
        getCardByDeviceType('touchscreen')!.devices[0]!.name);
  });

  test('KeyboardAddAndRemove', async () => {
    const fakeKeyboard: KeyboardInfo = {
      id: 4,
      connectionType: ConnectionType.kUsb,
      name: 'Sample USB keyboard',
      regionCode: 'US',
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
    assert(keyboardCard);
    provider.addFakeConnectedKeyboard(fakeKeyboard);
    await flushTasks();
    assertEquals(2, keyboardCard.devices.length);
    assertEquals(fakeKeyboards[0]!.name, keyboardCard.devices[0]!.name);
    assertEquals(fakeKeyboard.name, keyboardCard.devices[1]!.name);

    provider.removeFakeConnectedKeyboardById(fakeKeyboard.id);
    await flushTasks();
    assertEquals(1, keyboardCard.devices.length);
    assertEquals(fakeKeyboards[0]!.name, keyboardCard.devices[0]!.name);
  });

  test('KeyboardTesterShow', async () => {
    const keyboardInfo: KeyboardInfo|undefined = fakeKeyboards[0];
    assert(keyboardInfo);
    await initializeInputList([keyboardInfo], []);
    const testButton = strictQuery(
        'cr-button', getCardByDeviceType('keyboard')!.shadowRoot,
        CrButtonElement);
    assert(testButton);
    testButton.click();
    await flushTasks();
    assert(inputListElement);
    const keyboardTester = strictQuery(
        'keyboard-tester', inputListElement.shadowRoot, KeyboardTesterElement);
    assert(keyboardTester);
    assertTrue(keyboardTester.isOpen());
  });

  test('KeyboardTesterShowDirectlyWithOneKeyboard', async () => {
    const params = new URLSearchParams(window.location.search);
    params.set('showDefaultKeyboardTester', '');
    window.history.replaceState(
        null, '', `${window.location.pathname}?${params.toString()}`);

    const keyboardInfo: KeyboardInfo|undefined = fakeKeyboards[0];
    assert(keyboardInfo);
    await initializeInputList([keyboardInfo], []);
    await flushTasks();
    assert(inputListElement);
    const keyboardTester = strictQuery(
        'keyboard-tester', inputListElement.shadowRoot, KeyboardTesterElement);
    assert(keyboardTester);
    assertTrue(keyboardTester.isOpen());
  });

  test('KeyboardTesterShowDirectlyWithNoKeyboard', async () => {
    const params = new URLSearchParams(window.location.search);
    params.set('showDefaultKeyboardTester', '');
    window.history.replaceState(
        null, '', `${window.location.pathname}?${params.toString()}`);

    await initializeInputList([], []);
    assert(inputListElement);
    await flushTasks();
    const keyboardTester = strictQuery(
        'keyboard-tester', inputListElement.shadowRoot, KeyboardTesterElement);
    assert(keyboardTester);
    assertFalse(keyboardTester.isOpen());
  });

  test('KeyboardTesterCloseOnLidClosed', async () => {
    const keyboardInfo: KeyboardInfo|undefined = fakeKeyboards[0];
    assert(keyboardInfo);
    await initializeInputList([keyboardInfo], []);
    const testButton = strictQuery(
        'cr-button', getCardByDeviceType('keyboard')!.shadowRoot,
        CrButtonElement);
    assertTrue(!!testButton);
    testButton.click();
    await flushTasks();
    assert(inputListElement);
    const keyboardTester = strictQuery(
        'keyboard-tester', inputListElement.shadowRoot, KeyboardTesterElement);
    assert(keyboardTester);
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
    const keyboardInfo: KeyboardInfo|undefined = fakeKeyboards[0];
    assert(keyboardInfo);
    await initializeInputList([keyboardInfo], []);
    const testButton = strictQuery(
        'cr-button', getCardByDeviceType('keyboard')!.shadowRoot,
        CrButtonElement);
    assert(testButton);
    testButton.click();
    await flushTasks();
    assert(inputListElement);
    const keyboardTester = strictQuery(
        'keyboard-tester', inputListElement.shadowRoot, KeyboardTesterElement);
    assert(keyboardTester);
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
    const keyboardInfo: KeyboardInfo|undefined = fakeKeyboards[0];
    assert(keyboardInfo);
    await initializeInputList([keyboardInfo], []);
    const testButton = strictQuery(
        'cr-button', getCardByDeviceType('keyboard')!.shadowRoot,
        CrButtonElement);
    assert(testButton);
    testButton.click();
    await flushTasks();
    assert(inputListElement);
    const keyboardTester = strictQuery(
        'keyboard-tester', inputListElement.shadowRoot, KeyboardTesterElement);
    assert(keyboardTester);
    assertTrue(keyboardTester.isOpen());
    const showToastEvent = eventToPromise('show-toast', inputListElement);
    // Remove keyboard while tester is open.
    provider.removeFakeConnectedKeyboardById(keyboardInfo.id);
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
    const fakeTouchpad: TouchDeviceInfo = {
      id: 4,
      connectionType: ConnectionType.kUsb,
      type: TouchDeviceType.kPointer,
      name: 'Sample USB touchpad',
      testable: true,
    };
    const touchDevice: TouchDeviceInfo|undefined = fakeTouchDevices[0];
    assert(touchDevice);
    await initializeInputList();
    const touchpadCard = getCardByDeviceType('touchpad');
    assert(touchpadCard);
    provider.addFakeConnectedTouchDevice(fakeTouchpad);
    await flushTasks();
    assertEquals(2, touchpadCard.devices.length);
    assertEquals(touchDevice.name, touchpadCard.devices[0]!.name);
    assertEquals(fakeTouchpad.name, touchpadCard.devices[1]!.name);

    provider.removeFakeConnectedTouchDeviceById(fakeTouchpad.id);
    await flushTasks();
    assertEquals(1, touchpadCard.devices.length);
    assertEquals(touchDevice.name, touchpadCard.devices[0]!.name);
  });

  test('TouchpadTesterShowAndClose', async () => {
    const touchDevice: TouchDeviceInfo|undefined = fakeTouchDevices[0];
    assert(touchDevice);
    await initializeInputList([], [touchDevice]);
    assert(inputListElement);
    const touchpadTester =
        inputListElement!.shadowRoot!.querySelector('touchpad-tester');
    assert(touchpadTester);
    assertFalse(touchpadTester.isOpen());

    const testButton = strictQuery(
        'cr-button', getCardByDeviceType('touchpad')!.shadowRoot,
        CrButtonElement);
    assert(testButton);
    testButton.click();
    await flushTasks();

    assertTrue(touchpadTester.isOpen());
    assertDeepEquals(touchDevice, touchpadTester.touchpad);
  });

  test('TouchscreenAddAndRemove', async () => {
    const fakeTouchscreen: TouchDeviceInfo = {
      id: 4,
      connectionType: ConnectionType.kUsb,
      type: TouchDeviceType.kDirect,
      name: 'Sample USB touchscreen',
      testable: true,
    };
    await initializeInputList();
    const touchscreenCard = getCardByDeviceType('touchscreen');
    assert(touchscreenCard);
    provider.addFakeConnectedTouchDevice(fakeTouchscreen);
    await flushTasks();
    const touchDevice: TouchDeviceInfo|undefined = fakeTouchDevices[1];
    assert(touchDevice);
    assertEquals(3, touchscreenCard.devices.length);
    assertEquals(touchDevice.name, touchscreenCard.devices[0]!.name);
    assertEquals(fakeTouchscreen.name, touchscreenCard.devices[2]!.name);

    provider.removeFakeConnectedTouchDeviceById(fakeTouchscreen.id);
    await flushTasks();
    assertEquals(2, touchscreenCard.devices.length);
    assertEquals(touchDevice.name, touchscreenCard.devices[0]!.name);
  });

  test('TouchscreenTesterShowAndClose', async () => {
    const touchDevice: TouchDeviceInfo|undefined = fakeTouchDevices[1];
    assert(touchDevice);
    await initializeInputList([], [touchDevice]);
    provider.setStartTesterWithClamshellMode();

    const resolver = new PromiseResolver();
    let requestFullscreenCalled = 0;
    assert(inputListElement);
    const touchscreenTester =
        inputListElement!.shadowRoot!.querySelector('touchscreen-tester');
    assert(touchscreenTester);
    const introDialog =
        touchscreenTester.getDialog('intro-dialog') as CrDialogElement;

    // Mock requestFullscreen function since this API can only be initiated by a
    // user gesture.
    introDialog.requestFullscreen = (): any => {
      requestFullscreenCalled++;
      resolver.resolve(undefined);
    };

    let eventTrackerRemoveAllCalled = 0;
    touchscreenTester.getEventTracker().removeAll = () => {
      eventTrackerRemoveAllCalled++;
      resolver.resolve(undefined);
    };

    // A11y touch passthrough state is false by default.
    assertFalse(provider.getA11yTouchPassthroughState());

    const testButton = strictQuery(
        'cr-button', getCardByDeviceType('touchscreen')!.shadowRoot,
        CrButtonElement);
    assert(testButton);
    testButton.click();
    await flushTasks();
    assertEquals(1, requestFullscreenCalled);
    assertTrue(introDialog.open);
    assertEquals(
        /*expectedMoveAppToTestingScreenCalled=*/ 1,
        provider.getMoveAppToTestingScreenCalled());

    // Click get stated button to open canvas dialog.
    const getStartedButton =
        introDialog.querySelector('cr-button') as CrButtonElement;
    getStartedButton.click();
    await flushTasks();
    assertFalse(introDialog.open);
    assertTrue(provider.getA11yTouchPassthroughState());

    const canvasDialog =
        touchscreenTester.getDialog('canvas-dialog') as CrDialogElement;
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
    const touchDevice: TouchDeviceInfo|undefined = fakeTouchDevices[1];
    assert(touchDevice);
    await openTouchscreenTester(
        /*touchscreensToInitialize=*/[touchDevice]);

    const resolver = new PromiseResolver();
    let exitFullscreenCalled = 0;
    // Mock exitFullscreen call.
    document.exitFullscreen = (): any => {
      exitFullscreenCalled++;
      resolver.resolve(undefined);
    };

    provider.startTabletMode();
    await flushTasks();

    const keyEvent = eventToPromise('keydown', window);
    window.dispatchEvent(new KeyboardEvent('keydown', {key: 'AudioVolumeUp'}));
    await keyEvent;
    assertEquals(1, exitFullscreenCalled);
  });

  test('StartTouchscreenTesterWithClamshellMode', async () => {
    const touchDevice: TouchDeviceInfo|undefined = fakeTouchDevices[1];
    assert(touchDevice);
    const touchscreenTester = await openTouchscreenTester(
        /*touchscreensToInitialize=*/[touchDevice]);
    assert(touchscreenTester);
    assertFalse(touchscreenTester.getIsTabletMode());

    provider.startTabletMode();
    await flushTasks();

    assertTrue(touchscreenTester.getIsTabletMode());
    assertTrue(
        (touchscreenTester.getDialog('canvas-dialog') as CrDialogElement).open);
  });

  test('StartTouchscreenTesterWithTabletMode', async () => {
    const touchDevice: TouchDeviceInfo|undefined = fakeTouchDevices[1];
    assert(touchDevice);
    const touchscreenTester = await openTouchscreenTester(
        /*touchscreensToInitialize=*/[touchDevice],
        /*isClamshellMode=*/ false);
    assert(touchscreenTester);
    assertTrue(touchscreenTester.getIsTabletMode());

    provider.endTabletMode();
    await flushTasks();

    assertFalse(touchscreenTester.getIsTabletMode());
    assertTrue(
        (touchscreenTester.getDialog('canvas-dialog') as CrDialogElement).open);
  });

  test('OnInternalDisplayPowerStateChanged', async () => {
    const touchDevice: TouchDeviceInfo|undefined = fakeTouchDevices[1];
    assert(touchDevice);
    await initializeInputList([], [touchDevice]);

    assertTrue(
        (getCardByDeviceType('touchscreen')!.devices[0] as TouchDeviceInfo)
            .testable);

    provider.setInternalDisplayPowerOff();
    await flushTasks();

    assertFalse(
        (getCardByDeviceType('touchscreen')!.devices[0] as TouchDeviceInfo)
            .testable);

    provider.setInternalDisplayPowerOn();
    await flushTasks();

    assertTrue(
        (getCardByDeviceType('touchscreen')!.devices[0] as TouchDeviceInfo)
            .testable);
  });

  test('InternalDisplayPowerOffWhileTouchscreenTesterIsRunning', async () => {
    const touchDevice: TouchDeviceInfo|undefined = fakeTouchDevices[1];
    assert(touchDevice);
    const touchscreenTester = await openTouchscreenTester(
        /*touchscreensToInitialize=*/[touchDevice]);
    assert(touchscreenTester);
    // Internal display power off.
    provider.setInternalDisplayPowerOff();
    await flushTasks();

    // Tester is expecetd to exit.
    assertFalse(
        (touchscreenTester.getDialog('canvas-dialog') as CrDialogElement).open);
  });

  test('TouchscreenDisconnectedWhileTesterIsRunning', async () => {
    const touchDevice: TouchDeviceInfo|undefined = fakeTouchDevices[1];
    assert(touchDevice);
    const touchscreenTester = await openTouchscreenTester(
        /*touchscreensToInitialize=*/[touchDevice]);

    // Remove touchscreen.
    provider.removeFakeConnectedTouchDeviceById(touchDevice.id);
    await flushTasks();

    // Tester is expecetd to exit.
    assertFalse(
        (touchscreenTester.getDialog('canvas-dialog') as CrDialogElement).open);
  });

  test('EmptySectionsHidden', async () => {
    await initializeInputList([], []);
    assertFalse(isVisible(getCardByDeviceType('keyboard')));
    assertFalse(isVisible(getCardByDeviceType('touchpad')));
    assertFalse(isVisible(getCardByDeviceType('touchscreen')));

    provider.addFakeConnectedTouchDevice(
        (fakeTouchDevices[1] as TouchDeviceInfo));
    await flushTasks();
    assertTrue(!!getCardByDeviceType('touchscreen'));
    assertFalse(isVisible(getCardByDeviceType('keyboard')));
    assertFalse(isVisible(getCardByDeviceType('touchpad')));
    assertTrue(isVisible(getCardByDeviceType('touchscreen')));

    provider.removeFakeConnectedTouchDeviceById(fakeTouchDevices[1]!.id);
    provider.addFakeConnectedTouchDevice(
        (fakeTouchDevices[0] as TouchDeviceInfo));
    await flushTasks();
    assertFalse(isVisible(getCardByDeviceType('keyboard')));
    assertTrue(isVisible(getCardByDeviceType('touchpad')));
    assertFalse(isVisible(getCardByDeviceType('touchscreen')));

    provider.removeFakeConnectedTouchDeviceById(fakeTouchDevices[0]!.id);
    provider.addFakeConnectedKeyboard((fakeKeyboards[0] as KeyboardInfo));
    await flushTasks();
    assertTrue(isVisible(getCardByDeviceType('keyboard')));
    assertFalse(isVisible(getCardByDeviceType('touchpad')));
    assertFalse(isVisible(getCardByDeviceType('touchscreen')));

    provider.removeFakeConnectedKeyboardById(fakeKeyboards[0]!.id);
    await flushTasks();
    assertFalse(isVisible(getCardByDeviceType('keyboard')));
    assertFalse(isVisible(getCardByDeviceType('touchpad')));
    assertFalse(isVisible(getCardByDeviceType('touchscreen')));
  });

  test('RecordNavigationCalled', async () => {
    await initializeInputList();
    assert(inputListElement);
    inputListElement.onNavigationPageChanged({isActive: false});
    await flushTasks();

    assertEquals(0, diagnosticsBrowserProxy.getCallCount('recordNavigation'));

    diagnosticsBrowserProxy.setPreviousView(NavigationView.SYSTEM);
    inputListElement.onNavigationPageChanged({isActive: true});

    await flushTasks();
    assertEquals(1, diagnosticsBrowserProxy.getCallCount('recordNavigation'));
    assertArrayEquals(
        [NavigationView.SYSTEM, NavigationView.INPUT],
        (diagnosticsBrowserProxy.getArgs('recordNavigation')[0]));
  });
});
