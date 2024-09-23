// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.js';
import {handleUpdateData, lineChart, updateInfoPage} from 'chrome://sys-internals/index.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {getTestData} from './test_util.js';

suite('Page_InfoPage', function() {
  test('check cpu info', function() {
    function getTextById(id) {
      return $(id).innerText;
    }

    assertTrue(window.DONT_SEND_UPDATE_REQUEST);
    updateInfoPage();
    assertEquals(getTextById('infopage-num-of-cpu'), '0');
    assertEquals(getTextById('infopage-cpu-kernel'), '0.00%');
    assertEquals(getTextById('infopage-cpu-usage'), '0.00%');
    assertEquals(getTextById('infopage-memory-total'), '0.00 B');
    assertEquals(getTextById('infopage-memory-used'), '0.00 B');
    assertEquals(getTextById('infopage-memory-swap-used'), '0.00 B');
    assertEquals(getTextById('infopage-zram-orig'), '0.00 B');
    assertEquals(getTextById('infopage-zram-compr'), '0.00 B');
    assertEquals(getTextById('infopage-zram-compr-ratio'), 'NaN%');

    handleUpdateData(
        getTestData([
          {idle: 100, kernel: 100, total: 100, user: 100},
          {idle: 100, kernel: 100, total: 100, user: 100},
          {idle: 100, kernel: 100, total: 100, user: 100},
          {idle: 100, kernel: 100, total: 100, user: 100},
        ]),
        1000);
    assertEquals(getTextById('infopage-num-of-cpu'), '4');
    assertEquals(getTextById('infopage-cpu-kernel'), '0.00%');
    assertEquals(getTextById('infopage-cpu-usage'), '0.00%');
    assertEquals(getTextById('infopage-memory-total'), '8.00 TB');
    assertEquals(getTextById('infopage-memory-used'), '4.00 TB');
    assertEquals(getTextById('infopage-memory-swap-used'), '2.00 TB');
    assertEquals(getTextById('infopage-zram-orig'), '200.00 GB');
    assertEquals(getTextById('infopage-zram-compr'), '100.00 GB');
    assertEquals(getTextById('infopage-zram-compr-ratio'), '50.00%');

    handleUpdateData(
        getTestData([
          {idle: 160, kernel: 120, total: 200, user: 120},
          {idle: 180, kernel: 110, total: 200, user: 110},
          {idle: 140, kernel: 130, total: 200, user: 130},
          {idle: 160, kernel: 120, total: 200, user: 120},
        ]),
        2000);
    assertEquals(getTextById('infopage-cpu-usage'), '40.00%');
    assertEquals(getTextById('infopage-cpu-kernel'), '20.00%');

    handleUpdateData(
        getTestData([
          {idle: 190, kernel: 150, total: 290, user: 150},
          {idle: 210, kernel: 140, total: 290, user: 140},
          {idle: 170, kernel: 160, total: 290, user: 160},
          {idle: 190, kernel: 150, total: 290, user: 150},
        ]),
        2876);
    assertEquals(getTextById('infopage-cpu-usage'), '66.67%');
    assertEquals(getTextById('infopage-cpu-kernel'), '33.33%');

    handleUpdateData(
        getTestData([
          {idle: 200, kernel: 160, total: 320, user: 160},
          {idle: 220, kernel: 150, total: 320, user: 150},
          {idle: 180, kernel: 170, total: 320, user: 170},
          {idle: 200, kernel: 160, total: 320, user: 160},
        ]),
        3999);
    assertEquals(getTextById('infopage-cpu-usage'), '66.67%');
    assertEquals(getTextById('infopage-cpu-kernel'), '33.33%');
  });
});
