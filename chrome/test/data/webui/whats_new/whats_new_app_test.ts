// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://whats-new/whats_new_app.js';

import {CommandHandlerRemote} from 'chrome://resources/js/browser_command/browser_command.mojom-webui.js';
import {BrowserCommandProxy} from 'chrome://resources/js/browser_command/browser_command_proxy.js';
import {isChromeOS} from 'chrome://resources/js/platform.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
import {WhatsNewProxy, WhatsNewProxyImpl} from 'chrome://whats-new/whats_new_proxy.js';

const whatsNewURL = 'chrome://webui-test/whats_new/test.html';

class TestWhatsNewProxy extends TestBrowserProxy implements WhatsNewProxy {
  private url_: string;

  /**
   * @param url The URL to load in the iframe.
   */
  constructor(url: string) {
    super([
      'initialize',
    ]);

    this.url_ = url;
  }

  initialize() {
    this.methodCalled('initialize');
    return Promise.resolve(this.url_);
  }
}

suite('WhatsNewAppTest', function() {
  const whatsNewWithCommandURL =
      'chrome://webui-test/whats_new/test_with_command_3.html';

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('with query parameters', async () => {
    const proxy = new TestWhatsNewProxy(whatsNewURL);
    WhatsNewProxyImpl.setInstance(proxy);
    window.history.replaceState({}, '', '?auto=true');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);
    await proxy.whenCalled('initialize');
    await flushTasks();

    const iframe =
        whatsNewApp.shadowRoot!.querySelector<HTMLIFrameElement>('#content');
    assertTrue(!!iframe);
    // iframe has latest=true URL query parameter except on CrOS
    assertEquals(
        whatsNewURL + (isChromeOS ? '?latest=false' : '?latest=true'),
        iframe.src);
  });

  test('with version as query parameter', async () => {
    const proxy = new TestWhatsNewProxy(whatsNewURL + '?version=m98');
    WhatsNewProxyImpl.setInstance(proxy);
    window.history.replaceState({}, '', '?auto=true');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);
    await proxy.whenCalled('initialize');
    await flushTasks();

    const iframe =
        whatsNewApp.shadowRoot!.querySelector<HTMLIFrameElement>('#content');
    assertTrue(!!iframe);
    // iframe has latest=true URL query parameter except on CrOS
    assertEquals(
        whatsNewURL + '?version=m98' +
            (isChromeOS ? '&latest=false' : '&latest=true'),
        iframe.src);
  });

  test('no query parameters', async () => {
    const proxy = new TestWhatsNewProxy(whatsNewURL);
    WhatsNewProxyImpl.setInstance(proxy);
    window.history.replaceState({}, '', '/');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);
    await proxy.whenCalled('initialize');
    await flushTasks();

    const iframe =
        whatsNewApp.shadowRoot!.querySelector<HTMLIFrameElement>('#content');
    assertTrue(!!iframe);
    assertEquals(whatsNewURL + '?latest=false', iframe.src);
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
