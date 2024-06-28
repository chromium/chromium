// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DataSeries} from 'chrome://sys-internals/line_chart/data_series.js';
import {Menu} from 'chrome://sys-internals/line_chart/menu.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('LineChart_Menu', function() {
  test('Menu integration test', function() {
    const menu = new Menu(function() {});
    const data1 = new DataSeries('test1', '#aabbcc');
    const data2 = new DataSeries('test2', '#aabbcc');
    const data3 = new DataSeries('test3', '#aabbcc');

    menu.addDataSeries(data1);
    menu.addDataSeries(data2);
    menu.addDataSeries(data3);
    /* Add second time won't do nothing. */
    menu.addDataSeries(data3);
    assertEquals(menu.dataSeries_.length, 3);
    assertEquals(menu.buttons_.length, 3);

    const buttons = menu.buttons_;
    buttons[0].click();
    assertFalse(data1.isVisible());
    buttons[2].click();
    assertFalse(data3.isVisible());
    buttons[0].click();
    assertTrue(data1.isVisible());

    assertFalse(menu.buttonOuterDiv_.hasAttribute('hidden'));
    menu.handleDiv_.click();
    assertTrue(menu.buttonOuterDiv_.hasAttribute('hidden'));
    menu.handleDiv_.click();
    assertFalse(menu.buttonOuterDiv_.hasAttribute('hidden'));

    menu.removeDataSeries(data1);
    menu.removeDataSeries(data3);
    menu.removeDataSeries(data2);
    /* Remove second time won't do nothing. */
    menu.removeDataSeries(data2);
    assertEquals(menu.dataSeries_.length, 0);
    assertEquals(menu.buttons_.length, 0);
  });
});
