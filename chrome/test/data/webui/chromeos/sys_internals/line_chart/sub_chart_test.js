// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {UnitLabelAlign} from 'chrome://sys-internals/line_chart/constants.js';
import {DataSeries} from 'chrome://sys-internals/line_chart/data_series.js';
import {SubChart} from 'chrome://sys-internals/line_chart/sub_chart.js';
import {UnitLabel} from 'chrome://sys-internals/line_chart/unit_label.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {assertCloseTo} from '../test_util.js';

suite('LineChart_SubChart', function() {
  test('SubChart integration test', function() {
    const data1 = new DataSeries('test1', '#aabbcc');
    data1.addDataPoint(100, 1504764694799);
    data1.addDataPoint(100, 1504764695799);
    data1.addDataPoint(100, 1504764696799);
    const data2 = new DataSeries('test2', '#aabbcc');
    data2.addDataPoint(40, 1504764694799);
    data2.addDataPoint(42, 1504764695799);
    data2.addDataPoint(40, 1504764696799);
    const data3 = new DataSeries('test3', '#aabbcc');
    data3.addDataPoint(1024, 1504764694799);
    data3.addDataPoint(2048, 1504764695799);
    data3.addDataPoint(4096, 1504764696799);

    const label = new UnitLabel(['/s', 'K/s', 'M/s'], 1000);
    const subChart = new SubChart(label, UnitLabelAlign.RIGHT);
    assertFalse(subChart.shouldRender());
    subChart.addDataSeries(data1);
    subChart.addDataSeries(data2);
    subChart.addDataSeries(data3);
    assertEquals(subChart.getDataSeriesList().length, 3);
    assertTrue(subChart.shouldRender());

    subChart.setLayout(1920, 1080, 14, 1504764695799, 150, 8);
    assertCloseTo(subChart.label_.maxValueCache_, 2389.333, 1e-2);
    subChart.setMaxValue(424242);
    assertCloseTo(subChart.label_.maxValueCache_, 424242, 1e-2);

    subChart.setLayout(1920, 1080, 14, 1504764695799, 10, 8);
    assertCloseTo(subChart.label_.maxValueCache_, 424242, 1e-2);
    subChart.setMaxValue(null);
    assertCloseTo(subChart.label_.maxValueCache_, 4096, 1e-2);

    subChart.setLayout(150, 100, 14, 1504764685799, 100, 8);
    assertCloseTo(subChart.label_.maxValueCache_, 3072, 1e-2);
  });
});
