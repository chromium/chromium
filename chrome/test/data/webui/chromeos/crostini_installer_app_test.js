// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://crostini-installer/app.js';

import {BrowserProxy} from 'chrome://crostini-installer/browser_proxy.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

const InstallerState = crostini.mojom.InstallerState;
const InstallerError = crostini.mojom.InstallerError;

class FakePageHandler extends TestBrowserProxy {
  constructor() {
    super(['install', 'cancel', 'cancelBeforeStart', 'close']);
  }

  /** @override */
  install() {
    this.methodCalled('install');
  }

  /** @override */
  cancel() {
    this.methodCalled('cancel');
  }

  /** @override */
  cancelBeforeStart() {
    this.methodCalled('cancelBeforeStart');
  }

  /** @override */
  close() {
    this.methodCalled('close');
  }
}

class FakeBrowserProxy {
  constructor() {
    this.handler = new FakePageHandler();
    this.callbackRouter =
        new chromeos.crostiniInstaller.mojom.PageCallbackRouter();
    /** @type {appManagement.mojom.PageRemote} */
    this.page = this.callbackRouter.$.bindNewPipeAndPassRemote();
  }
}

suite('<crostini-installer-app>', () => {
  let fakeBrowserProxy;
  let app;

  setup(async () => {
    fakeBrowserProxy = new FakeBrowserProxy();
    BrowserProxy.instance_ = fakeBrowserProxy;

    app = document.createElement('crostini-installer-app');
    PolymerTest.clearBody();
    document.body.appendChild(app);

    await flushTasks();
  });

  teardown(function() {
    app.remove();
  });

  const clickButton = async (button) => {
    assertFalse(button.hidden);
    assertFalse(button.disabled);
    button.click();
    await flushTasks();
  };

  const getInstallButton = () => {
    return app.$$('.action-button');
  };

  const getCancelButton = () => {
    return app.$$('.cancel-button');
  };

  const clickInstall = async () => {
    await clickButton(getInstallButton());
  };

  const clickCancel = async () => {
    await clickButton(getCancelButton());
  };

  test('installFlow', async () => {
    expectFalse(app.$$('#prompt-message').hidden);
    expectEquals(fakeBrowserProxy.handler.getCallCount('install'), 0);

    await clickInstall();
    expectFalse(app.$$('#installing-message').hidden);
    expectEquals(fakeBrowserProxy.handler.getCallCount('install'), 1);
    expectTrue(getInstallButton().hidden);

    fakeBrowserProxy.page.onProgressUpdate(InstallerState.kStartConcierge, 0.5);
    await flushTasks();
    expectTrue(
        !!app.$$('#installing-message > div').textContent,
        'progress message should be set');
    expectEquals(
        app.$$('#installing-message > paper-progress').getAttribute('value'),
        '50');

    expectEquals(fakeBrowserProxy.handler.getCallCount('close'), 0);
    fakeBrowserProxy.page.onInstallFinished(InstallerError.kNone);
    await flushTasks();
    expectEquals(fakeBrowserProxy.handler.getCallCount('close'), 1);
  });

  test('errorCancel', async () => {
    await clickInstall();
    fakeBrowserProxy.page.onInstallFinished(InstallerError.kErrorOffline);
    await flushTasks();
    expectFalse(app.$$('#error-message').hidden);
    expectTrue(
        !!app.$$('#error-message > div').textContent,
        'error message should be set');

    await clickCancel();
    expectEquals(fakeBrowserProxy.handler.getCallCount('close'), 1);
    expectEquals(fakeBrowserProxy.handler.getCallCount('cancelBeforeStart'), 0);
    expectEquals(fakeBrowserProxy.handler.getCallCount('cancel'), 0);
  });

  test('errorRetry', async () => {
    await clickInstall();
    fakeBrowserProxy.page.onInstallFinished(InstallerError.kErrorOffline);
    await flushTasks();
    expectFalse(app.$$('#error-message').hidden);
    expectTrue(
        !!app.$$('#error-message > div').textContent,
        'error message should be set');

    await clickInstall();
    expectEquals(fakeBrowserProxy.handler.getCallCount('install'), 2);
  });

  test('cancelBeforeStart', async () => {
    await clickCancel();
    expectEquals(fakeBrowserProxy.handler.getCallCount('cancelBeforeStart'), 1);
    expectEquals(fakeBrowserProxy.handler.getCallCount('close'), 1);
    expectEquals(fakeBrowserProxy.handler.getCallCount('cancel'), 0);
  });

  test('cancelAfterStart', async () => {
    await clickInstall();
    await clickCancel();
    expectEquals(fakeBrowserProxy.handler.getCallCount('cancel'), 1);
    expectEquals(
        fakeBrowserProxy.handler.getCallCount('close'), 0,
        'should not close until onCanceled is called');
    expectTrue(getInstallButton().hidden);
    expectTrue(getCancelButton().disabled);

    fakeBrowserProxy.page.onCanceled();
    await flushTasks();
    expectEquals(fakeBrowserProxy.handler.getCallCount('close'), 1);
  });
});
