// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://welcome/set_as_default/nux_set_as_default.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {NuxSetAsDefaultProxyImpl} from 'chrome://welcome/set_as_default/nux_set_as_default_proxy.js';

import {eventToPromise} from '../test_util.m.js';

import {TestNuxSetAsDefaultProxy} from './test_nux_set_as_default_proxy.js';

suite('SetAsDefaultTest', function() {
  /** @type {NuxSetAsDefaultElement} */
  let testElement;

  /** @type {NuxSetAsDefaultProxy} */
  let testSetAsDefaultProxy;

  /** @type {!Promise} */
  let navigatedPromise;

  setup(function() {
    testSetAsDefaultProxy = new TestNuxSetAsDefaultProxy();
    NuxSetAsDefaultProxyImpl.instance_ = testSetAsDefaultProxy;

    PolymerTest.clearBody();
    testElement = document.createElement('nux-set-as-default');
    document.body.appendChild(testElement);
    let navigateToNextStep;
    navigatedPromise = new Promise(resolve => {
      // Spy on navigational function to make sure it's called.
      navigateToNextStep = () => resolve();
    });
    testElement.navigateToNextStep_ = navigateToNextStep;
  });

  teardown(function() {
    testElement.remove();
  });

  test('skip', function() {
    testElement.$['decline-button'].click();
    return testSetAsDefaultProxy.whenCalled('recordSkip');
  });

  test(
      'click set-default button and finishes setting default',
      async function() {
        testElement.$$('.action-button').click();

        await Promise.all([
          testSetAsDefaultProxy.whenCalled('recordBeginSetDefault'),
          testSetAsDefaultProxy.whenCalled('setAsDefault'),
        ]);

        const notifyPromise =
            eventToPromise('default-browser-change', testElement);

        webUIListenerCallback(
            'browser-default-state-changed', {isDefault: true});

        return Promise.all([
          notifyPromise,
          testSetAsDefaultProxy.whenCalled('recordSuccessfullySetDefault'),
          navigatedPromise
        ]);
      });

  test('click set-default button but gives up and skip', async function() {
    testElement.$$('.action-button').click();

    await Promise.all([
      testSetAsDefaultProxy.whenCalled('recordBeginSetDefault'),
      testSetAsDefaultProxy.whenCalled('setAsDefault'),
    ]);

    testElement.$['decline-button'].click();

    return Promise.all([
      testSetAsDefaultProxy.whenCalled('recordSkip'),
      navigatedPromise,
    ]);
  });
});
