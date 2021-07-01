// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/input_list.js';

import {ConnectionType, KeyboardInfo, MechanicalLayout, PhysicalLayout, TouchDeviceInfo, TouchDeviceType} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeKeyboards, fakeTouchDevices} from 'chrome://diagnostics/fake_data.js';
import {FakeInputDataProvider} from 'chrome://diagnostics/fake_input_data_provider.js';
import {setInputDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

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

  function initializeInputList() {
    assertFalse(!!inputListElement);
    provider.setFakeConnectedDevices(fakeKeyboards, fakeTouchDevices);

    // Add the input list to the DOM.
    inputListElement =
        /** @type {!InputListElement} */ (document.createElement('input-list'));
    assertTrue(!!inputListElement);
    document.body.appendChild(inputListElement);

    return flushTasks();
  }

  test('InputListPopulated', () => {
    return initializeInputList().then(() => {
      dx_utils.assertElementContainsText(
          inputListElement.$$('#keyboardList'), fakeKeyboards[0].name);
      dx_utils.assertElementContainsText(
          inputListElement.$$('#touchpadList'), fakeTouchDevices[0].name);
      dx_utils.assertElementContainsText(
          inputListElement.$$('#touchscreenList'), fakeTouchDevices[1].name);
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
    };
    return initializeInputList()
        .then(() => {
          provider.addFakeConnectedKeyboard(fakeKeyboard);
          return flushTasks();
        })
        .then(() => {
          dx_utils.assertElementContainsText(
              inputListElement.$$('#keyboardList'), fakeKeyboards[0].name);
          dx_utils.assertElementContainsText(
              inputListElement.$$('#keyboardList'), fakeKeyboard.name);
          provider.removeFakeConnectedKeyboardById(fakeKeyboard.id);
          return flushTasks();
        })
        .then(() => {
          dx_utils.assertElementDoesNotContainText(
              inputListElement.$$('#keyboardList'), fakeKeyboard.name);
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
    return initializeInputList()
        .then(() => {
          provider.addFakeConnectedTouchDevice(fakeTouchpad);
          return flushTasks();
        })
        .then(() => {
          dx_utils.assertElementContainsText(
              inputListElement.$$('#touchpadList'), fakeTouchDevices[0].name);
          dx_utils.assertElementContainsText(
              inputListElement.$$('#touchpadList'), fakeTouchpad.name);
          provider.removeFakeConnectedTouchDeviceById(fakeTouchpad.id);
          return flushTasks();
        })
        .then(() => {
          dx_utils.assertElementDoesNotContainText(
              inputListElement.$$('#touchpadList'), fakeTouchpad.name);
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
    return initializeInputList()
        .then(() => {
          provider.addFakeConnectedTouchDevice(fakeTouchscreen);
          return flushTasks();
        })
        .then(() => {
          dx_utils.assertElementContainsText(
              inputListElement.$$('#touchscreenList'),
              fakeTouchDevices[1].name);
          dx_utils.assertElementContainsText(
              inputListElement.$$('#touchscreenList'), fakeTouchscreen.name);
          provider.removeFakeConnectedTouchDeviceById(fakeTouchscreen.id);
          return flushTasks();
        })
        .then(() => {
          dx_utils.assertElementDoesNotContainText(
              inputListElement.$$('#touchscreenList'), fakeTouchscreen.name);
        });
  });
}
