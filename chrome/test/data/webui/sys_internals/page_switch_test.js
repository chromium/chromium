// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var PageTest = PageTest || {};

PageTest.Switch = function() {
  suite('Page switch integration test', function() {
    suiteSetup('Wait for the page initialize.', function() {
      const testData = TestUtil.getTestData([
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
      return SysInternals.promiseResolvers.waitSysInternalsInitialized.promise
          .then(function() {
            SysInternals.handleUpdateData(testData, 1000);
            return Promise.resolve();
          });
    });

    function checkInfoPage() {
      assertEquals(location.hash, SysInternals.PAGE_HASH.INFO);
      assertEquals($('drawer-title').innerText, 'Info');
      assertFalse($('infopage-root').hasAttribute('hidden'));
      assertFalse(SysInternals.lineChart.shouldRender());
    }

    function checkChartPage(hash) {
      assertEquals(location.hash, hash);
      assertEquals($('drawer-title').innerText, hash.slice(1));
      assertTrue($('infopage-root').hasAttribute('hidden'));

      const lineChart = SysInternals.lineChart;
      assertTrue(lineChart.shouldRender());
      const PAGE_HASH = SysInternals.PAGE_HASH;
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
      const promiseResolvers = SysInternals.promiseResolvers;
      promiseResolvers.waitOnHashChangeCompleted = new PromiseResolver();
      clickDrawerBtn(btnIndex);
      return promiseResolvers.waitOnHashChangeCompleted.promise.then(
          function() {
            if (hash === SysInternals.PAGE_HASH.INFO) {
              checkInfoPage();
            } else {
              checkChartPage(hash);
            }
            return Promise.resolve();
          });
    }

    test('Switch test', function() {
      assertTrue(SysInternals.isInfoPage());
      const PAGE_HASH = SysInternals.PAGE_HASH;
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

  mocha.run();
};
