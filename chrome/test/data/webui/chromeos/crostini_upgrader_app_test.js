// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://crostini-upgrader/app.js';

import {BrowserProxy} from 'chrome://crostini-upgrader/browser_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

class FakePageHandler extends TestBrowserProxy {
  constructor() {
    super([
      'backup',
      'startPrechecks',
      'upgrade',
      'restore',
      'cancel',
      'cancelBeforeStart',
      'onPageClosed',
      'launch',
    ]);
  }

  /** @override */
  backup() {
    this.methodCalled('backup');
  }

  /** @override */
  startPrechecks() {
    this.methodCalled('startPrechecks');
  }

  /** @override */
  upgrade() {
    this.methodCalled('upgrade');
  }

  /** @override */
  restore() {
    this.methodCalled('restore');
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
  onPageClosed() {
    this.methodCalled('onPageClosed');
  }

  /** @override */
  launch() {
    this.methodCalled('launch');
  }
}

class FakeBrowserProxy {
  constructor() {
    this.handler = new FakePageHandler();
    this.callbackRouter = new ash.crostiniUpgrader.mojom.PageCallbackRouter();
    /** @type {appManagement.mojom.PageRemote} */
    this.page = this.callbackRouter.$.bindNewPipeAndPassRemote();
  }
}

suite('<crostini-upgrader-app>', () => {
  let fakeBrowserProxy;
  let app;

  setup(async () => {
    fakeBrowserProxy = new FakeBrowserProxy();
    BrowserProxy.setInstance(fakeBrowserProxy);

    app = document.createElement('crostini-upgrader-app');
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

  const getActionButton = () => {
    return app.$$('.action-button');
  };

  const getCancelButton = () => {
    return app.$$('#cancel-button');
  };

  const clickAction = async () => {
    await clickButton(getActionButton());
  };

  const clickCancel = async () => {
    await clickButton(getCancelButton());
  };

  const getBackupProgressBar = () => {
    return app.$$('#backup-progress-bar');
  };

  const getUpgradeProgressBar = () => {
    return app.$$('#upgrade-progress-bar');
  };

  const getRestoreProgressBar = () => {
    return app.$$('#restore-progress-bar');
  };

  const getProgressMessage = () => {
    return app.$$('#progress-message');
  };

  const waitMillis = (millis) => {
    return new Promise(resolve => {
      setTimeout(() => {
        resolve();
      }, millis);
    });
  };

  test('upgradeFlow', async () => {
    assertFalse(getProgressMessage().hidden);
    assertEquals(fakeBrowserProxy.handler.getCallCount('backup'), 0);

    // The page will not register that backup has started until the first
    // progress message.
    await clickAction();
    fakeBrowserProxy.page.onBackupProgress(0);
    await flushTasks();
    assertFalse(getProgressMessage().hidden);
    assertEquals(fakeBrowserProxy.handler.getCallCount('backup'), 1);
    assertTrue(getActionButton().hidden);
    assertTrue(getCancelButton().hidden);

    fakeBrowserProxy.page.onBackupProgress(50);
    await flushTasks();
    assertTrue(
        !!getProgressMessage().textContent, 'progress message should be set');
    assertFalse(getBackupProgressBar().hidden);
    assertEquals(app.$$('#backup-progress-bar > paper-progress').value, 50);

    fakeBrowserProxy.page.onBackupSucceeded();
    await flushTasks();
    assertEquals(fakeBrowserProxy.handler.getCallCount('upgrade'), 0);
    // The UI pauses for 2000 ms before continuing.
    await waitMillis(2010).then(flushTasks());
    fakeBrowserProxy.page.precheckStatus(
        ash.crostiniUpgrader.mojom.UpgradePrecheckStatus.OK);
    await flushTasks();

    assertEquals(fakeBrowserProxy.handler.getCallCount('upgrade'), 1);
    assertFalse(getUpgradeProgressBar().hidden);
    fakeBrowserProxy.page.onUpgradeProgress(['foo', 'bar']);
    fakeBrowserProxy.page.onUpgradeSucceeded();
    await flushTasks();

    assertEquals(fakeBrowserProxy.handler.getCallCount('onPageClosed'), 0);
    assertTrue(getRestoreProgressBar().hidden);

    await clickAction();
    assertEquals(fakeBrowserProxy.handler.getCallCount('launch'), 1);
    assertEquals(fakeBrowserProxy.handler.getCallCount('onPageClosed'), 1);
  });

  test('upgradeFlowFailureShowsLogs', async () => {
    // Uncheck backup box
    app.$$('#backup-checkbox > cr-checkbox').click();
    await clickAction();

    const kMaxUpgradeAttempts = 3;
    for (let i = 0; i < kMaxUpgradeAttempts; i++) {
      fakeBrowserProxy.page.precheckStatus(
          ash.crostiniUpgrader.mojom.UpgradePrecheckStatus.OK);
      await flushTasks();
      assertEquals(fakeBrowserProxy.handler.getCallCount('upgrade'), i + 1);
      fakeBrowserProxy.page.onUpgradeProgress(['foo', 'bar']);
      fakeBrowserProxy.page.onUpgradeFailed();
      await flushTasks();
    }

    const single = 'foo\nbar';
    assertFalse(app.$$('#upgrade-error-message').hidden);
    assertEquals(
        app.$$('#error-log').innerHTML, single + '\n' + single + '\n' + single);
  });

  test('upgradeFlowPrecheckRetry', async () => {
    // Uncheck backup box
    app.$$('#backup-checkbox > cr-checkbox').click();
    await clickAction();

    assertEquals(fakeBrowserProxy.handler.getCallCount('startPrechecks'), 1);
    fakeBrowserProxy.page.precheckStatus(
        ash.crostiniUpgrader.mojom.UpgradePrecheckStatus.NETWORK_FAILURE);
    await clickAction();

    assertEquals(fakeBrowserProxy.handler.getCallCount('startPrechecks'), 2);
    fakeBrowserProxy.page.precheckStatus(
        ash.crostiniUpgrader.mojom.UpgradePrecheckStatus.LOW_POWER);
    await clickAction();

    assertEquals(fakeBrowserProxy.handler.getCallCount('startPrechecks'), 3);
    fakeBrowserProxy.page.precheckStatus(
        ash.crostiniUpgrader.mojom.UpgradePrecheckStatus.OK);
    await clickAction();

    assertEquals(fakeBrowserProxy.handler.getCallCount('upgrade'), 1);
  });

  test('upgradeFlowFailureOffersRestore', async () => {
    assertFalse(getProgressMessage().hidden);
    assertEquals(fakeBrowserProxy.handler.getCallCount('backup'), 0);

    // The page will not register that backup has started until the first
    // progress message.
    await clickAction();
    fakeBrowserProxy.page.onBackupProgress(0);
    await flushTasks();
    assertFalse(getProgressMessage().hidden);
    assertEquals(fakeBrowserProxy.handler.getCallCount('backup'), 1);
    assertTrue(getActionButton().hidden);
    assertTrue(getCancelButton().hidden);

    fakeBrowserProxy.page.onBackupSucceeded();
    await flushTasks();
    assertEquals(fakeBrowserProxy.handler.getCallCount('upgrade'), 0);
    // The UI pauses for 2000 ms before continuing.
    await waitMillis(2010).then(flushTasks());

    const kMaxUpgradeAttempts = 3;
    for (let i = 0; i < kMaxUpgradeAttempts; i++) {
      fakeBrowserProxy.page.precheckStatus(
          ash.crostiniUpgrader.mojom.UpgradePrecheckStatus.OK);
      await flushTasks();
      assertEquals(fakeBrowserProxy.handler.getCallCount('upgrade'), i + 1);
      assertFalse(getUpgradeProgressBar().hidden);
      fakeBrowserProxy.page.onUpgradeProgress(['foo', 'bar']);
      fakeBrowserProxy.page.onUpgradeFailed();
      await flushTasks();
    }

    assertEquals(fakeBrowserProxy.handler.getCallCount('restore'), 0);
    await clickAction();
    assertEquals(fakeBrowserProxy.handler.getCallCount('restore'), 1);
    fakeBrowserProxy.page.onRestoreProgress(50);
    await flushTasks();
    assertTrue(
        !!getProgressMessage().textContent, 'progress message should be set');
    assertFalse(getRestoreProgressBar().hidden);
    assertEquals(app.$$('#restore-progress-bar > paper-progress').value, 50);
    fakeBrowserProxy.page.onRestoreSucceeded();
    await flushTasks();

    await clickCancel();
    assertEquals(fakeBrowserProxy.handler.getCallCount('launch'), 1);
    assertEquals(fakeBrowserProxy.handler.getCallCount('onPageClosed'), 1);
  });
});
