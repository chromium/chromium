// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {stringToMojoString16} from 'chrome://shortcut-customization/js/mojo_utils.js';
import {Accelerator, Modifier, MojoAccelerator, TextAcceleratorPart, TextAcceleratorPartType} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {areAcceleratorsEqual, getAccelerator, getAcceleratorId, isCustomizationDisabled, isDefaultAcceleratorInfo, isTextAcceleratorInfo} from 'chrome://shortcut-customization/js/shortcut_utils.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {createDefaultAcceleratorInfo, createTextAcceleratorInfo} from './shortcut_customization_test_util.js';

suite('shortcutUtilsTest', function() {
  test('CustomizationDisabled', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: false});
    assertTrue(isCustomizationDisabled());
  });

  test('CustomizationEnabled', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: true});
    assertFalse(isCustomizationDisabled());
  });

  test('AreAcceleratorsEqual', async () => {
    const accelShiftC: Accelerator = {
      modifiers: Modifier.SHIFT,
      keyCode: 67,  // c
    };
    const accelShiftCCopy: Accelerator = {
      ...accelShiftC,
    };
    const accelAltC: Accelerator = {
      modifiers: Modifier.ALT,
      keyCode: 67,  // c
    };
    const accelShiftD: Accelerator = {
      modifiers: Modifier.SHIFT,
      keyCode: 68,  // d
    };

    // Compare the same accelerator.
    assertTrue(areAcceleratorsEqual(accelShiftC, accelShiftC));

    // Compare accelerators with the same properties.
    assertTrue(areAcceleratorsEqual(accelShiftC, accelShiftCCopy));

    // Compare accelerators with different modifiers.
    assertFalse(areAcceleratorsEqual(accelShiftC, accelAltC));

    // Compare accelerators with different key and keyDisplay.
    assertFalse(areAcceleratorsEqual(accelShiftC, accelShiftD));
  });

  test('AreAcceleratorsEqualMojo', async () => {
    const accelShiftC: Accelerator = {
      modifiers: Modifier.SHIFT,
      keyCode: 67,  // c
    };
    const accelShiftCMojo: MojoAccelerator = {
      modifiers: Modifier.SHIFT,
      keyCode: 67,  // c
      keyState: 0,
      timeStamp: {internalValue: BigInt(0)},
    };

    // Accelerators and MojoAccelerators are comparable,
    // and shouldn't throw an error.
    assertTrue(areAcceleratorsEqual(accelShiftC, accelShiftCMojo));
  });

  test('GetAcceleratorId', async () => {
    assertEquals(`${0}-${80}`, getAcceleratorId(0, 80));
    assertEquals(`${0}-${80}`, getAcceleratorId('0', '80'));
  });

  test('isTextAcceleratorInfo', async () => {
    const textAcceleratorParts: TextAcceleratorPart[] =
        [{text: stringToMojoString16('a'), type: TextAcceleratorPartType.kKey}];
    const textAccelerator = createTextAcceleratorInfo(textAcceleratorParts);
    assertTrue(isTextAcceleratorInfo(textAccelerator));
    assertFalse(isDefaultAcceleratorInfo(textAccelerator));
  });

  test('isDefaultAcceleratorInfo', async () => {
    const defaultAccelerator = createDefaultAcceleratorInfo(
        Modifier.ALT,
        /*keyCode=*/ 221,
        /*keyDisplay=*/ ']');
    assertTrue(isDefaultAcceleratorInfo(defaultAccelerator));
    assertFalse(isTextAcceleratorInfo(defaultAccelerator));
  });

  test('GetAccelerator', async () => {
    const acceleratorInfo = createDefaultAcceleratorInfo(
        Modifier.ALT,
        /*keyCode=*/ 221,
        /*keyDisplay=*/ ']');
    const expectedAccelerator: Accelerator = {
      modifiers: Modifier.ALT,
      keyCode: 221,  // c
    };
    const actualAccelerator = getAccelerator(acceleratorInfo);
    assertDeepEquals(expectedAccelerator, actualAccelerator);
  });
});