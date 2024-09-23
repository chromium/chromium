// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {UnitLabel} from 'chrome://sys-internals/line_chart/unit_label.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';

import {assertCloseTo} from '../test_util.js';

suite('LineChart_UnitLabel', function() {
  let TEST_MEM_UNIT;
  let TEST_MEM_UNITBASE;
  let testLabel;

  suiteSetup(function() {
    TEST_MEM_UNIT = ['B', 'KB', 'MB', 'GB', 'TB', 'PB'];
    TEST_MEM_UNITBASE = 1024;
    testLabel = new UnitLabel(TEST_MEM_UNIT, TEST_MEM_UNITBASE);
  });

  test('getSuitableUnit', function() {
    assertDeepEquals(
        UnitLabel.getSuitableUnit(
            Math.pow(1024, 4) * 5, TEST_MEM_UNIT, TEST_MEM_UNITBASE),
        {value: 5, unitIdx: 4});

    assertDeepEquals(
        UnitLabel.getSuitableUnit(
            Math.pow(1024, 2) * 1023, TEST_MEM_UNIT, TEST_MEM_UNITBASE),
        {value: 1023, unitIdx: 2});

    assertDeepEquals(
        UnitLabel.getSuitableUnit(
            Math.pow(1024, 6), TEST_MEM_UNIT, TEST_MEM_UNITBASE),
        {value: 1024, unitIdx: 5});
  });

  test('getTopLabelValue_', function() {
    assertEquals(testLabel.getTopLabelValue_(55, 10), 60);
    assertEquals(testLabel.getTopLabelValue_(73.5, 15), 75);
  });

  test('UnitLabel integration test', function() {
    testLabel.setLayout(600, 12, 2);
    assertEquals(testLabel.getMaxNumberOfLabel_(), 6);
    assertEquals(testLabel.getCurrentUnitString(), 'B');
    assertEquals(testLabel.getRealValueWithCurrentUnit_(1234), 1234);

    testLabel.setMaxValue(Math.pow(1024, 4) * 123);
    assertEquals(testLabel.getCurrentUnitString(), 'TB');
    assertEquals(
        testLabel.getRealValueWithCurrentUnit_(42), Math.pow(1024, 4) * 42);

    assertEquals(testLabel.getNumberOfLabelWithStepSize_(20), 8);
    assertEquals(testLabel.getNumberOfLabelWithStepSize_(50), 4);
    assertEquals(testLabel.getNumberOfLabelWithStepSize_(0.1), 1231);

    assertDeepEquals(
        testLabel.getSuitableStepSize_(), {stepSize: 50, stepSizePrecision: 0});
    assertDeepEquals(
        testLabel.getLabels(), ['150 TB', '100 TB', '50 TB', '0 TB']);

    const realTopValue = Math.pow(1024, 4) * 150;
    assertCloseTo(testLabel.getScale() * realTopValue, 600, 1e-2);
  });
});
