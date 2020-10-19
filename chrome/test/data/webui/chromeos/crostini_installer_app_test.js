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
    super([
      'install', 'cancel', 'cancelBeforeStart', 'onPageClosed',
      'requestAmountOfFreeDiskSpace'
    ]);
  }

  /** @override */
  install(diskSize, username) {
    this.methodCalled('install', [diskSize, username]);
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
  requestAmountOfFreeDiskSpace() {
    this.methodCalled('requestAmountOfFreeDiskSpace');
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
    return app.$$('#install');
  };

  const getCancelButton = () => {
    return app.$$('.cancel-button');
  };

  const clickNext = async () => {
    await clickButton(app.$.next);
  };

  const clickInstall = async () => {
    await clickButton(getInstallButton());
  };

  const clickCancel = async () => {
    await clickButton(getCancelButton());
  };

  const clickCustomSize = async () => {
    await clickButton(app.$$('#custom-size'));
  };

  /**
   * Checks whether a given element is hidden.
   * @param {!Element} element
   * @returns {boolean}
   */
  function isHidden(element) {
    return (
        !element || element.getBoundingClientRect().width <= 0 ||
        element.hidden);
  }

  const diskTicks = [
    {value: 1000, ariaValue: '1', label: '1'},
    {value: 2000, ariaValue: '2', label: '2'}
  ];

  test('installFlow', async () => {
    expectFalse(app.$$('#prompt-message').hidden);
    expectEquals(fakeBrowserProxy.handler.getCallCount('install'), 0);

    // It should wait for disk info to be available.
    await clickNext();
    await flushTasks();
    expectFalse(app.$$('#prompt-message').hidden);

    fakeBrowserProxy.page.onAmountOfFreeDiskSpace(diskTicks, 0, false);
    await flushTasks();
    expectFalse(app.$$('#configure-message').hidden);
    await clickCancel();  // Back to the prompt page.
    expectFalse(app.$$('#prompt-message').hidden);

    await clickNext();
    await flushTasks();
    expectFalse(app.$$('#configure-message').hidden);
    await clickInstall();
    await fakeBrowserProxy.handler.whenCalled('install').then(
        ([diskSize, username]) => {
          assertEquals(
              username, loadTimeData.getString('defaultContainerUsername'));
        });
    expectFalse(app.$$('#installing-message').hidden);
    expectEquals(fakeBrowserProxy.handler.getCallCount('install'), 1);
    expectTrue(getInstallButton().hidden);

    fakeBrowserProxy.page.onProgressUpdate(
        InstallerState.kCreateDiskImage, 0.5);
    await flushTasks();
    expectTrue(
        !!app.$$('#installing-message > div').textContent.trim(),
        'progress message should be set');
    expectEquals(
        app.$$('#installing-message > paper-progress').getAttribute('value'),
        '50');

    expectEquals(fakeBrowserProxy.handler.getCallCount('onPageClosed'), 0);
    fakeBrowserProxy.page.onInstallFinished(InstallerError.kNone);
    await flushTasks();
    expectEquals(fakeBrowserProxy.handler.getCallCount('onPageClosed'), 1);
  });

  // We only proceed to the config page if disk info is available. Let's make
  // sure if the user click the next button multiple time very soon it dose not
  // blow up.
  test('multipleClickNextBeforeDiskAvailable', async () => {
    expectFalse(app.$$('#prompt-message').hidden);

    // It should wait for disk info to be available.
    await clickNext();
    await clickNext();
    await clickNext();
    await flushTasks();
    expectFalse(app.$$('#prompt-message').hidden);

    fakeBrowserProxy.page.onAmountOfFreeDiskSpace(diskTicks, 0, false);
    await flushTasks();
    // Enter configure page as usual
    expectFalse(app.$$('#configure-message').hidden);

    // Can back to prompt page as usual.
    await clickCancel();
    expectFalse(app.$$('#prompt-message').hidden);

    await clickNext();
    await flushTasks();
    // Re-enter configure page as usual
    expectFalse(app.$$('#configure-message').hidden);
  });

  test('straightToErrorPageIfMinDiskUnmet', async () => {
    expectFalse(app.$$('#prompt-message').hidden);

    fakeBrowserProxy.page.onAmountOfFreeDiskSpace([], 0, false);

    await clickNext();
    await flushTasks();
    expectFalse(app.$$('#error-message').hidden);
    expectTrue(
        !!app.$$('#error-message > div').textContent.trim(),
        'error message should be set');
    // We do not show retry button in this case.
    assertTrue(getInstallButton().hidden);
  });

  test('showWarningIfLowFreeSpace', async () => {
    expectFalse(app.$$('#prompt-message').hidden);

    fakeBrowserProxy.page.onAmountOfFreeDiskSpace(diskTicks, 0, true);

    await clickNext();
    await flushTasks();
    expectFalse(app.$$('#configure-message').hidden);
    expectFalse(isHidden(app.$$('#low-free-space-warning')));
  });

  diskTicks.forEach(async (_, defaultIndex) => {
    test(`configDiskSpaceWithDefault-${defaultIndex}`, async () => {
      expectFalse(app.$$('#prompt-message').hidden);

      fakeBrowserProxy.page.onAmountOfFreeDiskSpace(
          diskTicks, defaultIndex, false);

      await clickNext();
      await flushTasks();

      expectFalse(app.$$('#configure-message').hidden);
      expectTrue(isHidden(app.$$('#low-free-space-warning')));
      expectTrue(isHidden(app.$$('#diskSlider')));

      await clickInstall();
      await fakeBrowserProxy.handler.whenCalled('install').then(
          ([diskSize, username]) => {
            assertEquals(diskSize, diskTicks[defaultIndex].value);
          });
      expectEquals(fakeBrowserProxy.handler.getCallCount('install'), 1);
    });
  });

  test('configDiskSpaceWithUserSelection', async () => {
    expectFalse(app.$$('#prompt-message').hidden);

    fakeBrowserProxy.page.onAmountOfFreeDiskSpace(diskTicks, 0, false);

    await clickNext();
    await flushTasks();
    await clickCustomSize();
    await flushTasks();

    expectFalse(app.$$('#configure-message').hidden);
    expectTrue(isHidden(app.$$('#low-free-space-warning')));
    expectFalse(isHidden(app.$$('#diskSlider')));

    app.$$('#diskSlider').value = 1;

    await clickInstall();
    await fakeBrowserProxy.handler.whenCalled('install').then(
        ([diskSize, username]) => {
          assertEquals(diskSize, diskTicks[1].value);
        });
    expectEquals(fakeBrowserProxy.handler.getCallCount('install'), 1);
  });

  test('configUsername', async () => {
    fakeBrowserProxy.page.onAmountOfFreeDiskSpace(diskTicks, 0, false);
    await clickNext();

    expectEquals(
        app.$.username.value,
        loadTimeData.getString('defaultContainerUsername'));

    // Test invalid usernames
    const invalidUsernames = [
      'root',   // Unavailable.
      '0abcd',  // Invalid first character.
      'aBcd',   // Invalid (uppercase) character.
    ];

    for (const username of invalidUsernames) {
      app.$.username.value = username;

      await flushTasks();
      expectTrue(app.$.username.invalid);
      expectTrue(!!app.$.username.errorMessage);
      expectTrue(app.$.install.disabled);
    }

    // Test the empty username. The username field should not show an error, but
    // we want the install button to be disabled.
    app.$.username.value = '';
    await flushTasks();
    expectFalse(app.$.username.invalid);
    expectFalse(!!app.$.username.errorMessage);
    expectTrue(app.$.install.disabled);

    // Test a valid username
    const validUsername = 'totally-valid_username';
    app.$.username.value = validUsername;
    await flushTasks();
    expectFalse(app.$.username.invalid);
    clickInstall();
    await fakeBrowserProxy.handler.whenCalled('install').then(
        ([diskSize, username]) => {
          assertEquals(username, validUsername);
        });
    expectEquals(fakeBrowserProxy.handler.getCallCount('install'), 1);
  });

  test('errorCancel', async () => {
    fakeBrowserProxy.page.onAmountOfFreeDiskSpace(diskTicks, 0, false);
    await clickNext();
    await clickInstall();
    fakeBrowserProxy.page.onInstallFinished(InstallerError.kErrorOffline);
    await flushTasks();
    expectFalse(app.$$('#error-message').hidden);
    expectTrue(
        !!app.$$('#error-message > div').textContent.trim(),
        'error message should be set');

    await clickCancel();
    expectEquals(fakeBrowserProxy.handler.getCallCount('onPageClosed'), 1);
    expectEquals(fakeBrowserProxy.handler.getCallCount('cancelBeforeStart'), 0);
    expectEquals(fakeBrowserProxy.handler.getCallCount('cancel'), 0);
  });

  test('errorRetry', async () => {
    fakeBrowserProxy.page.onAmountOfFreeDiskSpace(diskTicks, 0, false);
    await clickNext();
    await clickInstall();
    fakeBrowserProxy.page.onInstallFinished(InstallerError.kErrorOffline);
    await flushTasks();
    expectFalse(app.$$('#error-message').hidden);
    expectTrue(
        !!app.$$('#error-message > div').textContent.trim(),
        'error message should be set');

    await clickInstall();
    expectEquals(fakeBrowserProxy.handler.getCallCount('install'), 2);
  });

  [clickCancel,
   () => fakeBrowserProxy.page.requestClose(),
  ].forEach((canceller, i) => test(`cancelBeforeStart-{i}`, async () => {
              await canceller();
              await flushTasks();
              expectEquals(
                  fakeBrowserProxy.handler.getCallCount('cancelBeforeStart'),
                  1);
              expectEquals(
                  fakeBrowserProxy.handler.getCallCount('onPageClosed'), 1);
              expectEquals(fakeBrowserProxy.handler.getCallCount('cancel'), 0);
            }));

  // This is a special case that requestClose is different from clicking cancel
  // --- instead of going back to the previous page, requestClose should close
  // the page immediately.
  test('requestCloseAtConfigPage', async () => {
    await clickNext();  // Progress to config page.
    await fakeBrowserProxy.page.requestClose();
    await flushTasks();
    expectEquals(fakeBrowserProxy.handler.getCallCount('cancelBeforeStart'), 1);
    expectEquals(fakeBrowserProxy.handler.getCallCount('onPageClosed'), 1);
    expectEquals(fakeBrowserProxy.handler.getCallCount('cancel'), 0);
  });


  [clickCancel,
   () => fakeBrowserProxy.page.requestClose(),
  ].forEach((canceller, i) => test(`cancelAfterStart-{i}`, async () => {
              fakeBrowserProxy.page.onAmountOfFreeDiskSpace(
                  diskTicks, 0, false);
              await clickNext();
              await clickInstall();
              await canceller();
              await flushTasks();
              expectEquals(fakeBrowserProxy.handler.getCallCount('cancel'), 1);
              expectEquals(
                  fakeBrowserProxy.handler.getCallCount('onPageClosed'), 0,
                  'should not close until onCanceled is called');
              expectTrue(getInstallButton().hidden);
              expectTrue(getCancelButton().disabled);

              fakeBrowserProxy.page.onCanceled();
              await flushTasks();
              expectEquals(
                  fakeBrowserProxy.handler.getCallCount('onPageClosed'), 1);
            }));
});
