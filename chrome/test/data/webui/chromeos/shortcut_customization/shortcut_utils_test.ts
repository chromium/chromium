// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {stringToMojoString16} from 'chrome://shortcut-customization/js/mojo_utils.js';
import {Accelerator, Modifier, MojoAccelerator, TextAcceleratorPart, TextAcceleratorPartType} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {areAcceleratorsEqual, getAccelerator, getAcceleratorId, getModifiersForAcceleratorInfo, getModifierString, getSortedModifiers, isCustomizationDisabled, isSearchEnabled, isStandardAcceleratorInfo, isTextAcceleratorInfo} from 'chrome://shortcut-customization/js/shortcut_utils.js';
import {assertArrayEquals, assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {createStandardAcceleratorInfo, createTextAcceleratorInfo} from './shortcut_customization_test_util.js';

function areModifiersEqual(
    modifiersA: string[], modifiersB: string[]): boolean {
  return modifiersA.length === modifiersB.length &&
      modifiersA.every((val, index) => val === modifiersB[index]);
}

suite('shortcutUtilsTest', function() {
  test('CustomizationDisabled', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: false});
    assertTrue(isCustomizationDisabled());
  });

  test('CustomizationEnabled', async () => {
    loadTimeData.overrideValues({isCustomizationEnabled: true});
    assertFalse(isCustomizationDisabled());
  });

  test('SearchDisabled', async () => {
    loadTimeData.overrideValues({isSearchEnabled: false});
    assertFalse(isSearchEnabled());
  });

  test('SearchEnabled', async () => {
    loadTimeData.overrideValues({isSearchEnabled: true});
    assertTrue(isSearchEnabled());
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
    assertFalse(isStandardAcceleratorInfo(textAccelerator));
  });

  test('isStandardAcceleratorInfo', async () => {
    const standardAccelerator = createStandardAcceleratorInfo(
        Modifier.ALT,
        /*keyCode=*/ 221,
        /*keyDisplay=*/ ']');
    assertTrue(isStandardAcceleratorInfo(standardAccelerator));
    assertFalse(isTextAcceleratorInfo(standardAccelerator));
  });

  test('GetAccelerator', async () => {
    const acceleratorInfo = createStandardAcceleratorInfo(
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

  test('GetSortedModifiers', () => {
    // Empty modifiers
    const actualEmpty = getSortedModifiers([]);
    const expectedEmpty: string[] = [];
    assertTrue(areModifiersEqual(actualEmpty, expectedEmpty));

    // Single modifiers
    const actualSingle = getSortedModifiers(['alt']);
    const expectedSingle: string[] = ['alt'];
    assertTrue(areModifiersEqual(actualSingle, expectedSingle));

    // Multiple modifiers
    const actualMultiple = getSortedModifiers(['ctrl', 'shift', 'meta', 'alt']);
    const expectedMultiple: string[] = ['ctrl', 'alt', 'shift', 'meta'];
    assertTrue(areModifiersEqual(actualMultiple, expectedMultiple));

    const actualMultiple2 = getSortedModifiers(['ctrl', 'shift', 'meta']);
    const expectedMultiple2: string[] = ['ctrl', 'shift', 'meta'];
    assertTrue(areModifiersEqual(actualMultiple2, expectedMultiple2));

    const actualMultiple3 =
        getSortedModifiers(['shift', 'meta', 'ctrl', 'alt']);
    const expectedMultiple3: string[] = ['ctrl', 'alt', 'shift', 'meta'];
    assertTrue(areModifiersEqual(actualMultiple3, expectedMultiple3));
  });

  test('getModifierString', () => {
    assertEquals('shift', getModifierString(Modifier.SHIFT));
    assertEquals('ctrl', getModifierString(Modifier.CONTROL));
    assertEquals('alt', getModifierString(Modifier.ALT));
    assertEquals('meta', getModifierString(Modifier.COMMAND));
  });

  test('getModifiersForAcceleratorInfo', () => {
    assertArrayEquals(
        [],
        getModifiersForAcceleratorInfo(createStandardAcceleratorInfo(
            0, /*keyCode=*/ 221,
            /*keyDisplay=*/ ']')));
    assertArrayEquals(
        ['alt'],
        getModifiersForAcceleratorInfo(createStandardAcceleratorInfo(
            Modifier.ALT, /*keyCode=*/ 221, /*keyDisplay=*/ ']')));
    assertArrayEquals(
        ['ctrl', 'alt'],
        getModifiersForAcceleratorInfo(createStandardAcceleratorInfo(
            Modifier.ALT | Modifier.CONTROL, /*keyCode=*/ 221,
            /*keyDisplay=*/ ']')));
    assertArrayEquals(
        ['ctrl', 'alt', 'shift', 'meta'],
        getModifiersForAcceleratorInfo(createStandardAcceleratorInfo(
            Modifier.ALT | Modifier.CONTROL | Modifier.COMMAND | Modifier.SHIFT,
            /*keyCode=*/ 221,
            /*keyDisplay=*/ ']')));
  });
});
