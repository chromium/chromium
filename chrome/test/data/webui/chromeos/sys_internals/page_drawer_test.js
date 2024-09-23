// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {$} from 'chrome://resources/js/util.js';
import {closeDrawer, initialize, openDrawer, promiseResolvers} from 'chrome://sys-internals/index.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('Page_Drawer', function() {
  suiteSetup('Wait for the page initialize.', function() {
    initialize();
    return promiseResolvers.waitSysInternalsInitialized.promise;
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
    promiseResolvers.waitDrawerActionCompleted = new PromiseResolver();
    action();
    return promiseResolvers.waitDrawerActionCompleted.promise.then(function() {
      checker();
      return Promise.resolve();
    });
  }

  test('open and close by SysInternals function', function() {
    return operate(openDrawer, checkOpen)
        .then(function() {
          return operate(closeDrawer, checkClose);
        })
        .then(function() {
          return operate(openDrawer, checkOpen);
        })
        .then(function() {
          return operate(closeDrawer, checkClose);
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

// mocha.run();
