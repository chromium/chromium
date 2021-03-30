// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {TestAboutPageBrowserProxyChromeOS} from './test_about_page_browser_proxy_chromeos.m.js';
// #import {TestDeviceNameBrowserProxy} from './test_device_name_browser_proxy.m.js';
// #import {BrowserChannel,UpdateStatus,Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {AboutPageBrowserProxyImpl,DeviceNameBrowserProxyImpl,LifetimeBrowserProxyImpl} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {TestLifetimeBrowserProxy} from './test_os_lifetime_browser_proxy.m.js';
// #import {eventToPromise,flushTasks,waitAfterNextRender} from 'chrome://test/test_util.m.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// clang-format on

cr.define('settings_about_page', function() {
  suite('AboutPageTest', function() {
    let page = null;

    /** @type {?settings.TestAboutPageBrowserProxyChromeOS} */
    let aboutBrowserProxy = null;

    /** @type {?settings.TestLifetimeBrowserProxy} */
    let lifetimeBrowserProxy = null;

    const SPINNER_ICON = 'chrome://resources/images/throbber_small.svg';

    setup(function() {
      lifetimeBrowserProxy = new settings.TestLifetimeBrowserProxy();
      settings.LifetimeBrowserProxyImpl.instance_ = lifetimeBrowserProxy;

      aboutBrowserProxy = new TestAboutPageBrowserProxyChromeOS();
      settings.AboutPageBrowserProxyImpl.instance_ = aboutBrowserProxy;
      return initNewPage();
    });

    teardown(function() {
      page.remove();
      page = null;
      settings.Router.getInstance().resetRouteForTesting();
    });

    /**
     * @param {!UpdateStatus} status
     * @param {{
     *   progress: number|undefined,
     *   message: string|undefined
     *   rollback: bool|undefined
     *   powerwash: bool|undefined
     * }} opt_options
     */
    function fireStatusChanged(status, opt_options) {
      const options = opt_options || {};
      cr.webUIListenerCallback('update-status-changed', {
        progress: options.progress === undefined ? 1 : options.progress,
        message: options.message,
        status: status,
        rollback: options.rollback,
        powerwash: options.powerwash,
      });
    }

    /** @return {!Promise} */
    function initNewPage() {
      aboutBrowserProxy.reset();
      lifetimeBrowserProxy.reset();
      PolymerTest.clearBody();
      page = document.createElement('os-settings-about-page');
      settings.Router.getInstance().navigateTo(settings.routes.ABOUT);
      document.body.appendChild(page);
      return Promise.all([
        aboutBrowserProxy.whenCalled('getChannelInfo'),
        aboutBrowserProxy.whenCalled('refreshUpdateStatus'),
        aboutBrowserProxy.whenCalled('refreshTPMFirmwareUpdateStatus'),
        aboutBrowserProxy.whenCalled('checkInternetConnection'),
      ]);
    }

    /**
     * Test that the status icon and status message update according to
     * incoming 'update-status-changed' events.
     */
    test('IconAndMessageUpdates', function() {
      const icon = page.$$('iron-icon');
      assertTrue(!!icon);
      const statusMessageEl = page.$$('#updateStatusMessage div');
      let previousMessageText = statusMessageEl.textContent;

      fireStatusChanged(UpdateStatus.CHECKING);
      assertEquals(SPINNER_ICON, icon.src);
      assertEquals(null, icon.getAttribute('icon'));
      assertNotEquals(previousMessageText, statusMessageEl.textContent);
      previousMessageText = statusMessageEl.textContent;

      fireStatusChanged(UpdateStatus.UPDATING, {progress: 0});
      assertEquals(SPINNER_ICON, icon.src);
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
      assertEquals(0, statusMessageEl.textContent.trim().length);

      fireStatusChanged(UpdateStatus.FAILED);
      assertEquals(null, icon.src);
      assertEquals('cr:error', icon.icon);
      assertEquals(0, statusMessageEl.textContent.trim().length);

      fireStatusChanged(UpdateStatus.DISABLED);
      assertEquals(null, icon.src);
      assertEquals(null, icon.getAttribute('icon'));
      assertEquals(0, statusMessageEl.textContent.trim().length);
    });

    test('ErrorMessageWithHtml', function() {
      const htmlError = 'hello<br>there<br>was<pre>an</pre>error';
      fireStatusChanged(UpdateStatus.FAILED, {message: htmlError});
      const statusMessageEl = page.$$('#updateStatusMessage div');
      assertEquals(htmlError, statusMessageEl.innerHTML);
    });

    test('FailedLearnMoreLink', function() {
      // Check that link is shown when update failed.
      fireStatusChanged(UpdateStatus.FAILED, {message: 'foo'});
      assertTrue(!!page.$$('#updateStatusMessage a:not([hidden])'));

      // Check that link is hidden when update hasn't failed.
      fireStatusChanged(UpdateStatus.UPDATED, {message: ''});
      assertTrue(!!page.$$('#updateStatusMessage a[hidden]'));
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

    test('NoInternet', function() {
      assertTrue(page.$.updateStatusMessage.hidden);
      aboutBrowserProxy.sendStatusNoInternet();
      Polymer.dom.flush();
      assertFalse(page.$.updateStatusMessage.hidden);
      assertNotEquals(
          page.$.updateStatusMessage.innerHTML.includes('no internet'));
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
      assertAllHidden();
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
     * page.$$("#") is used to access items inside dom-if.
     */
    test('ReleaseNotes', async () => {
      const releaseNotes = null;

      /**
       * Checks the visibility of the "release notes" section when online.
       * @param {boolean} isShowing Whether the section is expected to be
       *     visible.
       */
      function checkReleaseNotesOnline(isShowing) {
        const releaseNotesOnlineEl = page.$$('#releaseNotesOnline');
        assertEquals(isShowing, !!releaseNotesOnlineEl);
      }

      /**
       * Checks the visibility of the "release notes" for offline mode.
       * @param {boolean} isShowing Whether the section is expected to be
       *     visible.
       */
      function checkReleaseNotesOffline(isShowing) {
        const releaseNotesOfflineEl = page.$$('#releaseNotesOffline');
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

      page.$$('#releaseNotesOnline').click();
      return aboutBrowserProxy.whenCalled('launchReleaseNotes');
    });

    test('Deep link to release notes', async () => {
      loadTimeData.overrideValues({
        isDeepLinkingEnabled: true,
      });
      aboutBrowserProxy.setInternetConnection(false);
      await initNewPage();

      const params = new URLSearchParams;
      params.append('settingId', '1703');
      settings.Router.getInstance().navigateTo(
          settings.routes.ABOUT_ABOUT, params);

      Polymer.dom.flush();

      const deepLinkElement =
          page.$$('#releaseNotesOffline').$$('cr-icon-button');
      await test_util.waitAfterNextRender(deepLinkElement);
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
      aboutBrowserProxy.refreshTPMFirmwareUpdateStatus();
      assertFalse(page.$.aboutTPMFirmwareUpdate.hidden);
      page.$.aboutTPMFirmwareUpdate.click();
      await test_util.flushTasks();
      const dialog = page.$$('os-settings-powerwash-dialog');
      assertTrue(!!dialog);
      assertTrue(dialog.$.dialog.open);
      dialog.$$('#powerwash').click();
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
          const icon = page.$$('iron-icon');
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
      page.scroller = page.offsetParent;
      assertTrue(!!page.$['detailed-build-info-trigger']);
      page.$['detailed-build-info-trigger'].click();
      const buildInfoPage = page.$$('settings-detailed-build-info');
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
        const buildInfoPage = page.$$('settings-detailed-build-info');
        assertTrue(!!buildInfoPage.$['endOfLifeSectionContainer']);
        assertFalse(buildInfoPage.$['endOfLifeSectionContainer'].hidden);
      }

      aboutBrowserProxy.setEndOfLifeInfo({
        hasEndOfLife: true,
        aboutPageEndOfLifeMessage: '',
      });
      await initNewPage();
      page.scroller = page.offsetParent;
      assertTrue(!!page.$['detailed-build-info-trigger']);
      page.$['detailed-build-info-trigger'].click();
      const buildInfoPage = page.$$('settings-detailed-build-info');
      assertTrue(!!buildInfoPage);
      assertTrue(!!buildInfoPage.$['endOfLifeSectionContainer']);
      assertTrue(buildInfoPage.$['endOfLifeSectionContainer'].hidden);

      aboutBrowserProxy.setEndOfLifeInfo({
        hasEndOfLife: true,
        aboutPageEndOfLifeMessage: 'message',
      });
      await initNewPage();
      page.scroller = page.offsetParent;
      assertTrue(!!page.$['detailed-build-info-trigger']);
      page.$['detailed-build-info-trigger'].click();
      checkEndOfLifeSection();
    });

    test('GetHelp', function() {
      assertTrue(!!page.$.help);
      page.$.help.click();
      return aboutBrowserProxy.whenCalled('openOsHelpPage');
    });

    test('LaunchDiagnostics', async function() {
      loadTimeData.overrideValues({
        isDeepLinkingEnabled: true,
        diagnosticsAppEnabled: true,
      });

      await initNewPage();
      Polymer.dom.flush();

      assertTrue(!!page.$.diagnostics);
      page.$.diagnostics.click();
      await aboutBrowserProxy.whenCalled('openDiagnostics');
    });

    test('Deep link to diagnostics', async () => {
      loadTimeData.overrideValues({
        isDeepLinkingEnabled: true,
        diagnosticsAppEnabled: true,
      });

      await initNewPage();
      Polymer.dom.flush();

      const params = new URLSearchParams;
      params.append('settingId', '1707');  // Setting::kDiagnostics
      settings.Router.getInstance().navigateTo(
          settings.routes.ABOUT_ABOUT, params);

      Polymer.dom.flush();

      const deepLinkElement = page.$$('#diagnostics').$$('cr-icon-button');
      await test_util.waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Diagnostics should be focused for settingId=1707.');
    });
  });

  suite('DetailedBuildInfoTest', function() {
    let page = null;
    let browserProxy = null;
    let deviceNameBrowserProxy = null;

    setup(function() {
      browserProxy = new TestAboutPageBrowserProxyChromeOS();
      deviceNameBrowserProxy = new TestDeviceNameBrowserProxy();
      settings.AboutPageBrowserProxyImpl.instance_ = browserProxy;
      DeviceNameBrowserProxyImpl.instance_ = deviceNameBrowserProxy;
      PolymerTest.clearBody();
    });

    teardown(function() {
      page.remove();
      page = null;
      settings.Router.getInstance().resetRouteForTesting();
    });

    test('Initialization', async () => {
      page = document.createElement('settings-detailed-build-info');
      document.body.appendChild(page);

      await Promise.all([
        browserProxy.whenCalled('pageReady'),
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
      page = document.createElement('settings-detailed-build-info');
      document.body.appendChild(page);
      await browserProxy.whenCalled('canChangeChannel');
      const changeChannelButton = page.$$('cr-button');
      assertTrue(!!changeChannelButton);
      assertEquals(canChangeChannel, !changeChannelButton.disabled);
    }

    test('ChangeChannel_Enabled', function() {
      return checkChangeChannelButton(true);
    });

    test('ChangeChannel_Disabled', function() {
      return checkChangeChannelButton(false);
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
      page = document.createElement('settings-detailed-build-info');
      document.body.appendChild(page);

      const params = new URLSearchParams;
      params.append('settingId', '1700');
      settings.Router.getInstance().navigateTo(
          settings.routes.DETAILED_BUILD_INFO, params);

      Polymer.dom.flush();

      const deepLinkElement = page.$$('cr-button');
      await test_util.waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Change channel button should be focused for settingId=1700.');
    });

    async function checkCopyBuildDetailsButton() {
      page = document.createElement('settings-detailed-build-info');
      document.body.appendChild(page);
      const copyBuildDetailsButton = page.$$('cr-icon-button');
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

    test('DeviceName', async () => {
      loadTimeData.overrideValues({
        isHostnameSettingEnabled: true,
      });

      deviceNameBrowserProxy.setDeviceName('TestDeviceName');

      page = document.createElement('settings-detailed-build-info');
      document.body.appendChild(page);
      await deviceNameBrowserProxy.whenCalled('getDeviceNameMetadata');

      assertEquals(page.$$('#deviceName').innerText, 'TestDeviceName');
    });
  });

  suite('ChannelSwitcherDialogTest', function() {
    let dialog = null;
    let radioButtons = null;
    let browserProxy = null;
    let currentChannel;

    setup(function() {
      currentChannel = BrowserChannel.BETA;
      browserProxy = new TestAboutPageBrowserProxyChromeOS();
      browserProxy.setChannels(currentChannel, currentChannel);
      settings.AboutPageBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();
      dialog = document.createElement('settings-channel-switcher-dialog');
      document.body.appendChild(dialog);

      radioButtons = dialog.shadowRoot.querySelectorAll('cr-radio-button');
      assertEquals(3, radioButtons.length);
      return browserProxy.whenCalled('getChannelInfo');
    });

    teardown(function() {
      dialog.remove();
    });

    test('Initialization', function() {
      const radioGroup = dialog.$$('cr-radio-group');
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
      Polymer.dom.flush();

      await browserProxy.whenCalled('getChannelInfo');
      assertEquals(dialog.$.warningSelector.selected, 2);
      // Check that only the "Change channel" button becomes visible.
      assertTrue(dialog.$.changeChannelAndPowerwash.hidden);
      assertFalse(dialog.$.changeChannel.hidden);

      const whenTargetChannelChangedFired =
          test_util.eventToPromise('target-channel-changed', dialog);

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
      Polymer.dom.flush();

      await browserProxy.whenCalled('getChannelInfo');
      assertEquals(dialog.$.warningSelector.selected, 1);
      // Check that only the "Change channel and Powerwash" button becomes
      // visible.
      assertFalse(dialog.$.changeChannelAndPowerwash.hidden);
      assertTrue(dialog.$.changeChannel.hidden);

      const whenTargetChannelChangedFired =
          test_util.eventToPromise('target-channel-changed', dialog);

      dialog.$.changeChannelAndPowerwash.click();
      const [channel, isPowerwashAllowed] =
          await browserProxy.whenCalled('setChannel');
      assertEquals(BrowserChannel.STABLE, channel);
      assertTrue(isPowerwashAllowed);
      const {detail} = await whenTargetChannelChangedFired;
      assertEquals(BrowserChannel.STABLE, detail);
    });
  });

  suite('AboutPageTest_OfficialBuild', function() {
    let page = null;
    let browserProxy = null;

    setup(function() {
      browserProxy = new TestAboutPageBrowserProxyChromeOS();
      settings.AboutPageBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();
      page = document.createElement('os-settings-about-page');
      document.body.appendChild(page);
    });

    teardown(function() {
      page.remove();
      page = null;
      settings.Router.getInstance().resetRouteForTesting();
    });

    test('ReportAnIssue', function() {
      assertTrue(!!page.$.reportIssue);
      page.$.reportIssue.click();
      return browserProxy.whenCalled('openFeedbackDialog');
    });

    test('Deep link to report an issue', async () => {
      loadTimeData.overrideValues({
        isDeepLinkingEnabled: true,
      });

      const params = new URLSearchParams;
      params.append('settingId', '1705');
      settings.Router.getInstance().navigateTo(
          settings.routes.ABOUT_ABOUT, params);

      Polymer.dom.flush();

      const deepLinkElement = page.$$('#reportIssue').$$('cr-icon-button');
      await test_util.waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Report an issue button should be focused for settingId=1705.');
    });

    test('Deep link to terms of service', async () => {
      loadTimeData.overrideValues({
        isDeepLinkingEnabled: true,
      });

      const params = new URLSearchParams;
      params.append('settingId', '1706');
      settings.Router.getInstance().navigateTo(
          settings.routes.ABOUT_ABOUT, params);

      Polymer.dom.flush();

      const deepLinkElement = page.$$('#aboutProductTos');
      await test_util.waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Terms of service link should be focused for settingId=1706.');
    });
  });

  // #cr_define_end
  return {};
});
