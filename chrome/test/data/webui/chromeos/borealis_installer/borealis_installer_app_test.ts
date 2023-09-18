// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://borealis-installer/app.js';

import {BorealisInstallerAppElement} from 'chrome://borealis-installer/app.js';
import {PageCallbackRouter, PageHandlerRemote, PageRemote} from 'chrome://borealis-installer/borealis_installer.mojom-webui.js';
import {BrowserProxy} from 'chrome://borealis-installer/browser_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

class FakeBrowserProxy extends TestBrowserProxy implements BrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  page: PageRemote;
  constructor() {
    super([
      'install',
      'shutDown',
      'launch',
      'onPageClosed',
    ]);
    this.handler = TestMock.fromClass(PageHandlerRemote);
    this.callbackRouter = new PageCallbackRouter();
    this.page = this.callbackRouter.$.bindNewPipeAndPassRemote();
  }

  install() {
    this.methodCalled('install');
  }

  shutDown() {
    this.methodCalled('shutDown');
  }

  launch() {
    this.methodCalled('launch');
  }


  onPageClosed() {
    this.methodCalled('onPageClosed');
  }
}

suite('<borealis-installer-app>', async () => {
  let fakeBrowserProxy: FakeBrowserProxy;
  let app: BorealisInstallerAppElement;

  setup(async () => {
    fakeBrowserProxy = new FakeBrowserProxy();
    BrowserProxy.setInstance(fakeBrowserProxy);

    app = document.createElement('borealis-installer-app');
    document.body.appendChild(app);

    await flushTasks();
  });

  teardown(function() {
    app.remove();
  });

  function shadowRoot(): ShadowRoot {
    const shadowRoot = app.shadowRoot;
    assertTrue(shadowRoot !== null);
    return shadowRoot;
  }

  const clickButton = async (id: string) => {
    const button = shadowRoot().getElementById(id);
    assertTrue(button != null);
    assertFalse(button.hidden);
    button.click();
    await flushTasks();
  };


  test('install', async () => {
    assertEquals(fakeBrowserProxy.handler.getCallCount('install'), 0);
    await clickButton('install');
    assertEquals(fakeBrowserProxy.handler.getCallCount('install'), 1);

    fakeBrowserProxy.page.onProgressUpdate(0.5, '3 seconds left');
    await flushTasks();
    assertEquals(shadowRoot().querySelector('paper-progress')!.value, 50);
    assertEquals(
        shadowRoot().querySelector('#progress-message')!.textContent!.trim(),
        '50% completed | 3 seconds left');
    fakeBrowserProxy.page.onInstallFinished();
    await flushTasks();

    assertEquals(fakeBrowserProxy.handler.getCallCount('launch'), 0);
    assertEquals(fakeBrowserProxy.handler.getCallCount('onPageClosed'), 0);
    await clickButton('launch');
    assertEquals(fakeBrowserProxy.handler.getCallCount('launch'), 1);
    assertEquals(fakeBrowserProxy.handler.getCallCount('onPageClosed'), 1);
  });

  test('errorDuringInstall', async () => {
    await clickButton('install');

    fakeBrowserProxy.page.onProgressUpdate(0.5, '3 seconds left');
    await flushTasks();
    fakeBrowserProxy.page.restartInstallation();
    await flushTasks();

    assertEquals(fakeBrowserProxy.handler.getCallCount('install'), 2);
    assertEquals(shadowRoot().querySelector('paper-progress')!.value, 0);
    fakeBrowserProxy.page.onInstallFinished();
    await flushTasks();

    await clickButton('launch');
    assertEquals(fakeBrowserProxy.handler.getCallCount('launch'), 1);
    assertEquals(fakeBrowserProxy.handler.getCallCount('onPageClosed'), 1);
  });

  test('cancelBeforeLaunch', async () => {
    assertEquals(fakeBrowserProxy.handler.getCallCount('install'), 0);

    await clickButton('install');
    assertEquals(fakeBrowserProxy.handler.getCallCount('install'), 1);

    fakeBrowserProxy.page.restartInstallation();
    await flushTasks();
    assertEquals(fakeBrowserProxy.handler.getCallCount('install'), 2);
    fakeBrowserProxy.page.onInstallFinished();
    await flushTasks();
    await clickButton('cancel');
    assertEquals(fakeBrowserProxy.handler.getCallCount('onPageClosed'), 1);
    assertEquals(fakeBrowserProxy.handler.getCallCount('shutDown'), 1);
  });

  test('cancelDuringInstall', async () => {
    assertEquals(fakeBrowserProxy.handler.getCallCount('install'), 0);

    await clickButton('install');
    assertEquals(fakeBrowserProxy.handler.getCallCount('install'), 1);

    fakeBrowserProxy.page.restartInstallation();
    await flushTasks();
    assertEquals(fakeBrowserProxy.handler.getCallCount('install'), 2);
    fakeBrowserProxy.page.onInstallFinished();
    await flushTasks();
    await clickButton('cancel');
    assertEquals(fakeBrowserProxy.handler.getCallCount('onPageClosed'), 1);
    assertEquals(fakeBrowserProxy.handler.getCallCount('shutDown'), 1);
  });
});
