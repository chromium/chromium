// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://welcome/set_as_default/nux_set_as_default.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
import type {NuxSetAsDefaultElement} from 'chrome://welcome/set_as_default/nux_set_as_default.js';
import {NuxSetAsDefaultProxyImpl} from 'chrome://welcome/set_as_default/nux_set_as_default_proxy.js';

import {TestNuxSetAsDefaultProxy} from './test_nux_set_as_default_proxy.js';

suite('SetAsDefaultTest', function() {
  let testElement: NuxSetAsDefaultElement;
  let testSetAsDefaultProxy: TestNuxSetAsDefaultProxy;
  let navigatedPromise: Promise<void>;

  setup(function() {
    testSetAsDefaultProxy = new TestNuxSetAsDefaultProxy();
    NuxSetAsDefaultProxyImpl.setInstance(testSetAsDefaultProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('nux-set-as-default');
    document.body.appendChild(testElement);
    navigatedPromise = new Promise(resolve => {
      // Spy on navigational function to make sure it's called.
      testElement.navigateToNextStep = resolve;
    });
  });

  teardown(function() {
    testElement.remove();
  });

  test('skip', function() {
    testElement.$.declineButton.click();
    return testSetAsDefaultProxy.whenCalled('recordSkip');
  });

  test(
      'click set-default button and finishes setting default',
      async function() {
        testElement.shadowRoot!
            .querySelector<CrButtonElement>('.action-button')!.click();

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
          navigatedPromise,
        ]);
      });

  test('click set-default button but gives up and skip', async function() {
    testElement.shadowRoot!.querySelector<CrButtonElement>(
                               '.action-button')!.click();

    await Promise.all([
      testSetAsDefaultProxy.whenCalled('recordBeginSetDefault'),
      testSetAsDefaultProxy.whenCalled('setAsDefault'),
    ]);

    testElement.$.declineButton.click();

    return Promise.all([
      testSetAsDefaultProxy.whenCalled('recordSkip'),
      navigatedPromise,
    ]);
  });
});
