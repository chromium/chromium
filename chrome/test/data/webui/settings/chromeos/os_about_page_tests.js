// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_about_page', function() {
  function registerAboutPageTests() {
    /**
     * @param {!UpdateStatus} status
     * @param {{
     *   progress: number|undefined,
     *   message: string|undefined
     * }} opt_options
     */
    function fireStatusChanged(status, opt_options) {
      const options = opt_options || {};
      cr.webUIListenerCallback('update-status-changed', {
        progress: options.progress === undefined ? 1 : options.progress,
        message: options.message,
        status: status,
      });
    }

    suite('AboutPageTest', function() {
      let page = null;

      /** @type {?settings.TestAboutPageBrowserProxy} */
      let aboutBrowserProxy = null;

      /** @type {?settings.TestLifetimeBrowserProxy} */
      let lifetimeBrowserProxy = null;

      const SPINNER_ICON = 'chrome://resources/images/throbber_small.svg';

      setup(function() {
        lifetimeBrowserProxy = new settings.TestLifetimeBrowserProxy();
        settings.LifetimeBrowserProxyImpl.instance_ = lifetimeBrowserProxy;

        aboutBrowserProxy = new TestAboutPageBrowserProxy();
        settings.AboutPageBrowserProxyImpl.instance_ = aboutBrowserProxy;
        return initNewPage();
      });

      teardown(function() {
        page.remove();
        page = null;
      });

      /** @return {!Promise} */
      function initNewPage() {
        aboutBrowserProxy.reset();
        lifetimeBrowserProxy.reset();
        PolymerTest.clearBody();
        page = document.createElement('os-settings-about-page');
        settings.navigateTo(settings.routes.ABOUT);
        document.body.appendChild(page);
        return Promise.all([
          aboutBrowserProxy.whenCalled('getChannelInfo'),
          aboutBrowserProxy.whenCalled('refreshUpdateStatus'),
          aboutBrowserProxy.whenCalled('refreshTPMFirmwareUpdateStatus'),
          aboutBrowserProxy.whenCalled('getEnabledReleaseNotes'),
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
        const {checkForUpdates, relaunch, relaunchAndPowerwash} = page.$;

        assertTrue(!!relaunch);
        assertTrue(!!relaunchAndPowerwash);
        assertTrue(!!checkForUpdates);

        function assertAllHidden() {
          assertTrue(checkForUpdates.hidden);
          assertTrue(relaunch.hidden);
          assertTrue(relaunchAndPowerwash.hidden);
          // Ensure that when all buttons are hidden, the container is also
          // hidden.
          assertTrue(page.$.buttonContainer.hidden);
        }

        // Check that |UPDATED| status is ignored if the user has not
        // explicitly checked for updates yet.
        fireStatusChanged(UpdateStatus.UPDATED);
        assertFalse(checkForUpdates.hidden);
        assertTrue(relaunch.hidden);
        assertTrue(relaunchAndPowerwash.hidden);

        // Check that the "Check for updates" button gets hidden for certain
        // UpdateStatus values, even if the CHECKING state was never
        // encountered (for example triggering update from crosh command
        // line).
        fireStatusChanged(UpdateStatus.UPDATING);
        assertAllHidden();
        fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
        assertTrue(checkForUpdates.hidden);
        assertFalse(relaunch.hidden);
        assertTrue(relaunchAndPowerwash.hidden);

        fireStatusChanged(UpdateStatus.CHECKING);
        assertAllHidden();

        fireStatusChanged(UpdateStatus.UPDATING);
        assertAllHidden();

        fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
        assertTrue(checkForUpdates.hidden);
        assertFalse(relaunch.hidden);
        assertTrue(relaunchAndPowerwash.hidden);

        fireStatusChanged(UpdateStatus.UPDATED);
        assertAllHidden();

        fireStatusChanged(UpdateStatus.FAILED);
        assertFalse(checkForUpdates.hidden);
        assertTrue(relaunch.hidden);
        assertTrue(relaunchAndPowerwash.hidden);

        fireStatusChanged(UpdateStatus.DISABLED);
        assertAllHidden();

        fireStatusChanged(UpdateStatus.DISABLED_BY_ADMIN);
        assertAllHidden();
      });

      /**
       * Test that buttons update according to incoming
       * 'update-status-changed' events for the case where the target channel
       * is more stable than current channel.
       */
      test('ButtonsUpdate_BetaToStable', async () => {
        aboutBrowserProxy.setChannels(
            BrowserChannel.BETA, BrowserChannel.STABLE);
        aboutBrowserProxy.setUpdateStatus(UpdateStatus.NEARLY_UPDATED);

        await initNewPage();
        assertTrue(!!page.$.relaunch);
        assertTrue(!!page.$.relaunchAndPowerwash);

        assertTrue(page.$.relaunch.hidden);
        assertFalse(page.$.relaunchAndPowerwash.hidden);

        page.$.relaunchAndPowerwash.click();
        const requestTpmFirmwareUpdate =
            await lifetimeBrowserProxy.whenCalled('factoryReset');
        assertFalse(requestTpmFirmwareUpdate);
      });

      /**
       * Test that buttons update according to incoming
       * 'update-status-changed' events for the case where the target channel
       * is less stable than current channel.
       */
      test('ButtonsUpdate_StableToBeta', async () => {
        aboutBrowserProxy.setChannels(
            BrowserChannel.STABLE, BrowserChannel.BETA);
        aboutBrowserProxy.setUpdateStatus(UpdateStatus.NEARLY_UPDATED);

        await initNewPage();
        assertTrue(!!page.$.relaunch);
        assertTrue(!!page.$.relaunchAndPowerwash);

        assertFalse(page.$.relaunch.hidden);
        assertTrue(page.$.relaunchAndPowerwash.hidden);

        page.$.relaunch.click();
        await lifetimeBrowserProxy.whenCalled('relaunch');
      });

      /**
       * Test that buttons update as a result of receiving a
       * 'target-channel-changed' event (normally fired from
       * <settings-channel-switcher-dialog>).
       */
      test('ButtonsUpdate_TargetChannelChangedEvent', async () => {
        aboutBrowserProxy.setChannels(BrowserChannel.BETA, BrowserChannel.BETA);
        aboutBrowserProxy.setUpdateStatus(UpdateStatus.NEARLY_UPDATED);

        await initNewPage();
        assertFalse(page.$.relaunch.hidden);
        assertTrue(page.$.relaunchAndPowerwash.hidden);

        page.fire('target-channel-changed', BrowserChannel.DEV);
        assertFalse(page.$.relaunch.hidden);
        assertTrue(page.$.relaunchAndPowerwash.hidden);

        page.fire('target-channel-changed', BrowserChannel.STABLE);
        assertTrue(page.$.relaunch.hidden);
        assertFalse(page.$.relaunchAndPowerwash.hidden);
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
         * @return {!Promise}
         */
        async function checkReleaseNotesOnline(isShowing) {
          await aboutBrowserProxy.whenCalled('getEnabledReleaseNotes');
          const releaseNotesOnlineEl = page.$$('#releaseNotesOnline');
          assertTrue(!!releaseNotesOnlineEl);
          assertEquals(isShowing, !releaseNotesOnlineEl.hidden);
        }

        /**
         * Checks the visibility of the "release notes" for offline mode.
         * @param {boolean} isShowing Whether the section is expected to be
         *     visible.
         * @return {!Promise}
         */
        async function checkReleaseNotesOffline(isShowing) {
          await aboutBrowserProxy.whenCalled('getEnabledReleaseNotes');
          const releaseNotesOfflineEl = page.$$('#releaseNotesOffline');
          assertTrue(!!releaseNotesOfflineEl);
          assertEquals(isShowing, !releaseNotesOfflineEl.hidden);
        }

        /**
         * Checks the visibility of the "release notes" section when disabled.
         * @return {!Promise}
         */
        async function checkReleaseNotesDisabled() {
          await aboutBrowserProxy.whenCalled('getEnabledReleaseNotes');
          const releaseNotesOnlineEl = page.$$('#releaseNotesOnline');
          assertTrue(!releaseNotesOnlineEl);
          const releaseNotesOfflineEl = page.$$('#releaseNotesOffline');
          assertTrue(!releaseNotesOfflineEl);
        }

        aboutBrowserProxy.setReleaseNotes(false);
        aboutBrowserProxy.setInternetConnection(false);
        await initNewPage();
        await checkReleaseNotesDisabled();

        aboutBrowserProxy.setReleaseNotes(false);
        aboutBrowserProxy.setInternetConnection(true);
        await initNewPage();
        await checkReleaseNotesDisabled();

        aboutBrowserProxy.setReleaseNotes(true);
        aboutBrowserProxy.setInternetConnection(false);
        await initNewPage();
        await checkReleaseNotesOnline(false);
        await checkReleaseNotesOffline(true);

        aboutBrowserProxy.setReleaseNotes(true);
        aboutBrowserProxy.setInternetConnection(true);
        await initNewPage();
        await checkReleaseNotesOnline(true);
        await checkReleaseNotesOffline(false);

        page.$$('#releaseNotesOnline').click();
        return aboutBrowserProxy.whenCalled('launchReleaseNotes');
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

      test('detailed build info page', async () => {
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
    });
  }

  function registerOfficialBuildTests() {
    suite('AboutPageTest_OfficialBuild', function() {
      test('ReportAnIssue', function() {
        const browserProxy = new TestAboutPageBrowserProxy();
        settings.AboutPageBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        const page = document.createElement('os-settings-about-page');
        document.body.appendChild(page);

        assertTrue(!!page.$.reportIssue);
        page.$.reportIssue.click();
        return browserProxy.whenCalled('openFeedbackDialog');
      });
    });
  }

  return {
    // TODO(crbug.com/950007): Move the channel switch dialog tests to here
    // from the browser about page tests when those CrOS-specific parts are
    // removed from the browser about page.
    registerTests: registerAboutPageTests,
    registerOfficialBuildTests: registerOfficialBuildTests,
  };
});
