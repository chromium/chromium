// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';
import 'chrome://os-settings/lazy_load.js';

import {AboutPageBrowserProxyImpl, BrowserChannel, IronIconElement, LifetimeBrowserProxyImpl, OsAboutPageElement, Router, routes, settingMojom, setUserActionRecorderForTesting, UpdateStatus, userActionRecorderMojom} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {FakeUserActionRecorder} from '../fake_user_action_recorder.js';
import {TestLifetimeBrowserProxy} from '../test_os_lifetime_browser_proxy.js';
import {clearBody} from '../utils.js';

import {TestAboutPageBrowserProxy} from './test_about_page_browser_proxy.js';

type UserActionRecorderInterface =
    userActionRecorderMojom.UserActionRecorderInterface;

suite('<os-about-page> AllBuilds', () => {
  const isRevampWayfindingEnabled =
      loadTimeData.getBoolean('isRevampWayfindingEnabled');
  let page: OsAboutPageElement;
  let aboutBrowserProxy: TestAboutPageBrowserProxy;
  let lifetimeBrowserProxy: TestLifetimeBrowserProxy;
  let userActionRecorder: UserActionRecorderInterface;

  const SPINNER_ICON_LIGHT_MODE =
      'chrome://resources/images/throbber_small.svg';
  const SPINNER_ICON_DARK_MODE =
      'chrome://resources/images/throbber_small_dark.svg';

  setup(async () => {
    loadTimeData.overrideValues({
      isManaged: false,
    });

    userActionRecorder = new FakeUserActionRecorder();
    setUserActionRecorderForTesting(userActionRecorder);

    lifetimeBrowserProxy = new TestLifetimeBrowserProxy();
    LifetimeBrowserProxyImpl.setInstance(lifetimeBrowserProxy);

    aboutBrowserProxy = new TestAboutPageBrowserProxy();
    AboutPageBrowserProxyImpl.setInstanceForTesting(aboutBrowserProxy);
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
  });

  interface StatusChangeEventOptions {
    progress?: number;
    message?: string;
    rollback?: boolean;
    powerwash?: boolean;
    version?: string;
    size?: string;
  }

  function fireStatusChanged(
      status: UpdateStatus, options: StatusChangeEventOptions = {}): void {
    webUIListenerCallback('update-status-changed', {
      progress: options.progress === undefined ? 1 : options.progress,
      message: options.message,
      status,
      rollback: options.rollback,
      powerwash: options.powerwash,
      version: options.version,
      size: options.size,
    });
  }

  async function initPage(): Promise<void> {
    clearBody();
    page = document.createElement('os-about-page');
    document.body.appendChild(page);

    Router.getInstance().navigateTo(routes.ABOUT);
    await flushTasks();

    await Promise.all([
      aboutBrowserProxy.whenCalled('getChannelInfo'),
      aboutBrowserProxy.whenCalled('refreshUpdateStatus'),
      aboutBrowserProxy.whenCalled('refreshTpmFirmwareUpdateStatus'),
      aboutBrowserProxy.whenCalled('checkInternetConnection'),
    ]);
  }

  function deepLinkToSetting(setting: settingMojom.Setting): void {
    const params = new URLSearchParams();
    params.append('settingId', setting.toString());
    Router.getInstance().navigateTo(routes.ABOUT, params);
    flush();
  }

  if (isRevampWayfindingEnabled) {
    test('Crostini settings card is visible', async () => {
      await initPage();
      const crostiniSettingsCard =
          page.shadowRoot!.querySelector('crostini-settings-card');
      assertTrue(isVisible(crostiniSettingsCard));
    });
  }

  ['light', 'dark'].forEach((mode) => {
    suite(`with ${mode} mode active`, () => {
      const isDarkMode = mode === 'dark';

      function setDarkMode(isActive: boolean): void {
        page.set('isDarkModeActive_', isActive);
      }

      /**
       * Test that the OS update status message and icon update according to
       * incoming 'update-status-changed' events, for light and dark mode
       * respectively.
       */
      test('status message and icon update', async () => {
        await initPage();
        setDarkMode(isDarkMode);
        const expectedIcon =
            isDarkMode ? SPINNER_ICON_DARK_MODE : SPINNER_ICON_LIGHT_MODE;

        const updateRowIcon =
            page.shadowRoot!.querySelector<IronIconElement>('#updateRowIcon');
        assertTrue(!!updateRowIcon);
        const statusMessageEl = page.$.updateStatusMessageInner;
        let previousMessageText = statusMessageEl.innerText;

        fireStatusChanged(UpdateStatus.CHECKING);
        assertEquals(expectedIcon, updateRowIcon.src);
        assertNull(updateRowIcon.getAttribute('icon'));
        assertNotEquals(previousMessageText, statusMessageEl.innerText);
        previousMessageText = statusMessageEl.innerText;

        fireStatusChanged(UpdateStatus.UPDATING, {progress: 0});
        assertEquals(expectedIcon, updateRowIcon.src);
        assertNull(updateRowIcon.getAttribute('icon'));
        assertFalse(statusMessageEl.innerText.includes('%'));
        assertNotEquals(previousMessageText, statusMessageEl.innerText);
        previousMessageText = statusMessageEl.innerText;

        fireStatusChanged(UpdateStatus.UPDATING, {progress: 1});
        assertNotEquals(previousMessageText, statusMessageEl.innerText);
        assertTrue(statusMessageEl.innerText.includes('%'));
        previousMessageText = statusMessageEl.innerText;

        fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
        assertNull(updateRowIcon.src);
        assertEquals(
            isRevampWayfindingEnabled ? 'os-settings:about-update-complete' :
                                        'settings:check-circle',
            updateRowIcon.icon);
        assertNotEquals(previousMessageText, statusMessageEl.innerText);
        previousMessageText = statusMessageEl.innerText;

        fireStatusChanged(UpdateStatus.DISABLED_BY_ADMIN);
        assertNull(updateRowIcon.src);
        assertEquals('cr20:domain', updateRowIcon.icon);
        assertNotEquals(previousMessageText, statusMessageEl.innerText);

        fireStatusChanged(UpdateStatus.FAILED);
        assertNull(updateRowIcon.src);
        assertEquals(
            isRevampWayfindingEnabled ? 'os-settings:about-update-error' :
                                        'cr:error-outline',
            updateRowIcon.icon);
        assertEquals(0, statusMessageEl.innerText.trim().length);

        fireStatusChanged(UpdateStatus.DISABLED);
        assertNull(updateRowIcon.src);
        assertNull(updateRowIcon.getAttribute('icon'));
        assertEquals(0, statusMessageEl.innerText.trim().length);
      });
    });
  });

  test('Error HTML is reflected in the update status message', async () => {
    await initPage();
    const htmlError = 'hello<br>there<br>was<pre>an</pre>error';
    fireStatusChanged(UpdateStatus.FAILED, {message: htmlError});
    assertEquals(htmlError, page.$.updateStatusMessageInner.innerHTML);
  });

  test('Learn more link is shown when update fails', async () => {
    await initPage();

    // Check that link is shown when update failed.
    fireStatusChanged(UpdateStatus.FAILED, {message: 'foo'});
    assertTrue(!!page.shadowRoot!.querySelector(
        '#updateStatusMessage a:not([hidden])'));

    // Check that link is hidden when update hasn't failed.
    fireStatusChanged(UpdateStatus.UPDATED, {message: ''});
    assertTrue(
        !!page.shadowRoot!.querySelector('#updateStatusMessage a[hidden]'));
  });

  test('Relaunch', async () => {
    await initPage();

    const relaunchButton = page.$.relaunchButton;
    assertTrue(!!relaunchButton);
    assertFalse(isVisible(relaunchButton));

    fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
    assertTrue(isVisible(relaunchButton));
    relaunchButton.click();
    await lifetimeBrowserProxy.whenCalled('relaunch');
  });

  test('Rollback', async () => {
    const deviceManager = 'google.com';
    loadTimeData.overrideValues({
      deviceManager,
      isManaged: true,
    });
    await initPage();
    const statusMessageEl = page.$.updateStatusMessageInner;

    const progress = 90;
    fireStatusChanged(
        UpdateStatus.UPDATING, {progress, powerwash: true, rollback: true});
    let expectedMessage = page.i18nAdvanced('aboutRollbackInProgress', {
                                substitutions: [deviceManager, `${progress}%`],
                              })
                              .toString();
    assertEquals(expectedMessage, statusMessageEl.innerText);

    fireStatusChanged(
        UpdateStatus.NEARLY_UPDATED, {powerwash: true, rollback: true});
    expectedMessage =
        page.i18nAdvanced(
                'aboutRollbackSuccess', {substitutions: [deviceManager]})
            .toString();
    assertEquals(expectedMessage, statusMessageEl.innerText);

    // Simulate update disallowed to previously installed version after a
    // consumer rollback.
    fireStatusChanged(UpdateStatus.UPDATE_TO_ROLLBACK_VERSION_DISALLOWED);
    expectedMessage = page.i18n('aboutUpdateToRollbackVersionDisallowed');
    assertEquals(expectedMessage, statusMessageEl.innerText);
  });

  test(
      'Warning dialog is shown when attempting to update over metered network',
      async () => {
        await initPage();

        fireStatusChanged(
            UpdateStatus.NEED_PERMISSION_TO_UPDATE,
            {version: '9001.0.0', size: '9999'});
        flush();

        const warningDialog =
            page.shadowRoot!.querySelector('settings-update-warning-dialog');
        assertTrue(!!warningDialog);
        assertTrue(
            warningDialog.$.dialog.open, 'Warning dialog should be open');
      });

  test('Message is shown when there is no internet', async () => {
    await initPage();

    const statusMessageEl = page.$.updateStatusMessageInner;
    assertFalse(isVisible(statusMessageEl));

    aboutBrowserProxy.sendStatusNoInternet();
    flush();
    assertTrue(isVisible(statusMessageEl));
    assertTrue(statusMessageEl.innerText.includes('no internet'));
  });

  suite('Update/Relaunch button', () => {
    /**
     * Regression test for crbug.com/1220294
     */
    test('Update button is shown initially', async () => {
      aboutBrowserProxy.blockRefreshUpdateStatus();
      await initPage();

      assertTrue(isVisible(page.$.checkForUpdatesButton));
    });

    test(
        'Clicking the update button moves focus to status message',
        async () => {
          await initPage();

          const {checkForUpdatesButton, updateStatusMessageInner} = page.$;
          checkForUpdatesButton.click();
          await aboutBrowserProxy.whenCalled('requestUpdate');
          assertEquals(
              updateStatusMessageInner, page.shadowRoot!.activeElement,
              'Update status message should be focused.');
        });

    /**
     * Test that all buttons update according to incoming
     * 'update-status-changed' events for the case where target and current
     * channel are the same.
     */
    test('Button visibility based on update status', async () => {
      await initPage();
      const {checkForUpdatesButton, relaunchButton} = page.$;

      function assertAllHidden() {
        assertTrue(checkForUpdatesButton.hidden);
        assertTrue(relaunchButton.hidden);
        // Ensure that when all buttons are hidden, the container is also
        // hidden.
        assertTrue(page.$.buttonContainer.hidden);
      }

      // Check that |UPDATED| status is ignored if the user has not
      // explicitly checked for updates yet.
      fireStatusChanged(UpdateStatus.UPDATED);
      assertFalse(checkForUpdatesButton.hidden);
      assertTrue(relaunchButton.hidden);

      // Check that the "Check for updates" button gets hidden for certain
      // UpdateStatus values, even if the CHECKING state was never
      // encountered (for example triggering update from crosh command
      // line).
      fireStatusChanged(UpdateStatus.UPDATING);
      assertAllHidden();
      fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
      assertTrue(checkForUpdatesButton.hidden);
      assertFalse(relaunchButton.hidden);

      fireStatusChanged(UpdateStatus.CHECKING);
      assertAllHidden();

      fireStatusChanged(UpdateStatus.UPDATING);
      assertAllHidden();

      fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
      assertTrue(checkForUpdatesButton.hidden);
      assertFalse(relaunchButton.hidden);

      fireStatusChanged(UpdateStatus.UPDATED);
      assertAllHidden();

      fireStatusChanged(UpdateStatus.FAILED);
      assertFalse(checkForUpdatesButton.hidden);
      assertTrue(relaunchButton.hidden);

      fireStatusChanged(UpdateStatus.DISABLED);
      assertAllHidden();

      fireStatusChanged(UpdateStatus.DISABLED_BY_ADMIN);
      assertFalse(checkForUpdatesButton.hidden);
      assertTrue(relaunchButton.hidden);
    });

    /**
     * Test that buttons update according to incoming
     * 'update-status-changed' events for the case where the target channel
     * is more stable than current channel and update will powerwash.
     */
    test('Relaunch button when updating from beta to stable', async () => {
      aboutBrowserProxy.setChannels(BrowserChannel.BETA, BrowserChannel.STABLE);
      await initPage();

      fireStatusChanged(UpdateStatus.NEARLY_UPDATED, {powerwash: true});
      const relaunchButton = page.$.relaunchButton;
      assertTrue(isVisible(relaunchButton));

      assertEquals(
          page.i18n('aboutRelaunchAndPowerwash'), relaunchButton.innerText);

      relaunchButton.click();
      await lifetimeBrowserProxy.whenCalled('relaunch');
    });

    /**
     * Test that buttons update according to incoming
     * 'update-status-changed' events for the case where the target channel
     * is less stable than current channel.
     */
    test('Relaunch button when updating from stable to beta', async () => {
      aboutBrowserProxy.setChannels(BrowserChannel.STABLE, BrowserChannel.BETA);
      await initPage();

      fireStatusChanged(UpdateStatus.NEARLY_UPDATED, {powerwash: false});
      const relaunchButton = page.$.relaunchButton;
      assertTrue(isVisible(relaunchButton));

      assertEquals(page.i18n('aboutRelaunch'), relaunchButton.innerText);

      relaunchButton.click();
      await lifetimeBrowserProxy.whenCalled('relaunch');
    });

    /**
     * The relaunch and powerwash button is shown if the powerwash flag is set
     * in the update status.
     */
    test('Relaunch button when powerwash flag is set', async () => {
      await initPage();

      fireStatusChanged(UpdateStatus.NEARLY_UPDATED, {powerwash: true});
      const relaunchButton = page.$.relaunchButton;
      assertTrue(isVisible(relaunchButton));

      assertEquals(
          page.i18n('aboutRelaunchAndPowerwash'), relaunchButton.innerText);

      relaunchButton.click();
      await lifetimeBrowserProxy.whenCalled('relaunch');
    });
  });

  suite('Release notes', () => {
    function queryReleaseNotesOnline(): HTMLElement|null {
      return page.shadowRoot!.querySelector<HTMLElement>('#releaseNotesOnline');
    }

    function queryReleaseNotesOffline(): HTMLElement|null {
      return page.shadowRoot!.querySelector<HTMLElement>(
          '#releaseNotesOffline');
    }

    suite('when online', () => {
      setup(async () => {
        aboutBrowserProxy.setInternetConnection(true);
        await initPage();
      });

      test('Online release notes are visible', async () => {
        const releaseNotesOnlineButton = queryReleaseNotesOnline();
        assertTrue(!!releaseNotesOnlineButton);
        assertTrue(isVisible(releaseNotesOnlineButton));
        assertFalse(isVisible(queryReleaseNotesOffline()));

        releaseNotesOnlineButton.click();
        await aboutBrowserProxy.whenCalled('launchReleaseNotes');
      });

      test('Deep link to release notes button', async () => {
        const setting = settingMojom.Setting.kSeeWhatsNew;
        deepLinkToSetting(setting);

        const deepLinkElement = queryReleaseNotesOnline();
        assertTrue(!!deepLinkElement);
        await waitAfterNextRender(deepLinkElement);
        assertEquals(
            deepLinkElement, page.shadowRoot!.activeElement,
            `Release notes should be focused for settingId=${setting}.`);
      });
    });

    suite('when offline', () => {
      setup(async () => {
        aboutBrowserProxy.setInternetConnection(false);
        await initPage();
      });

      test('Offline release notes are visible', () => {
        assertFalse(isVisible(queryReleaseNotesOnline()));
        assertTrue(isVisible(queryReleaseNotesOffline()));
      });

      test('Deep link to release notes button', async () => {
        const setting = settingMojom.Setting.kSeeWhatsNew;
        deepLinkToSetting(setting);

        const deepLinkElement = queryReleaseNotesOffline();
        assertTrue(!!deepLinkElement);
        await waitAfterNextRender(deepLinkElement);
        assertEquals(
            deepLinkElement, page.shadowRoot!.activeElement,
            `Release notes should be focused for settingId=${setting}.`);
      });
    });
  });

  suite('Regulatory info', () => {
    const regulatoryInfo = {text: 'foo', url: 'bar'};

    async function checkRegulatoryInfo(isShowing: boolean): Promise<void> {
      await aboutBrowserProxy.whenCalled('getRegulatoryInfo');
      const regulatoryInfoEl = page.$.regulatoryInfo;
      assertEquals(isShowing, isVisible(regulatoryInfoEl));

      if (isShowing) {
        const img = regulatoryInfoEl.querySelector('img');
        assertTrue(!!img);
        assertEquals(regulatoryInfo.text, img.getAttribute('alt'));
        assertEquals(regulatoryInfo.url, img.getAttribute('src'));
      }
    }

    test('Regulatory info is not shown', async () => {
      aboutBrowserProxy.setRegulatoryInfo(null);
      await initPage();
      await checkRegulatoryInfo(false);
    });

    test('Regulatory info is shown', async () => {
      aboutBrowserProxy.setRegulatoryInfo(regulatoryInfo);
      await initPage();
      await checkRegulatoryInfo(true);
    });
  });

  test('TPM firmware update', async () => {
    await initPage();

    const tpmFirmwareUpdateRow =
        page.shadowRoot!.querySelector<HTMLElement>('#aboutTPMFirmwareUpdate');
    assertFalse(isVisible(tpmFirmwareUpdateRow));

    aboutBrowserProxy.setTpmFirmwareUpdateStatus({updateAvailable: true});
    aboutBrowserProxy.refreshTpmFirmwareUpdateStatus();
    assertTrue(!!tpmFirmwareUpdateRow);
    assertTrue(isVisible(tpmFirmwareUpdateRow));

    tpmFirmwareUpdateRow.click();
    await flushTasks();
    const dialog =
        page.shadowRoot!.querySelector('os-settings-powerwash-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.$.dialog.open);

    dialog.shadowRoot!.querySelector<HTMLElement>('#powerwash')!.click();
    const requestTpmFirmwareUpdate =
        await lifetimeBrowserProxy.whenCalled('factoryReset');
    assertTrue(requestTpmFirmwareUpdate);
  });

  suite('End of life', () => {
    /**
     * Checks the visibility of the end of life message and icon.
     */
    async function assertHasEndOfLife(isShowing: boolean): Promise<void> {
      await aboutBrowserProxy.whenCalled('getEndOfLifeInfo');

      const statusMessageEl = page.$.updateStatusMessageInner;
      const endOfLifeMessage =
          page.shadowRoot!.querySelector<HTMLElement>('#endOfLifeMessage');
      assertTrue(!!endOfLifeMessage);
      assertEquals(isShowing, isVisible(endOfLifeMessage));

      // Update status message should be hidden before user has
      // checked for updates.
      assertFalse(isVisible(statusMessageEl));

      fireStatusChanged(UpdateStatus.CHECKING);
      assertEquals(isShowing, !isVisible(statusMessageEl));

      if (isShowing) {
        const updateRowIcon =
            page.shadowRoot!.querySelector<IronIconElement>('#updateRowIcon');
        assertTrue(!!updateRowIcon);
        assertNull(updateRowIcon.src);
        assertEquals('os-settings:end-of-life', updateRowIcon.icon);

        const {checkForUpdatesButton} = page.$;
        assertTrue(!!checkForUpdatesButton);
        assertTrue(checkForUpdatesButton.hidden);
      }
    }

    test('End of life message is shown', async () => {
      aboutBrowserProxy.setEndOfLifeInfo({
        hasEndOfLife: true,
        aboutPageEndOfLifeMessage: '',
        shouldShowEndOfLifeIncentive: false,
        shouldShowOfferText: false,
        isExtendedUpdatesDatePassed: false,
        isExtendedUpdatesOptInRequired: false,
      });
      await initPage();
      await assertHasEndOfLife(true);
    });

    test('End of life message is not shown', async () => {
      aboutBrowserProxy.setEndOfLifeInfo({
        hasEndOfLife: false,
        aboutPageEndOfLifeMessage: '',
        shouldShowEndOfLifeIncentive: false,
        shouldShowOfferText: false,
        isExtendedUpdatesDatePassed: false,
        isExtendedUpdatesOptInRequired: false,
      });
      await initPage();
      await assertHasEndOfLife(false);
    });

    async function assertEndOfLifeIncentive(isShowing: boolean): Promise<void> {
      await aboutBrowserProxy.whenCalled('getEndOfLifeInfo');
      const eolOfferSection =
          page.shadowRoot!.querySelector('eol-offer-section');
      assertEquals(isShowing, isVisible(eolOfferSection));

      if (isShowing) {
        assertTrue(!!eolOfferSection);
        const eolIncentiveButton =
            eolOfferSection.shadowRoot!.querySelector<HTMLElement>(
                '#eolIncentiveButton');
        assertTrue(!!eolIncentiveButton);
        eolIncentiveButton.click();
        await aboutBrowserProxy.whenCalled('endOfLifeIncentiveButtonClicked');
      }
    }

    test('End of life incentive is not shown', async () => {
      aboutBrowserProxy.setEndOfLifeInfo({
        hasEndOfLife: false,
        aboutPageEndOfLifeMessage: '',
        shouldShowEndOfLifeIncentive: false,
        shouldShowOfferText: false,
        isExtendedUpdatesDatePassed: false,
        isExtendedUpdatesOptInRequired: false,
      });
      await initPage();
      await assertEndOfLifeIncentive(false);
    });

    test('End of life incentive is shown', async () => {
      aboutBrowserProxy.setEndOfLifeInfo({
        hasEndOfLife: false,
        aboutPageEndOfLifeMessage: '',
        shouldShowEndOfLifeIncentive: true,
        shouldShowOfferText: false,
        isExtendedUpdatesDatePassed: false,
        isExtendedUpdatesOptInRequired: false,
      });
      await initPage();
      await assertEndOfLifeIncentive(true);
    });
  });

  test(
      'Detailed build info row is focused when returning from subpage',
      async () => {
        const triggerSelector = '#detailedBuildInfoTrigger';
        const subpageTrigger =
            page.shadowRoot!.querySelector<HTMLElement>(triggerSelector);
        assertTrue(!!subpageTrigger);

        // Sub-page trigger navigates to Detailed build info subpage
        subpageTrigger.click();
        assertEquals(
            routes.ABOUT_DETAILED_BUILD_INFO,
            Router.getInstance().currentRoute);

        // Navigate back
        const popStateEventPromise = eventToPromise('popstate', window);
        Router.getInstance().navigateToPreviousRoute();
        await popStateEventPromise;
        await waitAfterNextRender(page);

        assertEquals(
            subpageTrigger, page.shadowRoot!.activeElement,
            `${triggerSelector} should be focused.`);
      });

  suite('Get help', () => {
    function getHelpRow(): HTMLElement {
      const row = page.shadowRoot!.querySelector<HTMLElement>('#help');
      assertTrue(!!row);
      return row;
    }

    test('Clicking row opens explore app', async () => {
      await initPage();

      getHelpRow().click();
      await aboutBrowserProxy.whenCalled('openOsHelpPage');
    });

    test('Deep link to get help row', async () => {
      await initPage();

      const setting = settingMojom.Setting.kGetHelpWithChromeOs;
      deepLinkToSetting(setting);

      const deepLinkElement = getHelpRow();
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, page.shadowRoot!.activeElement,
          `Get help row should be focused for settingId=${setting}.`);
    });
  });

  suite('Diagnostics', () => {
    function getDiagnosticsRow(): HTMLElement {
      const row = page.shadowRoot!.querySelector<HTMLElement>('#diagnostics');
      assertTrue(!!row);
      return row;
    }

    test('Clicking row opens diagnostics app', async () => {
      await initPage();

      getDiagnosticsRow().click();
      await aboutBrowserProxy.whenCalled('openDiagnostics');
    });

    test('Deep link to diagnostics', async () => {
      await initPage();

      const setting = settingMojom.Setting.kDiagnostics;
      deepLinkToSetting(setting);

      const deepLinkElement = getDiagnosticsRow();
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, page.shadowRoot!.activeElement,
          `Diagnostics row should be focused for settingId=${setting}.`);
    });
  });

  suite('Firmware updates', () => {
    function getFirmwareUpdateBadge(): IronIconElement {
      const badge = page.shadowRoot!.querySelector<IronIconElement>(
          '#firmwareUpdateBadge');
      assertTrue(!!badge);
      return badge;
    }

    function getFirmwareUpdateBadgeSeparator(): HTMLElement {
      const separator = page.shadowRoot!.querySelector<HTMLElement>(
          '#firmwareUpdateBadgeSeparator');
      assertTrue(!!separator);
      return separator;
    }

    test('Badge is not shown when there are no updates', async () => {
      aboutBrowserProxy.setFirmwareUpdatesCount(0);
      await initPage();
      await aboutBrowserProxy.whenCalled('getFirmwareUpdateCount');

      assertFalse(isVisible(getFirmwareUpdateBadge()));
      assertFalse(isVisible(getFirmwareUpdateBadgeSeparator()));
    });

    test('Badge shows the number of updates', async () => {
      for (let i = 1; i < 10; i++) {
        aboutBrowserProxy.setFirmwareUpdatesCount(i);
        await initPage();
        await aboutBrowserProxy.whenCalled('getFirmwareUpdateCount');

        const badge = getFirmwareUpdateBadge();
        assertTrue(isVisible(badge));
        assertTrue(isVisible(getFirmwareUpdateBadgeSeparator()));
        assertEquals(`os-settings:counter-${i}`, badge.icon);
      }
    });

    test('Badge uses the counter-9 icon for 10 updates', async () => {
      aboutBrowserProxy.setFirmwareUpdatesCount(10);
      await initPage();
      await aboutBrowserProxy.whenCalled('getFirmwareUpdateCount');

      const badge = getFirmwareUpdateBadge();
      assertTrue(isVisible(badge));
      assertTrue(isVisible(getFirmwareUpdateBadgeSeparator()));
      assertEquals('os-settings:counter-9', badge.icon);
    });

    test('Clicking link opens firmware updates page', async () => {
      await initPage();

      const firmwareUpdatesRow =
          page.shadowRoot!.querySelector<HTMLElement>('#firmwareUpdates');
      assertTrue(!!firmwareUpdatesRow);
      firmwareUpdatesRow.click();
      await aboutBrowserProxy.whenCalled('openFirmwareUpdatesPage');
    });

    test('Deep link to firmware updates', async () => {
      await initPage();

      const setting = settingMojom.Setting.kFirmwareUpdates;
      deepLinkToSetting(setting);

      const deepLinkElement =
          page.shadowRoot!.querySelector<HTMLElement>('#firmwareUpdates');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, page.shadowRoot!.activeElement,
          `Firmware updates should be focused for settingId=${setting}.`);
    });
  });

  suite('Extended Updates', () => {
    const EXTENDED_UPDATES_ICON = 'os-settings:about-update-complete';

    function assertExtendedUpdatesVisibility(visible: boolean) {
      const mainMessage = page.shadowRoot!.querySelector<HTMLElement>(
          '#extendedUpdatesMainMessage');
      const secondaryMessage = page.shadowRoot!.querySelector<HTMLElement>(
          '#extendedUpdatesSecondaryMessage');

      assertEquals(visible, isVisible(mainMessage));
      assertEquals(visible, isVisible(secondaryMessage));
      assertEquals(visible, isVisible(page.$.extendedUpdatesButton));
    }

    function getLastEligibilityArgs(): boolean[] {
      return aboutBrowserProxy.getArgs('isExtendedUpdatesOptInEligible').at(-1);
    }

    function fireExtendedUpdatesSettingChanged(): void {
      // This setting only changes when the user opts in,
      // which in turn makes the device no longer eligible for opt-in,
      // so we update the return value to false here.
      aboutBrowserProxy.setExtendedUpdatesOptInEligible(false);
      webUIListenerCallback('extended-updates-setting-changed');
    }

    test('is not shown by default', async () => {
      await initPage();
      assertExtendedUpdatesVisibility(false);
      assertTrue(isVisible(page.$.checkForUpdatesButton));
    });

    test('is shown when eligible and up to date', async () => {
      aboutBrowserProxy.setExtendedUpdatesOptInEligible(true);
      assertEquals(
          0, aboutBrowserProxy.getCallCount('recordExtendedUpdatesShown'));

      await initPage();
      // Extended updates is shown initially if no pending updates.
      assertExtendedUpdatesVisibility(true);
      await aboutBrowserProxy.whenCalled('recordExtendedUpdatesShown');
      assertEquals(
          1, aboutBrowserProxy.getCallCount('recordExtendedUpdatesShown'));

      fireStatusChanged(UpdateStatus.CHECKING);
      assertExtendedUpdatesVisibility(false);

      fireStatusChanged(UpdateStatus.UPDATING);
      assertExtendedUpdatesVisibility(false);

      fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
      assertExtendedUpdatesVisibility(false);

      fireStatusChanged(UpdateStatus.UPDATED);
      assertExtendedUpdatesVisibility(true);
      assertFalse(isVisible(page.$.checkForUpdatesButton));
      // Record call should only happen at most once.
      assertEquals(
          1, aboutBrowserProxy.getCallCount('recordExtendedUpdatesShown'));

      const updateRowIcon =
          page.shadowRoot!.querySelector<IronIconElement>('#updateRowIcon');
      assertTrue(!!updateRowIcon);
      assertNull(updateRowIcon.src);
      assertEquals(EXTENDED_UPDATES_ICON, updateRowIcon.icon);

      assertTrue(isVisible(page.$.extendedUpdatesButton));
      page.$.extendedUpdatesButton.click();
      await aboutBrowserProxy.whenCalled('openExtendedUpdatesDialog');
    });

    test('is not shown after opting in', async () => {
      aboutBrowserProxy.setExtendedUpdatesOptInEligible(true);
      await initPage();
      assertExtendedUpdatesVisibility(true);

      fireExtendedUpdatesSettingChanged();
      await aboutBrowserProxy.whenCalled('isExtendedUpdatesOptInEligible');
      assertExtendedUpdatesVisibility(false);
    });

    suite('when', () => {
      interface ExtendedUpdatesTestCase {
        eolPassed: boolean;
        extDatePassed: boolean;
        optInRequired: boolean;
        expectedVisibility: boolean;
      }
      [{
        eolPassed: true,
        extDatePassed: true,
        optInRequired: true,
        expectedVisibility: false,
      },
       {
         eolPassed: false,
         extDatePassed: true,
         optInRequired: true,
         expectedVisibility: true,
       },
       {
         eolPassed: false,
         extDatePassed: false,
         optInRequired: true,
         expectedVisibility: false,
       },
       {
         eolPassed: false,
         extDatePassed: true,
         optInRequired: false,
         expectedVisibility: false,
       },
      ].forEach((tc: ExtendedUpdatesTestCase) => {
        test(
            `eol has ${tc.eolPassed ? '' : 'not '}passed, ` +
                `extended date has ${tc.extDatePassed ? '' : 'not '}passed, ` +
                `and opt-in is ${tc.optInRequired ? '' : 'not '}required, ` +
                `is ${tc.expectedVisibility ? '' : 'not '}visible`,
            async () => {
              aboutBrowserProxy.setEndOfLifeInfo({
                hasEndOfLife: tc.eolPassed,
                aboutPageEndOfLifeMessage: '',
                shouldShowEndOfLifeIncentive: false,
                shouldShowOfferText: false,
                isExtendedUpdatesDatePassed: tc.extDatePassed,
                isExtendedUpdatesOptInRequired: tc.optInRequired,
              });
              aboutBrowserProxy.setExtendedUpdatesOptInEligible(
                  tc.expectedVisibility);
              await initPage();

              assertDeepEquals(
                  [tc.eolPassed, tc.extDatePassed, tc.optInRequired],
                  getLastEligibilityArgs());
              assertExtendedUpdatesVisibility(tc.expectedVisibility);
            });
      });
    });
  });
});

