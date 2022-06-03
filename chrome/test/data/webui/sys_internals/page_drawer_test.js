// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var PageTest = PageTest || {};

PageTest.Drawer = function() {
  suite('Page drawer integration test', function() {
    suiteSetup('Wait for the page initialize.', function() {
      return SysInternals.promiseResolvers.waitSysInternalsInitialized.promise;
    });

    function checkOpen() {
      assertFalse($('sys-internals-drawer').hasAttribute('hidden'));
      assertFalse($('sys-internals-drawer').classList.contains('hidden'));
      assertFalse($('drawer-menu').classList.contains('hidden'));
    }

    function checkClose() {
      assertTrue($('sys-internals-drawer').hasAttribute('hidden'));
      assertTrue($('sys-internals-drawer').classList.contains('hidden'));
      assertTrue($('drawer-menu').classList.contains('hidden'));
    }

    function operate(action, checker) {
      const promiseResolvers = SysInternals.promiseResolvers;
      promiseResolvers.waitDrawerActionCompleted = new PromiseResolver();
      action();
      return promiseResolvers.waitDrawerActionCompleted.promise.then(
          function() {
            checker();
            return Promise.resolve();
          });
    }

    test('open and close by SysInternals function', function() {
      return operate(SysInternals.openDrawer, checkOpen)
          .then(function() {
            return operate(SysInternals.closeDrawer, checkClose);
          })
          .then(function() {
            return operate(SysInternals.openDrawer, checkOpen);
          })
          .then(function() {
            return operate(SysInternals.closeDrawer, checkClose);
          });
    });

    function openByButton() {
      $('nav-menu-btn').click();
    }

    function closeByClickBackground() {
      $('sys-internals-drawer').click();
    }

    function closeByClickInfoPageButton() {
      const infoPageBtn = document.getElementsByClassName('drawer-item')[0];
      infoPageBtn.click();
    }

    test('Tap to open and close', function() {
      return operate(openByButton, checkOpen)
          .then(function() {
            return operate(closeByClickBackground, checkClose);
          })
          .then(function() {
            return operate(openByButton, checkOpen);
          })
          .then(function() {
            return operate(closeByClickInfoPageButton, checkClose);
          });
    });
  });

  mocha.run();
};
