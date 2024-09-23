// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {$} from 'chrome://resources/js/util.js';
import {PAGE_HASH} from 'chrome://sys-internals/constants.js';
import {handleUpdateData, initialize, isInfoPage, lineChart, promiseResolvers} from 'chrome://sys-internals/index.js';
import {assertEquals, assertFalse, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {getTestData} from './test_util.js';

suite('Page_Switch', function() {
  suiteSetup('Wait for the page initialize.', function() {
    const testData = getTestData([
      {idle: 100, kernel: 100, total: 100, user: 100},
      {idle: 100, kernel: 100, total: 100, user: 100},
      {idle: 100, kernel: 100, total: 100, user: 100},
      {idle: 100, kernel: 100, total: 100, user: 100},
      {idle: 100, kernel: 100, total: 100, user: 100},
      {idle: 100, kernel: 100, total: 100, user: 100},
      {idle: 100, kernel: 100, total: 100, user: 100},
      {idle: 100, kernel: 100, total: 100, user: 100},
      {idle: 100, kernel: 100, total: 100, user: 100},
    ]);
    initialize();
    return promiseResolvers.waitSysInternalsInitialized.promise.then(
        function() {
          handleUpdateData(testData, 1000);
          return Promise.resolve();
        });
  });

  function checkInfoPage() {
    assertEquals(location.hash, PAGE_HASH.INFO);
    assertEquals($('drawer-title').innerText, 'Info');
    assertFalse($('infopage-root').hasAttribute('hidden'));
    assertFalse(lineChart.shouldRender());
  }

  function checkChartPage(hash) {
    assertEquals(location.hash, hash);
    assertEquals($('drawer-title').innerText, hash.slice(1));
    assertTrue($('infopage-root').hasAttribute('hidden'));

    assertTrue(lineChart.shouldRender());
    if (hash === PAGE_HASH.CPU) {
      assertEquals(lineChart.subCharts_[0].dataSeriesList_.length, 0);
      assertEquals(lineChart.subCharts_[1].dataSeriesList_.length, 9);
      assertEquals(lineChart.menu_.buttons_.length, 9);
    } else if (hash === PAGE_HASH.MEMORY) {
      assertEquals(lineChart.subCharts_[0].dataSeriesList_.length, 2);
      assertEquals(lineChart.subCharts_[1].dataSeriesList_.length, 2);
      assertEquals(lineChart.menu_.buttons_.length, 4);
    } else if (hash === PAGE_HASH.ZRAM) {
      assertEquals(lineChart.subCharts_[0].dataSeriesList_.length, 2);
      assertEquals(lineChart.subCharts_[1].dataSeriesList_.length, 3);
      assertEquals(lineChart.menu_.buttons_.length, 5);
    } else {
      assertNotReached();
    }
  }

  function clickDrawerBtn(btnIndex) {
    $('nav-menu-btn').click();
    const infoBtn = document.getElementsByClassName('drawer-item')[btnIndex];
    infoBtn.click();
  }

  function goPage(hash, btnIndex) {
    promiseResolvers.waitOnHashChangeCompleted = new PromiseResolver();
    clickDrawerBtn(btnIndex);
    return promiseResolvers.waitOnHashChangeCompleted.promise.then(function() {
      if (hash === PAGE_HASH.INFO) {
        checkInfoPage();
      } else {
        checkChartPage(hash);
      }
      return Promise.resolve();
    });
  }

  test('Switch test', function() {
    assertTrue(isInfoPage());
    return goPage(PAGE_HASH.CPU, 1)
        .then(function() {
          return goPage(PAGE_HASH.ZRAM, 3);
        })
        .then(function() {
          return goPage(PAGE_HASH.MEMORY, 2);
        })
        .then(function() {
          return goPage(PAGE_HASH.CPU, 1);
        })
        .then(function() {
          return goPage(PAGE_HASH.INFO, 0);
        })
        .then(function() {
          return goPage(PAGE_HASH.MEMORY, 2);
        });
  });
});
