// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SAMPLE_RATE, UnitLabelAlign} from 'chrome://sys-internals/line_chart/constants.js';
import {DataSeries} from 'chrome://sys-internals/line_chart/data_series.js';
import {LineChart} from 'chrome://sys-internals/line_chart/line_chart.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {assertCloseTo} from '../test_util.js';

suite('LineChart_LineChart', function() {
  test('touchDistance', function() {
    const touchA = {clientX: 100, clientY: 100};
    const touchB = {clientX: 140, clientY: 130};
    assertCloseTo(LineChart.touchDistance_(touchA, touchB), 50, 1e-2);

    const touchC = {clientX: 1230, clientY: 475};
    const touchD = {clientX: 523, clientY: 675};
    assertCloseTo(LineChart.touchDistance_(touchC, touchD), 734.744, 1e-2);
  });

  test('getSuitableTimeStep_', function() {
    assertEquals(LineChart.getSuitableTimeStep_(100, 1000), 300000);
    assertEquals(LineChart.getSuitableTimeStep_(70, 1500), 300000);
    assertEquals(LineChart.getSuitableTimeStep_(120, 30000), 3600000);
    assertEquals(LineChart.getSuitableTimeStep_(90, 100), 30000);
  });

  test('LineChart integration test', function() {
    const rootDiv = document.createElement('div');
    rootDiv.style = 'position: absolute; width: 1000px; height: 600px;';
    rootDiv.className = 'line-chart-root';
    document.body.appendChild(rootDiv);
    assertEquals(rootDiv.offsetWidth, 1000);

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

    const lineChart = new LineChart();
    lineChart.attachRootDiv(rootDiv);

    assertFalse(lineChart.shouldRender());
    lineChart.setSubChart(UnitLabelAlign.LEFT, ['', 'K', 'M', 'G'], 1000);
    lineChart.setSubChart(UnitLabelAlign.RIGHT, ['B', 'KB', 'MB', 'GB'], 1024);
    lineChart.addDataSeries(UnitLabelAlign.LEFT, data1);
    lineChart.addDataSeries(UnitLabelAlign.RIGHT, data2);
    lineChart.addDataSeries(UnitLabelAlign.RIGHT, data3);
    assertTrue(lineChart.shouldRender());

    const visibleChartWidth = lineChart.getChartVisibleWidth();
    lineChart.updateEndTime(
        lineChart.startTime_ + 100000 + visibleChartWidth * 100);
    const offset = (1000 + visibleChartWidth) % SAMPLE_RATE;
    assertCloseTo(
        lineChart.getChartWidth_(), 1000 + visibleChartWidth - offset, 1e-2);
    assertTrue(lineChart.scrollbar_.isScrolledToRightEdge());

    /* See |Scrollbar.isScrolledToRightEdge()|. */
    const scrollError = 2;
    assertCloseTo(lineChart.scale_, 100, 1e-2);
    assertCloseTo(
        lineChart.scrollbar_.getPosition(), 1000 - offset, scrollError);

    lineChart.scroll(100);
    assertCloseTo(
        lineChart.scrollbar_.getPosition(), 1000 - offset, scrollError);
    lineChart.scroll(-50);
    assertCloseTo(
        lineChart.scrollbar_.getPosition(), 950 - offset, scrollError);
    lineChart.scroll(10.85);
    assertCloseTo(
        lineChart.scrollbar_.getPosition(), 961 - offset, scrollError);
    lineChart.scroll(-10000);
    assertCloseTo(lineChart.scrollbar_.getPosition(), 0, scrollError);
    lineChart.scrollbar_.scrollToRightEdge();
    assertCloseTo(
        lineChart.scrollbar_.getPosition(), 1000 - offset, scrollError);

    lineChart.zoom(7.6);
    assertCloseTo(lineChart.scale_, 760, 1e-2);
    assertTrue(lineChart.scrollbar_.isScrolledToRightEdge());
    lineChart.zoom(1.42);
    assertCloseTo(lineChart.scale_, 1079.2, 1e-2);
    assertTrue(lineChart.scrollbar_.isScrolledToRightEdge());
    lineChart.zoom(0.21);
    assertCloseTo(lineChart.scale_, 226.632, 1e-2);
    assertTrue(lineChart.scrollbar_.isScrolledToRightEdge());
    lineChart.zoom(1.25);
    assertCloseTo(lineChart.scale_, 283.29, 1e-2);
    assertTrue(lineChart.scrollbar_.isScrolledToRightEdge());

    lineChart.clearAllSubChart();
    assertFalse(lineChart.shouldRender());
  });
});
