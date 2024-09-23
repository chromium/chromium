// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DataSeries} from 'chrome://sys-internals/line_chart/data_series.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';

import {assertCloseTo} from '../test_util.js';

suite('LineChart_DataSeries', function() {
  test('linerInterpolation', function() {
    assertCloseTo(
        DataSeries.linearInterpolation(100, 15, 142, 32, 118), 22.285, 1e-2);
    assertCloseTo(
        DataSeries.linearInterpolation(42, 15, 142, 32, 65), 18.91, 1e-2);
    assertCloseTo(
        DataSeries.linearInterpolation(100000, 640000, 123456, 654545, 112345),
        647655.099, 1e-2);
  });

  test('dataPointLinerInterpolation', function() {
    const dataSeries = new DataSeries('Test', '#aabbcc');
    dataSeries.getValues(0, 100, 10);
    const pointA = {time: 100, value: 15};
    const pointB = {time: 142, value: 32};
    assertCloseTo(
        dataSeries.dataPointLinearInterpolation(pointA, pointB, 118), 23.5,
        1e-2);

    dataSeries.getValues(30, 100, 10);
    const pointC = {time: 42, value: 15};
    const pointD = {time: 142, value: 32};
    assertCloseTo(
        dataSeries.dataPointLinearInterpolation(pointC, pointD, 65), 15, 1e-2);
    assertCloseTo(
        dataSeries.dataPointLinearInterpolation(pointC, pointD, 135), 32, 1e-2);

    dataSeries.getValues(42, 100, 10);
    const pointE = {time: 42, value: 615};
    const pointF = {time: 542, value: 132};
    assertCloseTo(
        dataSeries.dataPointLinearInterpolation(pointE, pointF, 315), 421.7999,
        1e-2);
  });

  test('DataSeries integration test', function() {
    const dataSeries = new DataSeries('Test', '#aabbcc');
    dataSeries.getValues(1200, 150, 10);  // Call when 0 point.
    dataSeries.addDataPoint(10, 1000);
    dataSeries.getValues(1200, 150, 10);  // Call when 1 point.
    dataSeries.addDataPoint(20, 2000);
    dataSeries.addDataPoint(42, 3000);
    dataSeries.addDataPoint(31, 4000);
    dataSeries.addDataPoint(59, 5000);
    dataSeries.addDataPoint(787, 6000);
    dataSeries.addDataPoint(612, 7000);
    dataSeries.addDataPoint(4873, 8000);
    dataSeries.addDataPoint(22, 9000);
    dataSeries.addDataPoint(10, 10000);

    assertEquals(dataSeries.findLowerBoundPointIndex_(1500), 1);
    assertEquals(dataSeries.findLowerBoundPointIndex_(9000), 8);
    assertEquals(dataSeries.findLowerBoundPointIndex_(10001), 10);

    assertDeepEquals(
        dataSeries.getSampleValue_(1, 2500), {value: 20, nextIndex: 2});
    assertDeepEquals(
        dataSeries.getSampleValue_(3, 3000), {value: null, nextIndex: 3});
    assertDeepEquals(
        dataSeries.getSampleValue_(4, 7000), {value: 423, nextIndex: 6});
    assertDeepEquals(
        dataSeries.getSampleValue_(10, 11000), {value: null, nextIndex: 10});

    assertDeepEquals(dataSeries.getValues(2000, 2000, 1), [31]);
    assertEquals(dataSeries.getMaxValue(2000, 2000, 1), 31);

    assertDeepEquals(dataSeries.getValues(0, 1000, 3), [null, 10, 20]);
    assertEquals(dataSeries.getMaxValue(0, 1000, 3), 20);

    /* (10 + 20) / 2 === 15 */
    assertDeepEquals(dataSeries.getValues(0, 3000, 1), [15]);
    assertEquals(dataSeries.getMaxValue(0, 3000, 1), 15);

    assertDeepEquals(
        dataSeries.getValues(4545, 1500, 5), [423, 612, 2447.5, 10, null]);
    assertEquals(dataSeries.getMaxValue(4545, 1500, 5), 2447.5);

    assertDeepEquals(
        dataSeries.getValues(1200, 100, 10),
        [12, null, null, null, null, null, null, null, 20, 22.2]);
    assertEquals(dataSeries.getMaxValue(1200, 100, 10), 22.2);
  });
});
