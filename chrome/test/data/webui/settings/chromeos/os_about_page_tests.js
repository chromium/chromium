// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';
import 'chrome://os-settings/lazy_load.js';

import {CrPolicyIndicatorType} from '//resources/ash/common/cr_policy_indicator_behavior.js';
import {AboutPageBrowserProxyImpl, BrowserChannel, DeviceNameBrowserProxyImpl, DeviceNameState, LifetimeBrowserProxyImpl, Router, routes, SetDeviceNameResult, setUserActionRecorderForTesting, UpdateStatus, userActionRecorderMojom} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {FakeUserActionRecorder} from './fake_user_action_recorder.js';
import {TestAboutPageBrowserProxyChromeOS} from './test_about_page_browser_proxy_chromeos.js';
import {TestDeviceNameBrowserProxy} from './test_device_name_browser_proxy.js';
import {TestLifetimeBrowserProxy} from './test_os_lifetime_browser_proxy.js';

suite('AboutPageTest', function() {
  let page = null;

  /** @type {?TestAboutPageBrowserProxyChromeOS} */
  let aboutBrowserProxy = null;

  /** @type {?TestLifetimeBrowserProxy} */
  let lifetimeBrowserProxy = null;

  /** @type {?userActionRecorderMojom.UserActionRecorderInterface} */
  let userActionRecorder = null;

  const SPINNER_ICON_LIGHT_MODE =
      'chrome://resources/images/throbber_small.svg';
  const SPINNER_ICON_DARK_MODE =
      'chrome://resources/images/throbber_small_dark.svg';

  setup(function() {
    userActionRecorder = new FakeUserActionRecorder();
    setUserActionRecorderForTesting(userActionRecorder);

    lifetimeBrowserProxy = new TestLifetimeBrowserProxy();
    LifetimeBrowserProxyImpl.setInstance(lifetimeBrowserProxy);

    aboutBrowserProxy = new TestAboutPageBrowserProxyChromeOS();
    AboutPageBrowserProxyImpl.setInstanceForTesting(aboutBrowserProxy);
    return initNewPage();
  });

  teardown(function() {
    page.remove();
    page = null;
    Router.getInstance().resetRouteForTesting();
    setUserActionRecorderForTesting(null);
  });

  /**
   * @param {!UpdateStatus} status
   * @param {{
   *   progress: number|undefined,
   *   message: string|undefined,
   *   rollback: bool|undefined,
   *   powerwash: bool|undefined,
   *   version: string|undefined,
   *   size: string|undefined,
   * }} opt_options
   */
  function fireStatusChanged(status, opt_options) {
    const options = opt_options || {};
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

  /** @return {!Promise} */
  function initNewPage() {
    aboutBrowserProxy.reset();
    lifetimeBrowserProxy.reset();
    PolymerTest.clearBody();
    page = document.createElement('os-about-page');
    Router.getInstance().navigateTo(routes.ABOUT);
    document.body.appendChild(page);
    return Promise.all([
      aboutBrowserProxy.whenCalled('getChannelInfo'),
      aboutBrowserProxy.whenCalled('refreshUpdateStatus'),
      aboutBrowserProxy.whenCalled('refreshTpmFirmwareUpdateStatus'),
      aboutBrowserProxy.whenCalled('checkInternetConnection'),
    ]);
  }

  /**
   * @param {string} id
   */
  function navigateToSettingsPageWithId(id) {
    const params = new URLSearchParams();
    params.append('settingId', id);
    Router.getInstance().navigateTo(routes.ABOUT, params);

    flush();
  }

  /**
   * @param {string} id
   * @return {!HTMLButtonElement}
   */
  function getDeepLinkButtonElementById(id) {
    return page.shadowRoot.querySelector(`#${id}`).shadowRoot.querySelector(
        'cr-icon-button');
  }

  /**
   * @param {boolean} active
   */
  function setDarkMode(active) {
    assertTrue(!!page);
    page.isDarkModeActive_ = active;
  }

  suite('When OsSettingsRevampWayfinding feature is enabled', () => {
    setup(() => {
      loadTimeData.overrideValues({isRevampWayfindingEnabled: true});
    });

    test('Crostini settings card is visible', async () => {
      await initNewPage();
      const crostiniSettingsCard =
          page.shadowRoot.querySelector('crostini-settings-card');
      assertTrue(isVisible(crostiniSettingsCard));
    });
  });

  ['light', 'dark'].forEach((mode) => {
    suite(`with ${mode} mode active`, () => {
      const isDarkMode = mode === 'dark';

      /**
       * Test that the OS update status message and icon update according to
       * incoming 'update-status-changed' events, for light and dark mode
       * respectively.
       */
      test('status message and icon update', () => {
        setDarkMode(isDarkMode);
        const icon = page.shadowRoot.querySelector('iron-icon');
        assertTrue(!!icon);
        const statusMessageEl =
            page.shadowRoot.querySelector('#updateStatusMessage div');
        let previousMessageText = statusMessageEl.textContent;

        fireStatusChanged(UpdateStatus.CHECKING);
        if (isDarkMode) {
          assertEquals(SPINNER_ICON_DARK_MODE, icon.src);
        } else {
          assertEquals(SPINNER_ICON_LIGHT_MODE, icon.src);
        }
        assertEquals(null, icon.getAttribute('icon'));
        assertNotEquals(previousMessageText, statusMessageEl.textContent);
        previousMessageText = statusMessageEl.textContent;

        fireStatusChanged(UpdateStatus.UPDATING, {progress: 0});
        if (isDarkMode) {
          assertEquals(SPINNER_ICON_DARK_MODE, icon.src);
        } else {
          assertEquals(SPINNER_ICON_LIGHT_MODE, icon.src);
        }
        assertEquals(null, icon.getAttribute('icon'));
        assertFalse(statusMessageEl.textContent.includes('%'));
        assertNotEquals(previousMessageText, statusMessageEl.textContent);
        previousMessageText = statusMessageEl.textContent;

        fireStatusChanged(UpdateStatus.UPDATING, {progress: 1});
        assertNotEquals(previousMessageText, statusMessageEl.textContent);
        assertTrue(statusMessageEl.textContent.includes('%'));
        previousMessageText = statusMessageEl.textContent;

        fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
        assertEquals(null, icon.src);
        assertEquals('settings:check-circle', icon.icon);
        assertNotEquals(previousMessageText, statusMessageEl.textContent);
        previousMessageText = statusMessageEl.textContent;

        fireStatusChanged(UpdateStatus.DISABLED_BY_ADMIN);
        assertEquals(null, icon.src);
        assertEquals('cr20:domain', icon.icon);
        assertNotEquals(previousMessageText, statusMessageEl.textContent);

        fireStatusChanged(UpdateStatus.FAILED);
        assertEquals(null, icon.src);
        assertEquals('cr:error-outline', icon.icon);
        assertEquals(0, statusMessageEl.textContent.trim().length);

        fireStatusChanged(UpdateStatus.DISABLED);
        assertEquals(null, icon.src);
        assertEquals(null, icon.getAttribute('icon'));
        assertEquals(0, statusMessageEl.textContent.trim().length);
      });
    });
  });

  test('ErrorMessageWithHtml', function() {
    const htmlError = 'hello<br>there<br>was<pre>an</pre>error';
    fireStatusChanged(UpdateStatus.FAILED, {message: htmlError});
    const statusMessageEl =
        page.shadowRoot.querySelector('#updateStatusMessage div');
    assertEquals(htmlError, statusMessageEl.innerHTML);
  });

  test('FailedLearnMoreLink', function() {
    // Check that link is shown when update failed.
    fireStatusChanged(UpdateStatus.FAILED, {message: 'foo'});
    assertTrue(!!page.shadowRoot.querySelector(
        '#updateStatusMessage a:not([hidden])'));

    // Check that link is hidden when update hasn't failed.
    fireStatusChanged(UpdateStatus.UPDATED, {message: ''});
    assertTrue(
        !!page.shadowRoot.querySelector('#updateStatusMessage a[hidden]'));
  });

  test('Relaunch', function() {
    const {relaunch} = page.$;
    assertTrue(!!relaunch);
    assertTrue(relaunch.hidden);

    fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
    assertFalse(relaunch.hidden);
    relaunch.click();
    return lifetimeBrowserProxy.whenCalled('relaunch');
  });

  test('Rollback', async () => {
    loadTimeData.overrideValues({
      deviceManager: 'google.com',
      isManaged: true,
    });
    await initNewPage();
    const statusMessageEl =
        page.shadowRoot.querySelector('#updateStatusMessage div');

    const progress = 90;
    fireStatusChanged(
        UpdateStatus.UPDATING,
        {progress: progress, powerwash: true, rollback: true});

    assertEquals(
        page.i18nAdvanced(
                'aboutRollbackInProgress',
                {substitutions: [page.deviceManager_, progress + '%']})
            .toString(),
        statusMessageEl.textContent);

    fireStatusChanged(
        UpdateStatus.NEARLY_UPDATED, {powerwash: true, rollback: true});

    assertEquals(
        page.i18nAdvanced(
                'aboutRollbackSuccess', {substitutions: [page.deviceManager_]})
            .toString(),
        statusMessageEl.textContent);

    // Simulate update disallowed to previously installed version after a
    // consumer rollback.
    fireStatusChanged(UpdateStatus.UPDATE_TO_ROLLBACK_VERSION_DISALLOWED);
    const expectedMessage =
        page.i18n('aboutUpdateToRollbackVersionDisallowed').toString();
    assertEquals(expectedMessage, statusMessageEl.textContent);
  });

  test(
      'Warning dialog is shown when attempting to update over metered network',
      async () => {
        await initNewPage();

        fireStatusChanged(
            UpdateStatus.NEED_PERMISSION_TO_UPDATE,
            {version: '9001.0.0', size: '9999'});
        flush();

        const warningDialog =
            page.shadowRoot.querySelector('settings-update-warning-dialog');
        assertTrue(!!warningDialog);
        assertTrue(
            warningDialog.$.dialog.open, 'Warning dialog should be open');
      });

  test('NoInternet', function() {
    assertTrue(page.$.updateStatusMessage.hidden);
    aboutBrowserProxy.sendStatusNoInternet();
    flush();
    assertFalse(page.$.updateStatusMessage.hidden);
    assertTrue(page.$.updateStatusMessage.textContent.includes('no internet'));
  });

  /**
   * Test that all buttons update according to incoming
   * 'update-status-changed' events for the case where target and current
   * channel are the same.
   */
  test('ButtonsUpdate_SameChannel', function() {
    const {checkForUpdates, relaunch} = page.$;

    assertTrue(!!relaunch);
    assertTrue(!!checkForUpdates);

    function assertAllHidden() {
      assertTrue(checkForUpdates.hidden);
      assertTrue(relaunch.hidden);
      // Ensure that when all buttons are hidden, the container is also
      // hidden.
      assertTrue(page.$.buttonContainer.hidden);
    }

    // Check that |UPDATED| status is ignored if the user has not
    // explicitly checked for updates yet.
    fireStatusChanged(UpdateStatus.UPDATED);
    assertFalse(checkForUpdates.hidden);
    assertTrue(relaunch.hidden);

    // Check that the "Check for updates" button gets hidden for certain
    // UpdateStatus values, even if the CHECKING state was never
    // encountered (for example triggering update from crosh command
    // line).
    fireStatusChanged(UpdateStatus.UPDATING);
    assertAllHidden();
    fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
    assertTrue(checkForUpdates.hidden);
    assertFalse(relaunch.hidden);

    fireStatusChanged(UpdateStatus.CHECKING);
    assertAllHidden();

    fireStatusChanged(UpdateStatus.UPDATING);
    assertAllHidden();

    fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
    assertTrue(checkForUpdates.hidden);
    assertFalse(relaunch.hidden);

    fireStatusChanged(UpdateStatus.UPDATED);
    assertAllHidden();

    fireStatusChanged(UpdateStatus.FAILED);
    assertFalse(checkForUpdates.hidden);
    assertTrue(relaunch.hidden);

    fireStatusChanged(UpdateStatus.DISABLED);
    assertAllHidden();

    fireStatusChanged(UpdateStatus.DISABLED_BY_ADMIN);
    assertFalse(checkForUpdates.hidden);
    assertTrue(relaunch.hidden);
  });

  /**
   * Test that buttons update according to incoming
   * 'update-status-changed' events for the case where the target channel
   * is more stable than current channel and update will powerwash.
   */
  test('ButtonsUpdate_BetaToStable', async () => {
    aboutBrowserProxy.setChannels(BrowserChannel.BETA, BrowserChannel.STABLE);
    await initNewPage();

    fireStatusChanged(UpdateStatus.NEARLY_UPDATED, {powerwash: true});

    assertTrue(!!page.$.relaunch);
    assertFalse(page.$.relaunch.hidden);

    assertEquals(
        page.$.relaunch.innerText,
        loadTimeData.getString('aboutRelaunchAndPowerwash'));

    page.$.relaunch.click();
    await lifetimeBrowserProxy.whenCalled('relaunch');
  });

  /**
   * Test that buttons update according to incoming
   * 'update-status-changed' events for the case where the target channel
   * is less stable than current channel.
   */
  test('ButtonsUpdate_StableToBeta', async () => {
    aboutBrowserProxy.setChannels(BrowserChannel.STABLE, BrowserChannel.BETA);
    await initNewPage();

    fireStatusChanged(UpdateStatus.NEARLY_UPDATED, {powerwash: false});

    assertTrue(!!page.$.relaunch);
    assertFalse(page.$.relaunch.hidden);

    assertEquals(
        page.$.relaunch.innerText, loadTimeData.getString('aboutRelaunch'));

    page.$.relaunch.click();
    await lifetimeBrowserProxy.whenCalled('relaunch');
  });

  /**
   * The relaunch and powerwash button is shown if the powerwash flag is set
   * in the update status.
   */
  test('ButtonsUpdate_Powerwash', async () => {
    await initNewPage();

    fireStatusChanged(UpdateStatus.NEARLY_UPDATED, {powerwash: true});

    assertTrue(!!page.$.relaunch);
    assertFalse(page.$.relaunch.hidden);

    assertEquals(
        page.$.relaunch.innerText,
        loadTimeData.getString('aboutRelaunchAndPowerwash'));

    page.$.relaunch.click();
    await lifetimeBrowserProxy.whenCalled('relaunch');
  });

  /**
   * Test that release notes button can toggled by feature flags.
   * Test that release notes button handles offline/online mode properly.
   * page.shadowRoot.querySelector("#") is used to access items inside dom-if.
   */
  test('ReleaseNotes', async () => {
    const releaseNotes = null;

    /**
     * Checks the visibility of the "release notes" section when online.
     * @param {boolean} isShowing Whether the section is expected to be
     *     visible.
     */
    function checkReleaseNotesOnline(isShowing) {
      const releaseNotesOnlineEl =
          page.shadowRoot.querySelector('#releaseNotesOnline');
      assertEquals(isShowing, !!releaseNotesOnlineEl);
    }

    /**
     * Checks the visibility of the "release notes" for offline mode.
     * @param {boolean} isShowing Whether the section is expected to be
     *     visible.
     */
    function checkReleaseNotesOffline(isShowing) {
      const releaseNotesOfflineEl =
          page.shadowRoot.querySelector('#releaseNotesOffline');
      // According to
      // https://polymer-library.polymer-project.org/1.0/api/elements/dom-if
      // the element will not be removed from the dom if already rendered.
      // Can be just hidden instead for better performance.
      assertEquals(
          isShowing,
          !!releaseNotesOfflineEl &&
              window.getComputedStyle(releaseNotesOfflineEl).display !==
                  'none');
    }

    aboutBrowserProxy.setInternetConnection(false);
    await initNewPage();
    checkReleaseNotesOnline(false);
    checkReleaseNotesOffline(true);

    aboutBrowserProxy.setInternetConnection(true);
    await initNewPage();
    checkReleaseNotesOnline(true);
    checkReleaseNotesOffline(false);

    page.shadowRoot.querySelector('#releaseNotesOnline').click();
    return aboutBrowserProxy.whenCalled('launchReleaseNotes');
  });

  test('Deep link to release notes', async () => {
    loadTimeData.overrideValues({
      isDeepLinkingEnabled: true,
    });
    aboutBrowserProxy.setInternetConnection(false);
    await initNewPage();

    const params = new URLSearchParams();
    params.append('settingId', '1703');
    Router.getInstance().navigateTo(routes.ABOUT, params);

    flush();

    const deepLinkElement =
        page.shadowRoot.querySelector('#releaseNotesOffline')
            .shadowRoot.querySelector('cr-icon-button');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Release notes should be focused for settingId=1703.');
  });

  test('RegulatoryInfo', async () => {
    const regulatoryInfo = {text: 'foo', url: 'bar'};

    /**
     * Checks the visibility of the "regulatory info" section.
     * @param {boolean} isShowing Whether the section is expected to be
     *     visible.
     * @return {!Promise}
     */
    async function checkRegulatoryInfo(isShowing) {
      await aboutBrowserProxy.whenCalled('getRegulatoryInfo');
      const regulatoryInfoEl = page.$.regulatoryInfo;
      assertTrue(!!regulatoryInfoEl);
      assertEquals(isShowing, !regulatoryInfoEl.hidden);

      if (isShowing) {
        const img = regulatoryInfoEl.querySelector('img');
        assertTrue(!!img);
        assertEquals(regulatoryInfo.text, img.getAttribute('alt'));
        assertEquals(regulatoryInfo.url, img.getAttribute('src'));
      }
    }

    await checkRegulatoryInfo(false);
    aboutBrowserProxy.setRegulatoryInfo(regulatoryInfo);
    await initNewPage();
    await checkRegulatoryInfo(true);
  });

  test('TPMFirmwareUpdate', async () => {
    assertTrue(page.$.aboutTPMFirmwareUpdate.hidden);
    aboutBrowserProxy.setTPMFirmwareUpdateStatus({updateAvailable: true});
    aboutBrowserProxy.refreshTpmFirmwareUpdateStatus();
    assertFalse(page.$.aboutTPMFirmwareUpdate.hidden);
    page.$.aboutTPMFirmwareUpdate.click();
    await flushTasks();
    const dialog =
        page.shadowRoot.querySelector('os-settings-powerwash-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.$.dialog.open);
    dialog.shadowRoot.querySelector('#powerwash').click();
    const requestTpmFirmwareUpdate =
        await lifetimeBrowserProxy.whenCalled('factoryReset');
    assertTrue(requestTpmFirmwareUpdate);
  });

  test('DeviceEndOfLife', async () => {
    /**
     * Checks the visibility of the end of life message and icon.
     * @param {boolean} isShowing Whether the end of life UI is expected
     *     to be visible.
     * @return {!Promise}
     */
    async function checkHasEndOfLife(isShowing) {
      await aboutBrowserProxy.whenCalled('getEndOfLifeInfo');
      const {endOfLifeMessageContainer} = page.$;
      assertTrue(!!endOfLifeMessageContainer);

      assertEquals(isShowing, !endOfLifeMessageContainer.hidden);

      // Update status message should be hidden before user has
      // checked for updates.
      assertTrue(page.$.updateStatusMessage.hidden);

      fireStatusChanged(UpdateStatus.CHECKING);
      assertEquals(isShowing, page.$.updateStatusMessage.hidden);

      if (isShowing) {
        const icon = page.shadowRoot.querySelector('iron-icon');
        assertTrue(!!icon);
        assertEquals(null, icon.src);
        assertEquals('os-settings:end-of-life', icon.icon);

        const {checkForUpdates} = page.$;
        assertTrue(!!checkForUpdates);
        assertTrue(checkForUpdates.hidden);
      }
    }

    // Force test proxy to not respond to JS requests.
    // End of life message should still be hidden in this case.
    aboutBrowserProxy.setEndOfLifeInfo(new Promise(function(res, rej) {}));
    await initNewPage();
    await checkHasEndOfLife(false);
    aboutBrowserProxy.setEndOfLifeInfo({
      hasEndOfLife: true,
      endOfLifeAboutMessage: '',
    });
    await initNewPage();
    await checkHasEndOfLife(true);
    aboutBrowserProxy.setEndOfLifeInfo({
      hasEndOfLife: false,
      endOfLifeAboutMessage: '',
    });
    await initNewPage();
    await checkHasEndOfLife(false);
  });

  test('DeviceEndOfLifeIncentive', async () => {
    async function checkEndOfLifeIncentive(isShowing) {
      await aboutBrowserProxy.whenCalled('getEndOfLifeInfo');
      const eolSection = page.shadowRoot.querySelector('eol-offer-section');
      assertEquals(isShowing, !!eolSection);

      if (isShowing) {
        eolSection.$.eolIncentiveButton.click();
        await aboutBrowserProxy.whenCalled('endOfLifeIncentiveButtonClicked');
      }
    }

    aboutBrowserProxy.setEndOfLifeInfo({
      hasEndOfLife: false,
      endOfLifeAboutMessage: '',
      shouldShowEndOfLifeIncentive: false,
      shouldShowOfferText: false,
    });
    await initNewPage();
    await checkEndOfLifeIncentive(false);

    aboutBrowserProxy.setEndOfLifeInfo({
      hasEndOfLife: false,
      endOfLifeAboutMessage: '',
      shouldShowEndOfLifeIncentive: true,
      shouldShowOfferText: false,
    });
    await initNewPage();
    await checkEndOfLifeIncentive(true);
  });

  test('managed detailed build info page', async () => {
    loadTimeData.overrideValues({
      isManaged: true,
    });

    // Despite there being a valid end of life, the information is not
    // shown if the user is managed.
    aboutBrowserProxy.setEndOfLifeInfo({
      hasEndOfLife: true,
      aboutPageEndOfLifeMessage: 'message',
    });
    await initNewPage();

    const subpageTrigger =
        page.shadowRoot.querySelector('#detailedBuildInfoTrigger');
    assertTrue(!!subpageTrigger);
    subpageTrigger.click();
    const buildInfoPage =
        page.shadowRoot.querySelector('settings-detailed-build-info-subpage');
    assertTrue(!!buildInfoPage);
    assertTrue(!!buildInfoPage.$['endOfLifeSectionContainer']);
    assertTrue(buildInfoPage.$['endOfLifeSectionContainer'].hidden);
  });

  test('detailed build info page', async () => {
    loadTimeData.overrideValues({
      isManaged: false,
    });

    async function checkEndOfLifeSection() {
      await aboutBrowserProxy.whenCalled('getEndOfLifeInfo');
      const buildInfoPage =
          page.shadowRoot.querySelector('settings-detailed-build-info-subpage');
      assertTrue(!!buildInfoPage.$['endOfLifeSectionContainer']);
      assertFalse(buildInfoPage.$['endOfLifeSectionContainer'].hidden);
    }

    aboutBrowserProxy.setEndOfLifeInfo({
      hasEndOfLife: true,
      aboutPageEndOfLifeMessage: '',
    });
    await initNewPage();
    let subpageTrigger =
        page.shadowRoot.querySelector('#detailedBuildInfoTrigger');
    assertTrue(!!subpageTrigger);
    subpageTrigger.click();
    const buildInfoPage =
        page.shadowRoot.querySelector('settings-detailed-build-info-subpage');
    assertTrue(!!buildInfoPage);
    assertTrue(!!buildInfoPage.$['endOfLifeSectionContainer']);
    assertTrue(buildInfoPage.$['endOfLifeSectionContainer'].hidden);

    aboutBrowserProxy.setEndOfLifeInfo({
      hasEndOfLife: true,
      aboutPageEndOfLifeMessage: 'message',
    });
    await initNewPage();
    subpageTrigger = page.shadowRoot.querySelector('#detailedBuildInfoTrigger');
    assertTrue(!!subpageTrigger);
    subpageTrigger.click();
    checkEndOfLifeSection();
  });

  test(
      'Detailed build info subpage trigger is focused when returning ' +
          'from subpage',
      async () => {
        const triggerSelector = '#detailedBuildInfoTrigger';
        const subpageTrigger = page.shadowRoot.querySelector(triggerSelector);
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
            subpageTrigger, page.shadowRoot.activeElement,
            `${triggerSelector} should be focused.`);
      });

  function getBuildInfoPage() {
    const subpageTrigger =
        page.shadowRoot.querySelector('#detailedBuildInfoTrigger');
    assertTrue(!!subpageTrigger);
    subpageTrigger.click();
    const buildInfoPage =
        page.shadowRoot.querySelector('settings-detailed-build-info-subpage');
    assertTrue(!!buildInfoPage);
    return buildInfoPage;
  }

  test('Managed user auto update toggle in build info page', async () => {
    loadTimeData.overrideValues({
      isManaged: true,
    });

    async function checkManagedAutoUpdateToggle(isToggleEnabled, showToggle) {
      // Create the page.
      await initNewPage();
      // Set overrides + response values.
      aboutBrowserProxy.setManagedAutoUpdate(isToggleEnabled);

      loadTimeData.overrideValues({showAutoUpdateToggle: showToggle});
      // Go to the build info page.
      const buildInfoPage = getBuildInfoPage();
      // Wait for overrides + response values.
      await aboutBrowserProxy.whenCalled('isManagedAutoUpdateEnabled');

      const mau_toggle =
          buildInfoPage.shadowRoot.querySelector('#managedAutoUpdateToggle');

      if (showToggle) {
        assertTrue(!!mau_toggle);
        // Managed auto update toggle should always be disabled to toggle.
        assertTrue(!!mau_toggle.hasAttribute('disabled'));
        assertEquals(isToggleEnabled, mau_toggle.checked);
        // Consumer auto update toggle should not exist.
        assertFalse(!!buildInfoPage.shadowRoot.querySelector(
            '#consumerAutoUpdateToggle'));
      } else {
        assertFalse(!!mau_toggle);
      }
    }

    for (let i = 0; i < (1 << 2); i++) {
      await checkManagedAutoUpdateToggle(
          /*isToggleEnabled=*/ (i & 1) > 0,
          /*showToggle=*/ (i & 2) > 0);
    }
  });

  test('Consumer user auto update toggle in build info page', async () => {
    loadTimeData.overrideValues({
      isManaged: false,
    });

    async function checkConsumerAutoUpdateToggle(
        isEnabled, isTogglingAllowed, showToggle) {
      // Create the page.
      await initNewPage();
      // Set overrides + response values.
      loadTimeData.overrideValues({
        isConsumerAutoUpdateTogglingAllowed: isTogglingAllowed,
        showAutoUpdateToggle: showToggle,
      });
      aboutBrowserProxy.resetConsumerAutoUpdate(isEnabled);
      const prefs = {
        'settings': {
          'consumer_auto_update_toggle': {
            key: 'consumer_auto_update_toggle',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: isEnabled,
          },
        },
      };
      // Go to the build info page.
      const buildInfoPage = getBuildInfoPage();
      // Wait for overrides + response values.
      buildInfoPage.prefs = Object.assign({}, prefs);
      await Promise.all([
        aboutBrowserProxy.whenCalled('isConsumerAutoUpdateEnabled'),
        aboutBrowserProxy.whenCalled('setConsumerAutoUpdate'),
      ]);

      // Managed auto update toggle should not exist.
      assertFalse(
          !!buildInfoPage.shadowRoot.querySelector('#managedAutoUpdateToggle'));
      const cauToggle =
          buildInfoPage.shadowRoot.querySelector('#consumerAutoUpdateToggle');
      if (showToggle) {
        assertTrue(!!cauToggle);
        assertEquals(isTogglingAllowed, !cauToggle.disabled);
        assertEquals(isEnabled, cauToggle.checked);
      } else {
        assertFalse(!!cauToggle);
      }

      // Check dialog popup when toggling off.
      if (showToggle && isEnabled) {
        let dialog = buildInfoPage.shadowRoot.querySelector(
            'settings-consumer-auto-update-toggle-dialog');
        assertFalse(!!dialog);

        cauToggle.click();
        flush();

        dialog = buildInfoPage.shadowRoot.querySelector(
            'settings-consumer-auto-update-toggle-dialog');
        // Only when toggling is allowed, should the dialog popup.
        if (isTogglingAllowed) {
          assertTrue(!!dialog);
        } else {
          assertFalse(!!dialog);
        }
      }
    }

    for (let i = 0; i < (1 << 3); i++) {
      // showToggle should always be true when isTogglingAllowed is true, but
      // test to catch unintended behaviors from happening.
      await checkConsumerAutoUpdateToggle(
          /*isEnabled=*/ (i & 1) > 0,
          /*isTogglingAllowed=*/ (i & 2) > 0,
          /*showToggle=*/ (i & 4) > 0);
    }
  });

  test('GetHelp', function() {
    assertTrue(!!page.$.help);
    page.$.help.click();
    return aboutBrowserProxy.whenCalled('openOsHelpPage');
  });

  test('LaunchDiagnostics', async function() {
    loadTimeData.overrideValues({
      isDeepLinkingEnabled: true,
    });

    await initNewPage();
    flush();

    assertTrue(!!page.$.diagnostics);
    page.$.diagnostics.click();
    await aboutBrowserProxy.whenCalled('openDiagnostics');
  });

  test('Deep link to diagnostics', async () => {
    loadTimeData.overrideValues({
      isDeepLinkingEnabled: true,
    });

    await initNewPage();
    flush();

    const diagnosticsId = '1707';
    navigateToSettingsPageWithId(diagnosticsId);

    const deepLinkElement = getDeepLinkButtonElementById('diagnostics');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        `Diagnostics should be focused for settingId=${diagnosticsId}.`);
  });

  test('FirmwareUpdatesBadge No Updates', async function() {
    aboutBrowserProxy.setFirmwareUpdatesCount(0);
    await initNewPage();
    flush();
    await aboutBrowserProxy.whenCalled('getFirmwareUpdateCount');

    assertTrue(!!page.$.firmwareUpdateBadge);
    assertTrue(!!page.$.firmwareUpdateBadgeSeparator);

    assertTrue(page.$.firmwareUpdateBadge.hidden);
    assertTrue(page.$.firmwareUpdateBadgeSeparator.hidden);
  });

  test('FirmwareUpdatesBadge N Updates', async function() {
    for (let i = 1; i < 10; i++) {
      aboutBrowserProxy.setFirmwareUpdatesCount(i);
      await initNewPage();
      flush();
      await aboutBrowserProxy.whenCalled('getFirmwareUpdateCount');

      assertTrue(!!page.$.firmwareUpdateBadge);
      assertTrue(!!page.$.firmwareUpdateBadgeSeparator);

      assertFalse(page.$.firmwareUpdateBadge.hidden);
      assertEquals('os-settings:counter-' + i, page.$.firmwareUpdateBadge.icon);

      assertFalse(page.$.firmwareUpdateBadgeSeparator.hidden);
    }
  });

  test('FirmwareUpdatesBadge 10 Updates', async function() {
    aboutBrowserProxy.setFirmwareUpdatesCount(10);
    await initNewPage();
    flush();
    await aboutBrowserProxy.whenCalled('getFirmwareUpdateCount');

    assertTrue(!!page.$.firmwareUpdateBadge);
    assertTrue(!!page.$.firmwareUpdateBadgeSeparator);

    assertFalse(page.$.firmwareUpdateBadge.hidden);
    assertEquals('os-settings:counter-9', page.$.firmwareUpdateBadge.icon);

    assertFalse(page.$.firmwareUpdateBadgeSeparator.hidden);
  });

  test('LaunchFirmwareUpdates', async function() {
    loadTimeData.overrideValues({
      isDeepLinkingEnabled: true,
    });

    await initNewPage();
    flush();

    assertTrue(!!page.$.firmwareUpdates);
    page.$.firmwareUpdates.click();
    await aboutBrowserProxy.whenCalled('openFirmwareUpdatesPage');
  });

  test('Deep link to firmware updates', async () => {
    loadTimeData.overrideValues({
      isDeepLinkingEnabled: true,
    });

    await initNewPage();
    flush();

    const firmwareUpdatesId = '1709';
    navigateToSettingsPageWithId(firmwareUpdatesId);

    const deepLinkElement = getDeepLinkButtonElementById('firmwareUpdates');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        `Firmware updates should be focused for settingId=${
            firmwareUpdatesId}.`);
  });

  // Regression test for crbug.com/1220294
  test('Update button shown initially', async () => {
    aboutBrowserProxy.blockRefreshUpdateStatus();
    await initNewPage();

    const {checkForUpdates} = page.$;
    assertFalse(checkForUpdates.hidden);
  });

  test('Update button click moves focus', async () => {
    await initNewPage();

    const {checkForUpdates, updateStatusMessageInner} = page.$;
    checkForUpdates.click();
    await aboutBrowserProxy.whenCalled('requestUpdate');
    assertEquals(
        updateStatusMessageInner, getDeepActiveElement(),
        'Update status message should be focused.');
  });
});

