// Copyright 2017 The Chromium Authors
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

  browsePreload:
      'chrome://sys-internals/index.html?module=chromeos/sys_internals/all_tests.js',

  isAsync: true,

  extraLibraries: [
    '//third_party/node/node_modules/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],
};

TEST_F('SysInternalsBrowserTest', 'getSysInfo', function() {
  runMochaSuite('getSysInfo');
});

TEST_F('SysInternalsBrowserTest', 'LineChart_DataSeries', function() {
  runMochaSuite('LineChart_DataSeries');
});

TEST_F('SysInternalsBrowserTest', 'LineChart_LineChart', function() {
  runMochaSuite('LineChart_LineChart');
});

TEST_F('SysInternalsBrowserTest', 'LineChart_Menu', function() {
  runMochaSuite('LineChart_Menu');
});

TEST_F('SysInternalsBrowserTest', 'LineChart_Scrollbar', function() {
  runMochaSuite('LineChart_Scrollbar');
});

TEST_F('SysInternalsBrowserTest', 'LineChart_SubChart', function() {
  runMochaSuite('LineChart_SubChart');
});

TEST_F('SysInternalsBrowserTest', 'LineChart_UnitLabel', function() {
  runMochaSuite('LineChart_UnitLabel');
});

TEST_F('SysInternalsBrowserTest', 'Page_Drawer', function() {
  runMochaSuite('Page_Drawer');
});

TEST_F('SysInternalsBrowserTest', 'Page_InfoPage', function() {
  runMochaSuite('Page_InfoPage');
});

TEST_F('SysInternalsBrowserTest', 'Page_Switch', function() {
  runMochaSuite('Page_Switch');
});

TEST_F('SysInternalsBrowserTest', 'Page_Unit', function() {
  runMochaSuite('Page_Unit');
});