suite('<os-about-page> OfficialBuild', () => {
  let page: OsAboutPageElement;
  let browserProxy: TestAboutPageBrowserProxy;

  function deepLinkToSetting(setting: settingMojom.Setting): void {
    const params = new URLSearchParams();
    params.append('settingId', setting.toString());
    Router.getInstance().navigateTo(routes.ABOUT, params);
    flush();
  }

  async function assertElementIsDeepLinked(element: HTMLElement):
      Promise<void> {
    await waitAfterNextRender(element);
    assertEquals(element, page.shadowRoot!.activeElement);
  }

  setup(async () => {
    browserProxy = new TestAboutPageBrowserProxy();
    AboutPageBrowserProxyImpl.setInstanceForTesting(browserProxy);

    clearBody();
    page = document.createElement('os-about-page');
    document.body.appendChild(page);
    await flushTasks();
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
  });

  test('Report an issue click opens feedback dialog', async () => {
    const reportIssueRow =
        page.shadowRoot!.querySelector<HTMLElement>('#reportIssue');
    assertTrue(!!reportIssueRow);
    reportIssueRow.click();
    await browserProxy.whenCalled('openFeedbackDialog');
  });

  test('Deep link to report an issue', async () => {
    const setting = settingMojom.Setting.kReportAnIssue;
    deepLinkToSetting(setting);

    const reportIssueRow =
        page.shadowRoot!.querySelector<HTMLElement>('#reportIssue');
    assertTrue(!!reportIssueRow);
    await assertElementIsDeepLinked(reportIssueRow);
  });

  test('Deep link to terms of service', async () => {
    const setting = settingMojom.Setting.kTermsOfService;
    deepLinkToSetting(setting);

    const productTosRow =
        page.shadowRoot!.querySelector<HTMLElement>('#aboutProductTos');
    assertTrue(!!productTosRow);
    await assertElementIsDeepLinked(productTosRow);
  });
});