suite('DetailedBuildInfoTest', function() {
  let page = null;
  let browserProxy = null;
  let deviceNameBrowserProxy = null;

  setup(function() {
    browserProxy = new TestAboutPageBrowserProxyChromeOS();
    deviceNameBrowserProxy = new TestDeviceNameBrowserProxy();
    AboutPageBrowserProxyImpl.setInstanceForTesting(browserProxy);
    DeviceNameBrowserProxyImpl.setInstanceForTesting(deviceNameBrowserProxy);
    PolymerTest.clearBody();
  });

  teardown(function() {
    page.remove();
    page = null;
    Router.getInstance().resetRouteForTesting();
  });

  test('Initialization', async () => {
    loadTimeData.overrideValues({
      isManaged: false,
    });

    page = document.createElement('settings-detailed-build-info-subpage');
    document.body.appendChild(page);

    await Promise.all([
      browserProxy.whenCalled('pageReady'),
      browserProxy.whenCalled('isConsumerAutoUpdateEnabled'),
      browserProxy.whenCalled('setConsumerAutoUpdate'),
      browserProxy.whenCalled('canChangeChannel'),
      browserProxy.whenCalled('getChannelInfo'),
      browserProxy.whenCalled('getVersionInfo'),
    ]);
  });

  test('InitializationManaged', async () => {
    loadTimeData.overrideValues({
      isManaged: true,
    });

    page = document.createElement('settings-detailed-build-info-subpage');
    document.body.appendChild(page);

    await Promise.all([
      browserProxy.whenCalled('pageReady'),
      browserProxy.whenCalled('isManagedAutoUpdateEnabled'),
      browserProxy.whenCalled('canChangeChannel'),
      browserProxy.whenCalled('getChannelInfo'),
      browserProxy.whenCalled('getVersionInfo'),
    ]);
  });

  /**
   * Checks whether the "change channel" button state (enabled/disabled)
   * correctly reflects whether the user is allowed to change channel (as
   * dictated by the browser via loadTimeData boolean).
   * @param {boolean} canChangeChannel Whether to simulate the case where
   *     changing channels is allowed.
   * @return {!Promise}
   */
  async function checkChangeChannelButton(canChangeChannel) {
    browserProxy.setCanChangeChannel(canChangeChannel);
    page = document.createElement('settings-detailed-build-info-subpage');
    document.body.appendChild(page);
    await browserProxy.whenCalled('canChangeChannel');
    await waitAfterNextRender(page);

    const changeChannelButton = page.shadowRoot.querySelector('cr-button');
    assertTrue(!!changeChannelButton);
    assertEquals(canChangeChannel, !changeChannelButton.disabled);
  }

  /**
   * Checks whether the change channel policy indicator shows correct state.
   * @param {boolean} canChangeChannel Whether to simulate the case where
   *     changing channels is allowed.
   * @param {boolean?} isManaged Whether device is enterprise managed.
   * @return {!Promise}
   */
  async function checkChangeChannelPolicyIndicator(
      canChangeChannel, isManaged) {
    if (isManaged !== undefined) {
      loadTimeData.overrideValues({
        aboutEnterpriseManaged: isManaged,
      });
    }
    browserProxy.setCanChangeChannel(canChangeChannel);
    page = document.createElement('settings-detailed-build-info-subpage');
    document.body.appendChild(page);
    await browserProxy.whenCalled('canChangeChannel');
    await waitAfterNextRender(page);

    const policyIndicator =
        page.shadowRoot.querySelector('#changeChannelPolicyIndicator');
    assertEquals(!policyIndicator, canChangeChannel);
    if (!canChangeChannel) {
      if (isManaged) {
        assertEquals(
            CrPolicyIndicatorType.DEVICE_POLICY, policyIndicator.indicatorType);
      } else {
        assertEquals(
            CrPolicyIndicatorType.OWNER, policyIndicator.indicatorType);
      }
    }
  }

  test(
      'Change channel button should be enabled when user can change',
      async function() {
        await checkChangeChannelButton(/*canChangeChannel=*/ true);
      });

  test(
      'Change channel button should be disabled when user can not change',
      async function() {
        await checkChangeChannelButton(/*canChangeChannel=*/ false);
      });

  test(
      'Change channel policy indicator should hide when user can change',
      async function() {
        await checkChangeChannelPolicyIndicator(/*canChangeChannel=*/ true);
      });

  test(
      'Change channel policy indicator should show when user can not change',
      async function() {
        // show managed by device policy.
        await checkChangeChannelPolicyIndicator(
            /*canChangeChannel=*/ false, /*isManaged*/ true);
        // show managed by owner.
        await checkChangeChannelPolicyIndicator(
            /*canChangeChannel=*/ false, /*isManaged*/ false);
      });

  /**
   * Checks whether the "change channel" button state (enabled/disabled)
   * is correct before getChannelInfo() returns
   * (see https://crbug.com/848750). Here, getChannelInfo() is blocked
   * manually until after the button check.
   */
  async function checkChangeChannelButtonWithDelayedChannelState(
      canChangeChannel) {
    const resolver = new PromiseResolver();
    browserProxy.getChannelInfo = async function() {
      await resolver.promise;
      this.methodCalled('getChannelInfo');
      return Promise.resolve(this.channelInfo_);
    };
    const result = await checkChangeChannelButton(canChangeChannel);
    resolver.resolve();
    return result;
  }

  test('ChangeChannel_EnabledWithDelayedChannelState', function() {
    return checkChangeChannelButtonWithDelayedChannelState(true);
  });

  test('ChangeChannel_DisabledWithDelayedChannelState', function() {
    return checkChangeChannelButtonWithDelayedChannelState(false);
  });

  test('Deep link to change channel', async () => {
    loadTimeData.overrideValues({
      isDeepLinkingEnabled: true,
    });
    page = document.createElement('settings-detailed-build-info-subpage');
    document.body.appendChild(page);

    const params = new URLSearchParams();
    params.append('settingId', '1700');
    Router.getInstance().navigateTo(routes.ABOUT_DETAILED_BUILD_INFO, params);

    flush();

    const deepLinkElement = page.shadowRoot.querySelector('cr-button');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Change channel button should be focused for settingId=1700.');
  });

  async function checkCopyBuildDetailsButton() {
    page = document.createElement('settings-detailed-build-info-subpage');
    document.body.appendChild(page);
    const copyBuildDetailsButton =
        page.shadowRoot.querySelector('cr-icon-button');
    await browserProxy.whenCalled('getVersionInfo');
    await browserProxy.whenCalled('getChannelInfo');
    await browserProxy.whenCalled('canChangeChannel');

    const expectedClipBoardText =
        `${loadTimeData.getString('application_label')}: ` +
        `${loadTimeData.getString('aboutBrowserVersion')}\n` +
        `Platform: ${browserProxy.versionInfo_.osVersion}\n` +
        `Channel: ${browserProxy.channelInfo_.targetChannel}\n` +
        `Firmware Version: ${browserProxy.versionInfo_.osFirmware}\n` +
        `ARC Enabled: ${loadTimeData.getBoolean('aboutIsArcEnabled')}\n` +
        `ARC: ${browserProxy.versionInfo_.arcVersion}\n` +
        `Enterprise Enrolled: ` +
        `${loadTimeData.getBoolean('aboutEnterpriseManaged')}\n` +
        `Developer Mode: ` +
        `${loadTimeData.getBoolean('aboutIsDeveloperMode')}`;

    assertTrue(!!copyBuildDetailsButton);
    await navigator.clipboard.readText().then(text => assertFalse(!!text));
    copyBuildDetailsButton.click();
    await navigator.clipboard.readText().then(
        text => assertEquals(text, expectedClipBoardText));
  }

  test('CheckCopyBuildDetails', function() {
    checkCopyBuildDetailsButton();
  });

  test('Deep link to change device name', async () => {
    loadTimeData.overrideValues({
      isDeepLinkingEnabled: true,
    });
    page = document.createElement('settings-detailed-build-info-subpage');
    document.body.appendChild(page);

    const params = new URLSearchParams();
    params.append('settingId', '1708');
    Router.getInstance().navigateTo(routes.ABOUT_DETAILED_BUILD_INFO, params);

    flush();

    const deepLinkElement = page.shadowRoot.querySelector('cr-icon-button');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Change device name button should be focused for settingId=1708.');
  });

  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  /**
   * Checks whether the "edit device name" button state (enabled/disabled)
   * correctly reflects whether the user is allowed to edit the name.
   * @param {string} testDeviceName
   * @param {DeviceNameState} deviceNameState
   * @return {!Promise}
   */
  function checkDeviceNameMetadata(testDeviceName, deviceNameState) {
    webUIListenerCallback(
        'settings.updateDeviceNameMetadata',
        {deviceName: testDeviceName, deviceNameState: deviceNameState});

    assertEquals(
        page.shadowRoot.querySelector('#deviceName').innerText, testDeviceName);

    let canEditDeviceName = null;
    switch (deviceNameState) {
      case (DeviceNameState.CAN_BE_MODIFIED):
        canEditDeviceName = true;
        break;
      default:
        canEditDeviceName = false;
    }

    const canEditDeviceNameButton =
        page.shadowRoot.querySelector('cr-icon-button');
    assertTrue(!!canEditDeviceNameButton);
    assertEquals(canEditDeviceName, !canEditDeviceNameButton.disabled);

    flushAsync();
    const policyIndicator =
        page.shadowRoot.querySelector('#editHostnamePolicyIndicator');
    if (deviceNameState === DeviceNameState.CAN_BE_MODIFIED) {
      assertFalse(!!policyIndicator);
    } else if (
        deviceNameState ===
        DeviceNameState.CANNOT_BE_MODIFIED_BECAUSE_OF_POLICIES) {
      assertTrue(!!policyIndicator);
      assertEquals(
          CrPolicyIndicatorType.DEVICE_POLICY, policyIndicator.indicatorType);
    } else if (
        deviceNameState ===
        DeviceNameState.CANNOT_BE_MODIFIED_BECAUSE_NOT_DEVICE_OWNER) {
      assertTrue(!!policyIndicator);
      assertEquals(CrPolicyIndicatorType.OWNER, policyIndicator.indicatorType);
    }
  }

  test('DeviceNameMetadata', async () => {
    loadTimeData.overrideValues({
      isHostnameSettingEnabled: true,
    });

    page = document.createElement('settings-detailed-build-info-subpage');
    document.body.appendChild(page);

    await deviceNameBrowserProxy.whenCalled('notifyReadyForDeviceName');
    checkDeviceNameMetadata('TestDeviceName1', DeviceNameState.CAN_BE_MODIFIED);

    // Verify that we can still make changes to device name metadata even
    // if notifyReadyForDeviceName() is not called again.
    checkDeviceNameMetadata(
        'TestDeviceName2',
        DeviceNameState.CANNOT_BE_MODIFIED_BECAUSE_OF_POLICIES);
    checkDeviceNameMetadata(
        'TestDeviceName3',
        DeviceNameState.CANNOT_BE_MODIFIED_BECAUSE_NOT_DEVICE_OWNER);
  });
});

