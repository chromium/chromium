// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {constructRgba, getTrailOpacityFromPressure, lookupCssVariableValue, MARK_COLOR, MARK_OPACITY, TRAIL_COLOR, TRAIL_MAX_OPACITY} from 'chrome://diagnostics/drawing_provider_utils.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from 'chrome://webui-test/mock_controller.js';

// A helper function to mock getPropertyValue function.
const mockGetPropertyValue = (valName: string): string => {
  switch (valName) {
    case TRAIL_COLOR:
      return 'rgb(220, 210, 155)';
    case MARK_COLOR:
      return 'rgb(198, 179, 165)';
    case MARK_OPACITY:
      return '0.7';
    default:
      assertNotReached();
  }
};

suite('drawingProviderUtilsTestSuite', function() {
  const mockController = new MockController();

  setup(() => {
    // Setup mock for window.getComputedStyle function to prevent test flaky.
    const mockComputedStyle =
        mockController.createFunctionMock(window, 'getComputedStyle');
    mockComputedStyle.returnValue = {
      getPropertyValue: mockGetPropertyValue,
    };
  });

  teardown(() => {
    mockController.reset();
  });

  test('LookupCssVariableValue', () => {
    [TRAIL_COLOR, MARK_COLOR, MARK_OPACITY].forEach((valName: string) => {
      assertEquals(
          mockGetPropertyValue(valName), lookupCssVariableValue(valName));
    });
  });

  test('GetTrailOpacityFromPressure', () => {
    const pressures: number[] = [0.1, 0.4, 0.5, 0.6];
    const expectedOpacity: string[] = pressures.map(
        (pressure: number) => String(TRAIL_MAX_OPACITY * pressure));

    for (let i = 0; i < pressures.length; i++) {
      const pressure = pressures[i];
      assert(pressure);
      assertEquals(expectedOpacity[i], getTrailOpacityFromPressure(pressure));
    }
  });

  test('ConstructRgba', () => {
    const rgbList: string[] = ['rgb(55, 10, 223)', 'rgb(1, 2, 44)'];
    const opacityList: string[] = ['0.1', '1'];
    const expectedRgba: string[] =
        ['rgba(55, 10, 223, 0.1)', 'rgba(1, 2, 44, 1)'];
    for (let i = 0; i < rgbList.length; i++) {
      const rgb = rgbList[i];
      assert(rgb);
      const opacity = opacityList[i];
      assert(opacity);
      assertEquals(expectedRgba[i], constructRgba(rgb, opacity));
    }
  });
});
