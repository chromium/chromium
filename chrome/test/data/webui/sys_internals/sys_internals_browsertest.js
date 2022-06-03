// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the SysInternals WebUI. (CrOS only)
 */

GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "content/public/test/browser_test.h"');

/* Set up this global variable to disable sending the update request. */
DONT_SEND_UPDATE_REQUEST = true;

function SysInternalsBrowserTest() {}

SysInternalsBrowserTest.prototype = {
  __proto__: testing.Test.prototype,

  browsePreload: 'chrome://sys-internals',

  isAsync: true,

  extraLibraries: [
    'api_test.js',
    'line_chart/data_series_test.js',
    'line_chart/line_chart_test.js',
    'line_chart/menu_test.js',
    'line_chart/scrollbar_test.js',
    'line_chart/sub_chart_test.js',
    'line_chart/unit_label_test.js',
    'page_drawer_test.js',
    'page_infopage_test.js',
    'page_switch_test.js',
    'page_unit_test.js',
    'test_util.js',
    '//third_party/mocha/mocha.js',
    '//third_party/polymer/v1_0/components-chromium/' +
        'iron-test-helpers/mock-interactions.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],
};

TEST_F('SysInternalsBrowserTest', 'getSysInfo', function() {
  ApiTest.getSysInfo();
});

TEST_F('SysInternalsBrowserTest', 'LineChart_DataSeries', function() {
  LineChartTest.DataSeries();
});

TEST_F('SysInternalsBrowserTest', 'LineChart_LineChart', function() {
  LineChartTest.LineChart();
});

TEST_F('SysInternalsBrowserTest', 'LineChart_Menu', function() {
  LineChartTest.Menu();
});

TEST_F('SysInternalsBrowserTest', 'LineChart_Scrollbar', function() {
  LineChartTest.Scrollbar();
});

TEST_F('SysInternalsBrowserTest', 'LineChart_SubChart', function() {
  LineChartTest.SubChart();
});

TEST_F('SysInternalsBrowserTest', 'LineChart_UnitLabel', function() {
  LineChartTest.UnitLabel();
});

TEST_F('SysInternalsBrowserTest', 'Page_Drawer', function() {
  PageTest.Drawer();
});

TEST_F('SysInternalsBrowserTest', 'Page_InfoPage', function() {
  PageTest.InfoPage();
});

TEST_F('SysInternalsBrowserTest', 'Page_Switch', function() {
  PageTest.Switch();
});

TEST_F('SysInternalsBrowserTest', 'Page_Unit', function() {
  PageTest.Unit();
});
