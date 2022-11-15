// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/strings.m.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {constructRgba, getTrailOpacityFromPressure, lookupCssVariableValue, MARK_COLOR, MARK_OPACITY, TRAIL_COLOR, TRAIL_MAX_OPACITY} from 'chrome://diagnostics/drawing_provider_utils.js';

import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from '../mock_controller.m.js';

// A helper function to mock getPropertyValue function.
const mockGetPropertyValue = (valName) => {
  switch (valName) {
    case TRAIL_COLOR:
      return 'rgb(220, 210, 155)';
    case MARK_COLOR:
      return 'rgb(198, 179, 165)';
    case MARK_OPACITY:
      return '0.7';
  }
};

suite('drawingProviderUtilsTestSuite', function() {
  /** @type {{createFunctionMock: Function, reset: Function}} */
  let mockController;

  setup(() => {
    // Setup mock for window.getComputedStyle function to prevent test flaky.
    mockController = new MockController();
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
    [TRAIL_COLOR, MARK_COLOR, MARK_OPACITY].forEach(valName => {
      assertEquals(
          mockGetPropertyValue(valName), lookupCssVariableValue(valName));
    });
  });

  test('GetTrailOpacityFromPressure', () => {
    const pressures = [0.1, 0.4, 0.5, 0.6];
    const expectedOpacity =
        pressures.map(pressure => String(TRAIL_MAX_OPACITY * pressure));

    for (let i = 0; i < pressures.length; i++) {
      assertEquals(
          expectedOpacity[i], getTrailOpacityFromPressure(pressures[i]));
    }
  });


  test('ConstructRgba', () => {
    const rgbList = ['rgb(55, 10, 223)', 'rgb(  1,2, 44 )'];
    const opacityList = ['0.1', '1'];
    const expectedRgba = ['rgba(55, 10, 223, 0.1)', 'rgba(1,2, 44, 1)'];

    for (let i = 0; i < rgbList.length; i++) {
      assertEquals(expectedRgba[i], constructRgba(rgbList[i], opacityList[i]));
    }
  });
});
