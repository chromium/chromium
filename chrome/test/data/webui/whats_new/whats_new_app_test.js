// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CommandHandlerRemote} from 'chrome://resources/js/browser_command/browser_command.mojom-webui.js';
import {BrowserCommandProxy} from 'chrome://resources/js/browser_command/browser_command_proxy.js';
import {isChromeOS} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WhatsNewAppElement} from 'chrome://whats-new/whats_new_app.js';
import {WhatsNewProxyImpl} from 'chrome://whats-new/whats_new_proxy.js';

import {assertFalse, assertTrue} from '../chai_assert.js';
import {TestBrowserProxy} from '../test_browser_proxy.js';
import {eventToPromise, flushTasks} from '../test_util.js';

class TestWhatsNewProxy extends TestBrowserProxy {
  /**
   * @param {?string} url The URL to load in the iframe or null to simulate a
   *     load failure.
   */
  constructor(url) {
    super([
      'initialize',
    ]);

    /** @private {?string} */
    this.url_ = url;
  }

  /** @override */
  initialize(isAuto) {
    this.methodCalled('initialize', isAuto);
    return Promise.resolve(this.url_);
  }
}

suite('WhatsNewAppTest', function() {
  const whatsNewURL = 'chrome://test/whats_new/test.html';
  const whatsNewWithCommandURL =
      'chrome://test/whats_new/test_with_command_3.html';

  setup(function() {
    document.body.innerHTML = '';
  });

  test('with query parameters', async () => {
    loadTimeData.overrideValues({'showFeedbackButton': true});
    const proxy = new TestWhatsNewProxy(whatsNewURL);
    WhatsNewProxyImpl.setInstance(proxy);
    window.history.replaceState({}, '', '?auto=true');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);
    const isAuto = await proxy.whenCalled('initialize');
    assertEquals(!isChromeOS, isAuto);
    await flushTasks();

    const iframe = whatsNewApp.shadowRoot.querySelector('#content');
    assertTrue(!!iframe);
    // iframe has latest=true URL query parameter except on CrOS
    assertEquals(
        whatsNewURL + (isChromeOS ? '?latest=false' : '?latest=true') +
            '&feedback=true',
        iframe.src);
    const errorPage =
        whatsNewApp.shadowRoot.querySelector('whats-new-error-page');
    assertFalse(!!errorPage);
  });

  test('no query parameters', async () => {
    loadTimeData.overrideValues({'showFeedbackButton': false});
    const proxy = new TestWhatsNewProxy(whatsNewURL);
    WhatsNewProxyImpl.setInstance(proxy);
    window.history.replaceState({}, '', '/');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);
    const isAuto = await proxy.whenCalled('initialize');
    assertFalse(isAuto);
    await flushTasks();

    const iframe = whatsNewApp.shadowRoot.querySelector('#content');
    assertTrue(!!iframe);
    assertEquals(whatsNewURL + '?latest=false&feedback=false', iframe.src);
    const errorPage =
        whatsNewApp.shadowRoot.querySelector('whats-new-error-page');
    assertFalse(!!errorPage);
  });

  test('no query parameter failure', async () => {
    // Simulate a failure to load the remote content.
    const proxy = new TestWhatsNewProxy(null);
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

  test('with command', async () => {
    const proxy = new TestWhatsNewProxy(whatsNewWithCommandURL);
    WhatsNewProxyImpl.setInstance(proxy);
    const browserCommandHandler =
        TestBrowserProxy.fromClass(CommandHandlerRemote);
    BrowserCommandProxy.getInstance().handler = browserCommandHandler;
    browserCommandHandler.setResultFor(
        'canExecuteCommand', Promise.resolve({canExecute: true}));
    window.history.replaceState({}, '', '/');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);

    const whenMessage = eventToPromise('message', window);
    const commandId =
        await browserCommandHandler.whenCalled('canExecuteCommand');
    assertEquals(3, commandId);

    const {data} = await whenMessage;
    assertEquals(3, data.data.commandId);
  });
});
