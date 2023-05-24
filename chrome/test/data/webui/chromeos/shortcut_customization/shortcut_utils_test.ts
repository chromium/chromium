// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CycleTabsTextSearchResult, SnapWindowLeftSearchResult, TakeScreenshotSearchResult} from 'chrome://shortcut-customization/js/fake_data.js';
import {stringToMojoString16} from 'chrome://shortcut-customization/js/mojo_utils.js';
import {Accelerator, AcceleratorCategory, Modifier, MojoAccelerator, StandardAcceleratorInfo, TextAcceleratorPart, TextAcceleratorPartType} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {areAcceleratorsEqual, compareAcceleratorInfos, getAccelerator, getAcceleratorId, getModifiersForAcceleratorInfo, getModifierString, getSortedModifiers, getSourceAndActionFromAcceleratorId, getURLForSearchResult, isCustomizationDisabled, isSearchEnabled, isStandardAcceleratorInfo, isTextAcceleratorInfo, SHORTCUTS_APP_URL} from 'chrome://shortcut-customization/js/shortcut_utils.js';
import {assertArrayEquals, assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {createStandardAcceleratorInfo, createTextAcceleratorInfo} from './shortcut_customization_test_util.js';

function areModifiersEqual(
    modifiersA: string[], modifiersB: string[]): boolean {
  return modifiersA.length === modifiersB.length &&
      modifiersA.every((val, index) => val === modifiersB[index]);
}

function areStandardAcceleratorInfosEqual(
    first: StandardAcceleratorInfo[], second: StandardAcceleratorInfo[]) {
  assertEquals(first.length, second.length);
  for (let i = 0; i < first.length; ++i) {
    const firstAccelerator = first[i]!.layoutProperties.standardAccelerator;
    const secondAccelerator = second[i]!.layoutProperties.standardAccelerator;
    assertEquals(
        firstAccelerator.accelerator.modifiers,
        secondAccelerator.accelerator.modifiers);
    assertEquals(
        firstAccelerator.accelerator.keyCode,
        secondAccelerator.accelerator.keyCode);
    assertEquals(firstAccelerator.keyDisplay, secondAccelerator.keyDisplay);
  }
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
    const expectedMultiple: string[] = ['meta', 'ctrl', 'alt', 'shift'];
    assertTrue(areModifiersEqual(actualMultiple, expectedMultiple));

    const actualMultiple2 = getSortedModifiers(['ctrl', 'shift', 'meta']);
    const expectedMultiple2: string[] = ['meta', 'ctrl', 'shift'];
    assertTrue(areModifiersEqual(actualMultiple2, expectedMultiple2));

    const actualMultiple3 =
        getSortedModifiers(['shift', 'meta', 'ctrl', 'alt']);
    const expectedMultiple3: string[] = ['meta', 'ctrl', 'alt', 'shift'];
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
        ['meta', 'ctrl', 'alt', 'shift'],
        getModifiersForAcceleratorInfo(createStandardAcceleratorInfo(
            Modifier.ALT | Modifier.CONTROL | Modifier.COMMAND | Modifier.SHIFT,
            /*keyCode=*/ 221,
            /*keyDisplay=*/ ']')));
  });

  test('getURLForSearchResult', () => {
    assertEquals(
        `${SHORTCUTS_APP_URL}/?action=1&category=${
            AcceleratorCategory.kGeneral}`,
        getURLForSearchResult(CycleTabsTextSearchResult).href);
    assertEquals(
        `${SHORTCUTS_APP_URL}/?action=0&category=${
            AcceleratorCategory.kWindowsAndDesks}`,
        getURLForSearchResult(SnapWindowLeftSearchResult).href);
    assertEquals(
        `${SHORTCUTS_APP_URL}/?action=2&category=${
            AcceleratorCategory.kWindowsAndDesks}`,
        getURLForSearchResult(TakeScreenshotSearchResult).href);
  });

  test('sortStandardAcceleratorInfo', () => {
    const standardAcceleratorInfo1 = createStandardAcceleratorInfo(
        Modifier.ALT,
        /*keyCode=*/ 221,
        /*keyDisplay=*/ ']');

    // No modifier, this should get the highest priority.
    const standardAcceleratorInfo2 = createStandardAcceleratorInfo(
        Modifier.NONE,
        /*keyCode=*/ 221,
        /*keyDisplay=*/ ']');

    const standardAcceleratorInfo3 = createStandardAcceleratorInfo(
        Modifier.ALT | Modifier.SHIFT | Modifier.COMMAND,
        /*keyCode=*/ 221,
        /*keyDisplay=*/ ']');

    const standardAcceleratorInfo4 = createStandardAcceleratorInfo(
        Modifier.ALT | Modifier.SHIFT,
        /*keyCode=*/ 221,
        /*keyDisplay=*/ ']');

    const initialOrder = [
      standardAcceleratorInfo1,
      standardAcceleratorInfo2,
      standardAcceleratorInfo3,
      standardAcceleratorInfo4,
    ];
    const expectedOrder = [
      standardAcceleratorInfo2,
      standardAcceleratorInfo1,
      standardAcceleratorInfo4,
      standardAcceleratorInfo3,
    ];
    initialOrder.sort(compareAcceleratorInfos);
    areStandardAcceleratorInfosEqual(expectedOrder, initialOrder);
  });

  test('sortStandardAcceleratorInfoStableOrder', async () => {
    const standardAcceleratorInfo1 = createStandardAcceleratorInfo(
        Modifier.ALT,
        /*keyCode=*/ 221,
        /*keyDisplay=*/ ']');

    // No modifier, this should get the highest priority.
    const standardAcceleratorInfo2 = createStandardAcceleratorInfo(
        Modifier.COMMAND,
        /*keyCode=*/ 221,
        /*keyDisplay=*/ ']');

    const standardAcceleratorInfo3 = createStandardAcceleratorInfo(
        Modifier.CONTROL,
        /*keyCode=*/ 221,
        /*keyDisplay=*/ ']');

    const initialOrder = [
      standardAcceleratorInfo1,
      standardAcceleratorInfo2,
      standardAcceleratorInfo3,
    ];
    const expectedOrder = [
      standardAcceleratorInfo1,
      standardAcceleratorInfo2,
      standardAcceleratorInfo3,
    ];
    initialOrder.sort(compareAcceleratorInfos);
    areStandardAcceleratorInfosEqual(expectedOrder, initialOrder);
  });

  test('getSourceAndActionFromAcceleratorId', async () => {
    const result1 = getSourceAndActionFromAcceleratorId('3-45');
    assertDeepEquals(result1, {source: 3, action: 45});

    const result2 = getSourceAndActionFromAcceleratorId('0-33');
    assertDeepEquals(result2, {source: 0, action: 33});
  });
});
