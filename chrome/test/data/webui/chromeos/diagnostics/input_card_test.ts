// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import type {IronIconElement} from '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import {fakeTouchDevices} from 'chrome://diagnostics/fake_data.js';
import {FakeInputDataProvider} from 'chrome://diagnostics/fake_input_data_provider.js';
import {ConnectionType, KeyboardInfo, MechanicalLayout, NumberPadPresence, PhysicalLayout, TopRightKey, TopRowKey} from 'chrome://diagnostics/input.mojom-webui.js';
import {InputCardElement, InputCardType} from 'chrome://diagnostics/input_card.js';
import {TouchDeviceInfo} from 'chrome://diagnostics/input_data_provider.mojom-webui.js';
import {setInputDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

const keyboards: KeyboardInfo[] = [
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
    regionCode: 'US',
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
    regionCode: 'US',
  },
];

suite('inputCardTestSuite', function() {
  let inputCardElement: InputCardElement|null = null;

  const provider = new FakeInputDataProvider();

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    setInputDataProviderForTesting(provider);

    provider.setStartWithLidOpen();
    provider.setStartTesterWithClamshellMode();
  });

  teardown(() => {
    inputCardElement?.remove();
    inputCardElement = null;
    provider.reset();
  });

  function initializeInputCard(
      deviceType: InputCardType,
      devices: KeyboardInfo[]|TouchDeviceInfo[]): Promise<void> {
    inputCardElement = document.createElement('input-card');
    assert(inputCardElement);
    inputCardElement.deviceType = deviceType;
    inputCardElement.devices = devices;
    inputCardElement.hostDeviceStatus = {
      isLidOpen: true,
      isTabletMode: false,
    };
    document.body.appendChild(inputCardElement);

    return flushTasks();
  }

  function getDeviceName(element: Element|undefined): string {
    assert(element);
    return strictQuery('.device-name', element, HTMLDivElement).innerText;
  }

  function getDeviceDescription(element: Element|undefined): string {
    assert(element);
    return strictQuery('.device-description', element, HTMLDivElement)
        .innerText;
  }

  function getButtonDisabledState(element: Element|undefined): boolean {
    assert(element);
    return strictQuery('cr-button', element, CrButtonElement).disabled;
  }

  function getIconHiddenState(element: Element|undefined): boolean {
    assert(element);
    return (strictQuery('#infoIcon', element, Element) as IronIconElement)
        .hidden;
  }

  function getTooltipTextElement(element: Element|undefined): IronIconElement {
    assert(element);
    const tooltipText = element.querySelector<IronIconElement>('#tooltipText');
    assert(tooltipText);
    return tooltipText;
  }

  test('KeyboardsListedCorrectly', async () => {
    await initializeInputCard(InputCardType.KEYBOARD, keyboards);
    assert(inputCardElement);
    const elements = inputCardElement.shadowRoot!.querySelectorAll('.device');
    assertEquals(2, elements.length);
    assertEquals(keyboards[0]!.name, getDeviceName(elements[0]));
    assertEquals('Built-in keyboard', getDeviceDescription(elements[0]));
    assertEquals(keyboards[1]!.name, getDeviceName(elements[1]));
    assertEquals('Bluetooth keyboard', getDeviceDescription(elements[1]));
  });

  test('TestButtonClickEvent', async () => {
    await initializeInputCard(InputCardType.KEYBOARD, keyboards);
    assert(inputCardElement);
    const testButtonClickPromise =
        eventToPromise('test-button-click', inputCardElement);
    strictQuery(
        '.device[data-evdev-id="10"] cr-button', inputCardElement.shadowRoot,
        CrButtonElement)
        .click();
    const clickEvent = await testButtonClickPromise;
    assertEquals(10, clickEvent.detail.evdevId);
  });

  test('TouchscreenTestability', async () => {
    await initializeInputCard(InputCardType.TOUCHSCREEN, fakeTouchDevices);
    assert(inputCardElement);
    const elements = inputCardElement.shadowRoot!.querySelectorAll('.device');
    assertEquals(3, elements.length);
    // Check a testable touchscreen.
    assertEquals(fakeTouchDevices[1]!.name, getDeviceName(elements[1]));
    assertFalse(getButtonDisabledState(elements[1]));
    assertTrue(getIconHiddenState(elements[1]));

    // Check an untestable touchscreen.
    assertEquals(fakeTouchDevices[2]!.name, getDeviceName(elements[2]));
    assertTrue(getButtonDisabledState(elements[2]));
    assertFalse(getIconHiddenState(elements[2]));
  });

  test('KeyboardTestabilityLidState', async () => {
    await initializeInputCard(InputCardType.KEYBOARD, keyboards);
    assert(inputCardElement);
    const elements = inputCardElement!.root!.querySelectorAll('.device');
    assertEquals(2, elements.length);

    inputCardElement.hostDeviceStatus = {
      ...inputCardElement.hostDeviceStatus,
      isLidOpen: true,
    };
    await flushTasks();

    // Check that all keyboards start testable.
    assertEquals(keyboards[0]!.name, getDeviceName(elements[0]));
    assertEquals('Built-in keyboard', getDeviceDescription(elements[0]));
    assertFalse(getButtonDisabledState(elements[0]));
    assertTrue(getIconHiddenState(elements[0]));

    assertEquals(keyboards[1]!.name, getDeviceName(elements[1]));
    assertEquals('Bluetooth keyboard', getDeviceDescription(elements[1]));
    assertFalse(getButtonDisabledState(elements[1]));
    assertTrue(getIconHiddenState(elements[1]));

    // Check that internal keyboard is no longer testable after lid closing.
    inputCardElement.hostDeviceStatus = {
      ...inputCardElement.hostDeviceStatus,
      isLidOpen: false,
    };
    await flushTasks();

    assertEquals(keyboards[0]!.name, getDeviceName(elements[0]));
    assertEquals('Built-in keyboard', getDeviceDescription(elements[0]));
    assertTrue(getButtonDisabledState(elements[0]));
    const tooltipText = getTooltipTextElement(elements[0]);
    assertEquals(
        loadTimeData.getString('inputKeyboardUntestableLidClosedNote'),
        tooltipText.innerText.trim());

    assertEquals(keyboards[1]!.name, getDeviceName(elements[1]));
    assertEquals('Bluetooth keyboard', getDeviceDescription(elements[1]));
    assertFalse(getButtonDisabledState(elements[1]));
    assertTrue(getIconHiddenState(elements[1]));

    // Check internal keyboard tester is re-enabled after lid opening.
    inputCardElement.hostDeviceStatus = {
      ...inputCardElement.hostDeviceStatus,
      isLidOpen: true,
    };
    await flushTasks();

    assertFalse(getButtonDisabledState(elements[1]));
    assertTrue(getIconHiddenState(elements[1]));
  });

  test('KeyboardTestabilityTabletMode', async () => {
    await initializeInputCard(InputCardType.KEYBOARD, keyboards);
    assert(inputCardElement);
    const elements = inputCardElement!.root!.querySelectorAll('.device');
    assertEquals(2, elements.length);
    inputCardElement.hostDeviceStatus = {
      ...inputCardElement.hostDeviceStatus,
      isTabletMode: false,
    };
    await flushTasks();

    // Check that all keyboards start testable.
    assertEquals(keyboards[0]!.name, getDeviceName(elements[0]));
    assertEquals('Built-in keyboard', getDeviceDescription(elements[0]));
    assertFalse(getButtonDisabledState(elements[0]));
    assertTrue(getIconHiddenState(elements[0]));

    assertEquals(keyboards[1]!.name, getDeviceName(elements[1]));
    assertEquals('Bluetooth keyboard', getDeviceDescription(elements[1]));
    assertFalse(getButtonDisabledState(elements[1]));
    assertTrue(getIconHiddenState(elements[1]));

    inputCardElement.hostDeviceStatus = {
      ...inputCardElement.hostDeviceStatus,
      isTabletMode: true,
    };
    await flushTasks();

    // Check that internal keyboard is no longer testable after switching to
    // tablet mode.
    assertEquals(keyboards[0]!.name, getDeviceName(elements[0]));
    assertEquals('Built-in keyboard', getDeviceDescription(elements[0]));
    assertTrue(getButtonDisabledState(elements[0]));
    assertFalse(getIconHiddenState(elements[0]));
    const tooltipText = getTooltipTextElement(elements[0]);
    assertEquals(
        loadTimeData.getString('inputKeyboardUntestableTabletModeNote'),
        tooltipText.innerText.trim());

    assertEquals(keyboards[1]!.name, getDeviceName(elements[1]));
    assertEquals('Bluetooth keyboard', getDeviceDescription(elements[1]));
    assertFalse(getButtonDisabledState(elements[1]));
    assertTrue(getIconHiddenState(elements[1]));

    // Check internal keyboard tester is re-enabled after ending tablet mode.
    inputCardElement.hostDeviceStatus = {
      ...inputCardElement.hostDeviceStatus,
      isTabletMode: false,
    };
    await flushTasks();

    assertFalse(getButtonDisabledState(elements[1]));
    assertTrue(getIconHiddenState(elements[1]));
  });
});