suite('EditHostnameDialogTest', function() {
  let dialog = null;
  let deviceNameBrowserProxy = null;

  setup(function() {
    deviceNameBrowserProxy = new TestDeviceNameBrowserProxy();
    DeviceNameBrowserProxyImpl.setInstanceForTesting(deviceNameBrowserProxy);
    PolymerTest.clearBody();
  });

  teardown(function() {
    dialog.remove();
    dialog = null;
    Router.getInstance().resetRouteForTesting();
  });

  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  test('OpenAndCloseDialog', async () => {
    loadTimeData.overrideValues({
      isHostnameSettingEnabled: true,
    });

    const page = document.createElement('settings-detailed-build-info-subpage');
    document.body.appendChild(page);

    await deviceNameBrowserProxy.whenCalled('notifyReadyForDeviceName');
    const editHostnameButton =
        page.shadowRoot.querySelector('#editHostnameButton');
    assertTrue(!!editHostnameButton);
    editHostnameButton.click();
    flush();

    dialog = page.shadowRoot.querySelector('edit-hostname-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.$.dialog.open);

    dialog.shadowRoot.querySelector('#cancel').click();
    flush();
    assertFalse(dialog.$.dialog.open);
  });

  /**
   * @param {string} value The value of the input
   * @param {boolean} invalid If the input is invalid or not
   * @param {string} inputCount The length of value in string format
   */
  function assertInput(value, invalid, valueLength) {
    const inputBox = dialog.shadowRoot.querySelector('#deviceName');
    const inputCount = dialog.shadowRoot.querySelector('#inputCount');
    assertTrue(!!inputBox);
    assertTrue(!!inputCount);

    assertEquals(inputBox.value, value);
    assertEquals(inputBox.invalid, invalid);
    assertEquals(inputCount.textContent.trim(), valueLength + '/15');

    // Done button should be disabled when input is invalid and cancel button
    // should be always enabled.
    const doneButton = dialog.shadowRoot.querySelector('#done');
    const cancelButton = dialog.shadowRoot.querySelector('#cancel');
    assertTrue(!!doneButton);
    assertTrue(!!cancelButton);
    assertEquals(invalid, doneButton.disabled);
    assertTrue(!cancelButton.disabled);

    // Verify A11y labels and descriptions.
    assertEquals(
        inputBox.ariaLabel, dialog.i18n('aboutDeviceNameInputA11yLabel'));
    assertEquals(
        inputBox.ariaDescription,
        dialog.i18n('aboutDeviceNameConstraintsA11yDescription'));
    assertEquals(
        doneButton.ariaLabel,
        dialog.i18n('aboutDeviceNameDoneBtnA11yLabel', value));
  }

  test('CheckInputSanitizationAndValidity', async function() {
    loadTimeData.overrideValues({
      isHostnameSettingEnabled: true,
    });

    dialog = document.createElement('edit-hostname-dialog');
    document.body.appendChild(dialog);
    const inputBox = dialog.shadowRoot.querySelector('#deviceName');
    assertTrue(!!inputBox);

    // Test empty name, which is the value on opening dialog.
    assertInput(
        /*value=*/ '', /*invalid=*/ true, /*valueLength=*/ '0');

    // Test name with no emojis, under character limit.
    inputBox.value = '123456789';
    assertInput(
        /*value=*/ '123456789', /*invalid=*/ false,
        /*valueLength=*/ '9');

    // Test name with emojis, under character limit.
    inputBox.value = '1234🦤56789🧟';
    assertInput(
        /*value=*/ '123456789', /*invalid=*/ false,
        /*valueLength=*/ '9');

    // Test name with only emojis, under character limit.
    inputBox.value = '🦤🦤🦤🦤🦤';
    assertInput(
        /*value=*/ '', /*invalid=*/ true, /*valueLength=*/ '0');

    // Test name with no emojis, at character limit.
    inputBox.value = '123456789012345';
    assertInput(
        /*value=*/ '123456789012345', /*invalid=*/ false,
        /*valueLength=*/ '15');

    // Test name with emojis, at character limit.
    inputBox.value = '123456789012345🧟';
    assertInput(
        /*value=*/ '123456789012345', /*invalid=*/ false,
        /*valueLength=*/ '15');

    // Test name with only emojis, at character limit.
    inputBox.value =
        '🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤';
    assertInput(
        /*value=*/ '', /*invalid=*/ true, /*valueLength=*/ '0');

    // Test name with no emojis, above character limit.
    inputBox.value = '1234567890123456';
    assertInput(
        /*value=*/ '123456789012345', /*invalid=*/ true,
        /*valueLength=*/ '15');

    // Make sure input is not invalid once its value changes to a string below
    // the character limit. (Simulates the user pressing backspace once
    // they've reached the limit).
    inputBox.value = '12345678901234';
    assertInput(
        /*value=*/ '12345678901234', /*invalid=*/ false,
        /*valueLength=*/ '14');

    // Test name with emojis, above character limit.
    inputBox.value = '123456789012345🧟';
    assertInput(
        /*value=*/ '123456789012345', /*invalid=*/ false,
        /*valueLength=*/ '15');

    // Test name with only emojis, above character limit.
    inputBox.value =
        '🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤🦤';
    assertInput(
        /*value=*/ '', /*invalid=*/ true, /*valueLength=*/ '0');

    // Test invalid name because of empty space character.
    inputBox.value = 'Device Name';
    assertInput(
        /*value=*/ 'Device Name', /*invalid=*/ true, /*valueLength=*/ '11');

    // Test invalid name because of special characters.
    inputBox.value = 'Device&(#@!';
    assertInput(
        /*value=*/ 'Device&(#@!', /*invalid=*/ true, /*valueLength=*/ '11');

    // Test valid name with letters and numbers.
    inputBox.value = 'Device123';
    assertInput(
        /*value=*/ 'Device123', /*invalid=*/ false, /*valueLength=*/ '9');

    // Test valid name with letters and numbers and hyphens.
    inputBox.value = '-Device1-';
    assertInput(
        /*value=*/ '-Device1-', /*invalid=*/ false, /*valueLength=*/ '9');
  });

  test('SetDeviceName', async () => {
    loadTimeData.overrideValues({
      isHostnameSettingEnabled: true,
    });

    dialog = document.createElement('edit-hostname-dialog');
    document.body.appendChild(dialog);

    deviceNameBrowserProxy.setDeviceNameResult(
        SetDeviceNameResult.UPDATE_SUCCESSFUL);
    dialog.shadowRoot.querySelector('#deviceName').value = 'TestName';
    dialog.shadowRoot.querySelector('#done').click();
    flush();

    await deviceNameBrowserProxy.whenCalled('attemptSetDeviceName');
    assertEquals(deviceNameBrowserProxy.getDeviceName(), 'TestName');
    assertFalse(dialog.$.dialog.open);
  });
});

suite('ChannelSwitcherDialogTest', function() {
  let dialog = null;
  let radioButtons = null;
  let browserProxy = null;
  let currentChannel;

  setup(async function() {
    currentChannel = BrowserChannel.BETA;
    browserProxy = new TestAboutPageBrowserProxyChromeOS();
    browserProxy.setChannels(currentChannel, currentChannel);
    AboutPageBrowserProxyImpl.setInstanceForTesting(browserProxy);
    PolymerTest.clearBody();
    dialog = document.createElement('settings-channel-switcher-dialog');
    document.body.appendChild(dialog);

    radioButtons = dialog.shadowRoot.querySelectorAll('cr-radio-button');
    assertEquals(3, radioButtons.length);
    await browserProxy.whenCalled('getChannelInfo');
  });

  teardown(function() {
    dialog.remove();
  });

  test('Initialization', function() {
    const radioGroup = dialog.shadowRoot.querySelector('cr-radio-group');
    assertTrue(!!radioGroup);
    assertTrue(!!dialog.$.warningSelector);
    assertTrue(!!dialog.$.changeChannel);
    assertTrue(!!dialog.$.changeChannelAndPowerwash);

    // Check that upon initialization the radio button corresponding to
    // the current release channel is pre-selected.
    assertEquals(currentChannel, radioGroup.selected);
    assertEquals(dialog.$.warningSelector.selected, -1);

    // Check that action buttons are hidden when current and target
    // channel are the same.
    assertTrue(dialog.$.changeChannel.hidden);
    assertTrue(dialog.$.changeChannelAndPowerwash.hidden);
  });

  // Test case where user switches to a less stable channel.
  test('ChangeChannel_LessStable', async () => {
    assertEquals(BrowserChannel.DEV, radioButtons.item(2).name);
    radioButtons.item(2).click();
    flush();

    await browserProxy.whenCalled('getChannelInfo');
    assertEquals(dialog.$.warningSelector.selected, 2);
    // Check that only the "Change channel" button becomes visible.
    assertTrue(dialog.$.changeChannelAndPowerwash.hidden);
    assertFalse(dialog.$.changeChannel.hidden);

    const whenTargetChannelChangedFired =
        eventToPromise('target-channel-changed', dialog);

    dialog.$.changeChannel.click();
    const [channel, isPowerwashAllowed] =
        await browserProxy.whenCalled('setChannel');
    assertEquals(BrowserChannel.DEV, channel);
    assertFalse(isPowerwashAllowed);
    const {detail} = await whenTargetChannelChangedFired;
    assertEquals(BrowserChannel.DEV, detail);
  });

  // Test case where user switches to a more stable channel.
  test('ChangeChannel_MoreStable', async () => {
    assertEquals(BrowserChannel.STABLE, radioButtons.item(0).name);
    radioButtons.item(0).click();
    flush();

    await browserProxy.whenCalled('getChannelInfo');
    assertEquals(dialog.$.warningSelector.selected, 1);
    // Check that only the "Change channel and Powerwash" button becomes
    // visible.
    assertFalse(dialog.$.changeChannelAndPowerwash.hidden);
    assertTrue(dialog.$.changeChannel.hidden);

    const whenTargetChannelChangedFired =
        eventToPromise('target-channel-changed', dialog);

    dialog.$.changeChannelAndPowerwash.click();
    const [channel, isPowerwashAllowed] =
        await browserProxy.whenCalled('setChannel');
    assertEquals(BrowserChannel.STABLE, channel);
    assertTrue(isPowerwashAllowed);
    const {detail} = await whenTargetChannelChangedFired;
    assertEquals(BrowserChannel.STABLE, detail);
  });
});

suite('Consumer auto update dialog popup', function() {
  let dialog = null;
  let browserProxy = null;
  let events;

  setup(function() {
    events = [];
    browserProxy = new TestAboutPageBrowserProxyChromeOS();
    AboutPageBrowserProxyImpl.setInstanceForTesting(browserProxy);
    PolymerTest.clearBody();
    dialog =
        document.createElement('settings-consumer-auto-update-toggle-dialog');
    document.body.appendChild(dialog);
  });

  teardown(function() {
    dialog.remove();
  });

  function getButtonEventPromise() {
    return new Promise(
        (resolve) =>
            dialog.addEventListener('set-consumer-auto-update', (e) => {
              events.push(e);
              resolve();
            }));
  }

  async function clickButton(buttonId, shouldEnable) {
    const ButtonEventPromise = getButtonEventPromise();
    const button = dialog.shadowRoot.querySelector(buttonId);
    assertTrue(!!button);
    button.click();
    await ButtonEventPromise;
    assertEquals(1, events.length);
    assertEquals(shouldEnable, events[0].detail.item);
  }

  test('click turn off button fires disable event', async function() {
    await clickButton('#turnOffButton', false);
  });

  test('click keep updates button fires enable event', async function() {
    await clickButton('#keepUpdatesButton', true);
  });
});

suite('AboutPageTest_OfficialBuild', function() {
  let page = null;
  let browserProxy = null;

  setup(function() {
    browserProxy = new TestAboutPageBrowserProxyChromeOS();
    AboutPageBrowserProxyImpl.setInstanceForTesting(browserProxy);
    PolymerTest.clearBody();
    page = document.createElement('os-about-page');
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
    page = null;
    Router.getInstance().resetRouteForTesting();
  });

  test('ReportAnIssue', async function() {
    assertTrue(!!page.$.reportIssue);
    page.$.reportIssue.click();
    await browserProxy.whenCalled('openFeedbackDialog');
  });

  test('Deep link to report an issue', async () => {
    loadTimeData.overrideValues({
      isDeepLinkingEnabled: true,
    });

    const params = new URLSearchParams();
    params.append('settingId', '1705');
    Router.getInstance().navigateTo(routes.ABOUT, params);

    flush();

    const deepLinkElement = page.shadowRoot.querySelector('#reportIssue')
                                .shadowRoot.querySelector('cr-icon-button');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Report an issue button should be focused for settingId=1705.');
  });

  test('Deep link to terms of service', async () => {
    loadTimeData.overrideValues({
      isDeepLinkingEnabled: true,
    });

    const params = new URLSearchParams();
    params.append('settingId', '1706');
    Router.getInstance().navigateTo(routes.ABOUT, params);

    flush();

    const deepLinkElement = page.shadowRoot.querySelector('#aboutProductTos');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Terms of service link should be focused for settingId=1706.');
  });
});
