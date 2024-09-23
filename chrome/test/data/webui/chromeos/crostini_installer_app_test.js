// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://crostini-installer/app.js';

import {BrowserProxy} from 'chrome://crostini-installer/browser_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

const InstallerState = crostini.mojom.InstallerState;
const InstallerError = crostini.mojom.InstallerError;

class FakePageHandler extends TestBrowserProxy {
  constructor() {
    super([
      'install',
      'cancel',
      'cancelBeforeStart',
      'onPageClosed',
      'requestAmountOfFreeDiskSpace',
    ]);

    this.requestAmountOfFreeDiskSpaceResult_ = new Promise((resolve) => {
      this.resolveRequestAmountOfFreeDiskSpace_ = resolve;
    });
  }

  /** @override */
  install(diskSize, username) {
    this.methodCalled('install', [Number(diskSize), username]);
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
    return this.requestAmountOfFreeDiskSpaceResult_;
  }

  /**
   * Resolve the promise returned by `requestAmountOfFreeDiskSpace()`. Can only
   * be called once for the lifetime of the handler.
   */
  resolveRequestAmountOfFreeDiskSpace(
      ticks, defaultIndex, isLowSpaceAvailable) {
    this.resolveRequestAmountOfFreeDiskSpace_(
        {ticks, defaultIndex, isLowSpaceAvailable});
  }
}

class FakeBrowserProxy {
  constructor() {
    this.handler = new FakePageHandler();
    this.callbackRouter = new ash.crostiniInstaller.mojom.PageCallbackRouter();
    /** @type {appManagement.mojom.PageRemote} */
    this.page = this.callbackRouter.$.bindNewPipeAndPassRemote();
  }
}

