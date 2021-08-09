// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WhatsNewAppElement} from 'chrome://whats-new/whats_new_app.js';
import {WhatsNewProxyImpl} from 'chrome://whats-new/whats_new_proxy.js';

import {assertFalse, assertTrue} from '../chai_assert.js';
import {TestBrowserProxy} from '../test_browser_proxy.js';
import {flushTasks} from '../test_util.m.js';

class TestWhatsNewProxy extends TestBrowserProxy {
  /**
   * @param {boolean=} shouldLoad Whether initialize() should load the test
   *     URL. Defaults to true.
   */
  constructor(shouldLoad = true) {
    super([
      'initialize',
    ]);

    /** @private {boolean} */
    this.shouldLoad_ = shouldLoad;
  }

  /** @override */
  initialize(isAuto) {
    this.methodCalled('initialize', isAuto);
    return this.shouldLoad_ ?
        Promise.resolve('chrome://test/whats_new/test.html') :
        Promise.resolve();
  }
}

suite('WhatsNewAppTest', function() {
  setup(function() {
    document.body.innerHTML = '';
  });

  test('with query parameter', async () => {
    const proxy = new TestWhatsNewProxy();
    WhatsNewProxyImpl.setInstance(proxy);
    window.history.replaceState({}, '', '?auto=true');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);
    const isAuto = await proxy.whenCalled('initialize');
    assertTrue(isAuto);
    await flushTasks();

    const iframe = whatsNewApp.shadowRoot.querySelector('#content');
    assertTrue(!!iframe);
    // iframe has latest=true URL query parameter.
    assertEquals('chrome://test/whats_new/test.html?latest=true', iframe.src);
    const errorPage =
        whatsNewApp.shadowRoot.querySelector('whats-new-error-page');
    assertFalse(!!errorPage);
  });

  test('no query parameter', async () => {
    const proxy = new TestWhatsNewProxy();
    WhatsNewProxyImpl.setInstance(proxy);
    window.history.replaceState({}, '', '/');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);
    const isAuto = await proxy.whenCalled('initialize');
    assertFalse(isAuto);
    await flushTasks();

    const iframe = whatsNewApp.shadowRoot.querySelector('#content');
    assertTrue(!!iframe);
    assertEquals('chrome://test/whats_new/test.html', iframe.src);
    const errorPage =
        whatsNewApp.shadowRoot.querySelector('whats-new-error-page');
    assertFalse(!!errorPage);
  });

  test('no query parameter failure', async () => {
    // Simulate a failure to load the remote content.
    const proxy = new TestWhatsNewProxy(false);
    WhatsNewProxyImpl.setInstance(proxy);
    window.history.replaceState({}, '', '/');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);
    const isAuto = await proxy.whenCalled('initialize');
    assertFalse(isAuto);
    await flushTasks();

    // Shows the error page in place of the iframe.
    const iframe = whatsNewApp.shadowRoot.querySelector('#content');
    assertFalse(!!iframe);
    const errorPage =
        whatsNewApp.shadowRoot.querySelector('whats-new-error-page');
    assertTrue(!!errorPage);
  });
});