suite('<crostini-installer-app>', () => {
  let fakeBrowserProxy;
  let app;

  setup(async () => {
    fakeBrowserProxy = new FakeBrowserProxy();
    BrowserProxy.setInstance(fakeBrowserProxy);

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
    {value: 2000, ariaValue: '2', label: '2'},
  ];

  test('installFlow', async () => {
    assertFalse(app.$$('#prompt-message').hidden);
    assertEquals(fakeBrowserProxy.handler.getCallCount('install'), 0);

    // It should wait for disk info to be available.
    await clickNext();
    await flushTasks();
    assertFalse(app.$$('#prompt-message').hidden);

    fakeBrowserProxy.handler.resolveRequestAmountOfFreeDiskSpace(
        diskTicks, 0, false);
    await flushTasks();
    assertFalse(app.$$('#configure-message').hidden);
    await clickCancel();  // Back to the prompt page.
    assertFalse(app.$$('#prompt-message').hidden);

    await clickNext();
    await flushTasks();
    assertFalse(app.$$('#configure-message').hidden);
    await clickInstall();
    const [diskSize, username] =
        await fakeBrowserProxy.handler.whenCalled('install');
    assertEquals(username, loadTimeData.getString('defaultContainerUsername'));
    assertFalse(app.$$('#installing-message').hidden);
    assertEquals(fakeBrowserProxy.handler.getCallCount('install'), 1);
    assertTrue(getInstallButton().hidden);

    fakeBrowserProxy.page.onProgressUpdate(
        InstallerState.kCreateDiskImage, 0.5);
    await flushTasks();
    assertTrue(
        !!app.$$('#installing-message > div').textContent.trim(),
        'progress message should be set');
    assertEquals(
        app.$$('#installing-message > paper-progress').getAttribute('value'),
        '50');

    assertEquals(fakeBrowserProxy.handler.getCallCount('onPageClosed'), 0);
    fakeBrowserProxy.page.onInstallFinished(InstallerError.kNone);
    await flushTasks();
    assertEquals(fakeBrowserProxy.handler.getCallCount('onPageClosed'), 1);
  });

  // We only proceed to the config page if disk info is available. Let's make
  // sure if the user click the next button multiple time very soon it dose not
  // blow up.
  test('multipleClickNextBeforeDiskAvailable', async () => {
    assertFalse(app.$$('#prompt-message').hidden);

    // It should wait for disk info to be available.
    await clickNext();
    await clickNext();
    await clickNext();
    await flushTasks();
    assertFalse(app.$$('#prompt-message').hidden);

    fakeBrowserProxy.handler.resolveRequestAmountOfFreeDiskSpace(
        diskTicks, 0, false);
    await flushTasks();
    // Enter configure page as usual
    assertFalse(app.$$('#configure-message').hidden);

    // Can back to prompt page as usual.
    await clickCancel();
    assertFalse(app.$$('#prompt-message').hidden);

    await clickNext();
    await flushTasks();
    // Re-enter configure page as usual
    assertFalse(app.$$('#configure-message').hidden);
  });

  test('straightToErrorPageIfMinDiskUnmet', async () => {
    assertFalse(app.$$('#prompt-message').hidden);

    fakeBrowserProxy.handler.resolveRequestAmountOfFreeDiskSpace([], 0, false);

    await clickNext();
    await flushTasks();
    assertFalse(app.$$('#error-message').hidden);
    assertTrue(
        !!app.$$('#error-message > div').textContent.trim(),
        'error message should be set');
    // We do not show retry button in this case.
    assertTrue(getInstallButton().hidden);
  });

  test('showWarningIfLowFreeSpace', async () => {
    assertFalse(app.$$('#prompt-message').hidden);

    fakeBrowserProxy.handler.resolveRequestAmountOfFreeDiskSpace(
        diskTicks, 0, true);

    await clickNext();
    await flushTasks();
    assertFalse(app.$$('#configure-message').hidden);
    assertFalse(isHidden(app.$$('#low-free-space-warning')));
  });

  diskTicks.forEach(async (_, defaultIndex) => {
    test(`configDiskSpaceWithDefault-${defaultIndex}`, async () => {
      assertFalse(app.$$('#prompt-message').hidden);

      fakeBrowserProxy.handler.resolveRequestAmountOfFreeDiskSpace(
          diskTicks, defaultIndex, false);

      await clickNext();
      await flushTasks();

      assertFalse(app.$$('#configure-message').hidden);
      assertTrue(isHidden(app.$$('#low-free-space-warning')));
      assertTrue(isHidden(app.$$('#diskSlider')));

      await clickInstall();
      const [diskSize, username] =
          await fakeBrowserProxy.handler.whenCalled('install');
      assertEquals(Number(diskSize), diskTicks[defaultIndex].value);
      assertEquals(fakeBrowserProxy.handler.getCallCount('install'), 1);
    });
  });

  test('configDiskSpaceWithUserSelection', async () => {
    assertFalse(app.$$('#prompt-message').hidden);

    fakeBrowserProxy.handler.resolveRequestAmountOfFreeDiskSpace(
        diskTicks, 0, false);

    await clickNext();
    await flushTasks();
    await clickCustomSize();
    await flushTasks();

    assertFalse(app.$$('#configure-message').hidden);
    assertTrue(isHidden(app.$$('#low-free-space-warning')));
    assertFalse(isHidden(app.$$('#diskSlider')));

    app.$$('#diskSlider').value = 1;

    await clickInstall();
    const [diskSize, username] =
        await fakeBrowserProxy.handler.whenCalled('install');
    assertEquals(Number(diskSize), diskTicks[1].value);
    assertEquals(fakeBrowserProxy.handler.getCallCount('install'), 1);
  });

  test('configUsername', async () => {
    fakeBrowserProxy.handler.resolveRequestAmountOfFreeDiskSpace(
        diskTicks, 0, false);
    await clickNext();

    assertEquals(
        app.$.username.value,
        loadTimeData.getString('defaultContainerUsername'));

    // Test invalid usernames
    const invalidUsernames = [
      '0abcd',            // Invalid (number) starting character.
      'aBcd',             // Invalid (uppercase) character.
      'spa ce',           // Invalid (space) character.
      '-dash',            // Invalid (dash) starting character.
      'name\\backslash',  // Invalid (backslash) character.
      'name@mpersand',    // Invalid (ampersand) character.
      // Reserved users
      'root', 'daemon', 'bin', 'sys', 'sync', 'games', 'man', 'lp', 'mail',
      'news', 'uucp', 'proxy', 'www-data', 'backup', 'list', 'irc', 'gnats',
      'nobody', '_apt', 'systemd-timesync', 'systemd-network',
      'systemd-resolve', 'systemd-bus-proxy', 'messagebus', 'sshd', 'rtkit',
      'pulse', 'android-root', 'chronos-access', 'android-everybody',
      // End reserved users
    ];

    for (const username of invalidUsernames) {
      app.$.username.value = username;

      await flushTasks();
      assertTrue(app.$.username.invalid);
      assertTrue(!!app.$.username.errorMessage);
      assertTrue(app.$.install.disabled);
    }

    // Test the empty username. The username field should not show an error, but
    // we want the install button to be disabled.
    app.$.username.value = '';
    await flushTasks();
    assertFalse(app.$.username.invalid);
    assertFalse(!!app.$.username.errorMessage);
    assertTrue(app.$.install.disabled);

    // Test a valid username
    const validUsername = 'totally-valid_username';
    app.$.username.value = validUsername;
    await flushTasks();
    assertFalse(app.$.username.invalid);
    clickInstall();
    const [diskSize, username] =
        await fakeBrowserProxy.handler.whenCalled('install');
    assertEquals(username, validUsername);
    assertEquals(fakeBrowserProxy.handler.getCallCount('install'), 1);
  });

  test('errorCancel', async () => {
    fakeBrowserProxy.handler.resolveRequestAmountOfFreeDiskSpace(
        diskTicks, 0, false);
    await clickNext();
    await clickInstall();
    fakeBrowserProxy.page.onInstallFinished(InstallerError.kErrorOffline);
    await flushTasks();
    assertFalse(app.$$('#error-message').hidden);
    assertTrue(
        !!app.$$('#error-message > div').textContent.trim(),
        'error message should be set');

    await clickCancel();
    assertEquals(fakeBrowserProxy.handler.getCallCount('onPageClosed'), 1);
    assertEquals(fakeBrowserProxy.handler.getCallCount('cancelBeforeStart'), 0);
    assertEquals(fakeBrowserProxy.handler.getCallCount('cancel'), 0);
  });

  test('errorRetry', async () => {
    fakeBrowserProxy.handler.resolveRequestAmountOfFreeDiskSpace(
        diskTicks, 0, false);
    await clickNext();
    await clickInstall();
    fakeBrowserProxy.page.onInstallFinished(InstallerError.kErrorOffline);
    await flushTasks();
    assertFalse(app.$$('#error-message').hidden);
    assertTrue(
        !!app.$$('#error-message > div').textContent.trim(),
        'error message should be set');

    await clickInstall();
    assertEquals(fakeBrowserProxy.handler.getCallCount('install'), 2);
  });

  test('errorNeedUpdate', async () => {
    fakeBrowserProxy.handler.resolveRequestAmountOfFreeDiskSpace(
        diskTicks, 0, false);
    await clickNext();
    await clickInstall();
    fakeBrowserProxy.page.onInstallFinished(InstallerError.kNeedUpdate);
    await flushTasks();

    assertEquals(app.$$('#title').innerText, 'ChromeOS update required');
    assertFalse(app.$$('#error-message').hidden);
    assertEquals(
        app.$$('#error-message').innerText,
        'To finish setting up Linux, update ChromeOS and try again.');
    assertFalse(app.$$('#settings').hidden);
    assertEquals(app.$$('#settings').innerText, 'Open Settings');
  });

  [clickCancel,
   () => fakeBrowserProxy.page.requestClose(),
  ].forEach((canceller, i) => test(`cancelBeforeStart-{i}`, async () => {
              await canceller();
              await flushTasks();
              assertEquals(
                  fakeBrowserProxy.handler.getCallCount('cancelBeforeStart'),
                  1);
              assertEquals(
                  fakeBrowserProxy.handler.getCallCount('onPageClosed'), 1);
              assertEquals(fakeBrowserProxy.handler.getCallCount('cancel'), 0);
            }));

  // This is a special case that requestClose is different from clicking cancel
  // --- instead of going back to the previous page, requestClose should close
  // the page immediately.
  test('requestCloseAtConfigPage', async () => {
    await clickNext();  // Progress to config page.
    await fakeBrowserProxy.page.requestClose();
    await flushTasks();
    assertEquals(fakeBrowserProxy.handler.getCallCount('cancelBeforeStart'), 1);
    assertEquals(fakeBrowserProxy.handler.getCallCount('onPageClosed'), 1);
    assertEquals(fakeBrowserProxy.handler.getCallCount('cancel'), 0);
  });


  [clickCancel,
   () => fakeBrowserProxy.page.requestClose(),
  ].forEach((canceller, i) => test(`cancelAfterStart-{i}`, async () => {
              fakeBrowserProxy.handler.resolveRequestAmountOfFreeDiskSpace(
                  diskTicks, 0, false);
              await clickNext();
              await clickInstall();
              await canceller();
              await flushTasks();
              assertEquals(fakeBrowserProxy.handler.getCallCount('cancel'), 1);
              assertEquals(
                  fakeBrowserProxy.handler.getCallCount('onPageClosed'), 0,
                  'should not close until onCanceled is called');
              assertTrue(getInstallButton().hidden);
              assertTrue(getCancelButton().disabled);

              fakeBrowserProxy.page.onCanceled();
              await flushTasks();
              assertEquals(
                  fakeBrowserProxy.handler.getCallCount('onPageClosed'), 1);
            }));
});
