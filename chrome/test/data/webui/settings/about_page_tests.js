// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_about_page', function() {
  /** @implements {settings.AboutPageBrowserProxy} */
  class TestAboutPageBrowserProxy extends TestBrowserProxy {
    constructor() {
      const methodNames = [
        'pageReady',
        'refreshUpdateStatus',
        'openHelpPage',
        'openFeedbackDialog',
      ];

      if (cr.isChromeOS) {
        methodNames.push(
            'getChannelInfo', 'getVersionInfo', 'getRegulatoryInfo',
            'getHasEndOfLife', 'refreshTPMFirmwareUpdateStatus', 'setChannel');
      }

      if (cr.isMac)
        methodNames.push('promoteUpdater');

      super(methodNames);

      /** @private {!UpdateStatus} */
      this.updateStatus_ = UpdateStatus.UPDATED;

      if (cr.isChromeOS) {
        /** @private {!VersionInfo} */
        this.versionInfo_ = {
          arcVersion: '',
          osFirmware: '',
          osVersion: '',
        };

        /** @private {!ChannelInfo} */
        this.channelInfo_ = {
          currentChannel: BrowserChannel.BETA,
          targetChannel: BrowserChannel.BETA,
          canChangeChannel: true,
        };

        /** @private {?RegulatoryInfo} */
        this.regulatoryInfo_ = null;

        /** @private {!TPMFirmwareUpdateStatus} */
        this.tpmFirmwareUpdateStatus_ = {
          updateAvailable: false,
        };

        /** @private {boolean|Promise} */
        this.hasEndOfLife_ = false;
      }
    }

    /** @param {!UpdateStatus} updateStatus */
    setUpdateStatus(updateStatus) {
      this.updateStatus_ = updateStatus;
    }

    sendStatusNoInternet() {
      cr.webUIListenerCallback('update-status-changed', {
        progress: 0,
        status: UpdateStatus.FAILED,
        message: 'offline',
        connectionTypes: 'no internet',
      });
    }

    /** @override */
    pageReady() {
      this.methodCalled('pageReady');
    }

    /** @override */
    refreshUpdateStatus() {
      cr.webUIListenerCallback('update-status-changed', {
        progress: 1,
        status: this.updateStatus_,
      });
      this.methodCalled('refreshUpdateStatus');
    }

    /** @override */
    openFeedbackDialog() {
      this.methodCalled('openFeedbackDialog');
    }

    /** @override */
    openHelpPage() {
      this.methodCalled('openHelpPage');
    }
  }

  if (cr.isMac) {
    /** @override */
    TestAboutPageBrowserProxy.prototype.promoteUpdater = function() {
      this.methodCalled('promoteUpdater');
    };
  }

  if (cr.isChromeOS) {
    /** @param {!VersionInfo} */
    TestAboutPageBrowserProxy.prototype.setVersionInfo = function(versionInfo) {
      this.versionInfo_ = versionInfo;
    };

    /** @param {boolean} canChangeChannel */
    TestAboutPageBrowserProxy.prototype.setCanChangeChannel = function(
        canChangeChannel) {
      this.channelInfo_.canChangeChannel = canChangeChannel;
    };

    /**
     * @param {!BrowserChannel} current
     * @param {!BrowserChannel} target
     */
    TestAboutPageBrowserProxy.prototype.setChannels = function(
        current, target) {
      this.channelInfo_.currentChannel = current;
      this.channelInfo_.targetChannel = target;
    };

    /** @param {?RegulatoryInfo} regulatoryInfo */
    TestAboutPageBrowserProxy.prototype.setRegulatoryInfo = function(
        regulatoryInfo) {
      this.regulatoryInfo_ = regulatoryInfo;
    };

    /** @param {boolean|Promise} hasEndOfLife */
    TestAboutPageBrowserProxy.prototype.setHasEndOfLife = function(
        hasEndOfLife) {
      this.hasEndOfLife_ = hasEndOfLife;
    };

    /** @override */
    TestAboutPageBrowserProxy.prototype.getChannelInfo = function() {
      this.methodCalled('getChannelInfo');
      return Promise.resolve(this.channelInfo_);
    };

    /** @override */
    TestAboutPageBrowserProxy.prototype.getVersionInfo = function() {
      this.methodCalled('getVersionInfo');
      return Promise.resolve(this.versionInfo_);
    };

    /** @override */
    TestAboutPageBrowserProxy.prototype.getRegulatoryInfo = function() {
      this.methodCalled('getRegulatoryInfo');
      return Promise.resolve(this.regulatoryInfo_);
    };

    /** @override */
    TestAboutPageBrowserProxy.prototype.getHasEndOfLife = function() {
      this.methodCalled('getHasEndOfLife');
      return Promise.resolve(this.hasEndOfLife_);
    };

    /** @override */
    TestAboutPageBrowserProxy.prototype.setChannel = function(
        channel, isPowerwashAllowed) {
      this.methodCalled('setChannel', [channel, isPowerwashAllowed]);
    };

    /** @param {!TPMFirmwareUpdateStatus} status */
    TestAboutPageBrowserProxy.prototype.setTPMFirmwareUpdateStatus = function(
        status) {
      this.tpmFirmwareUpdateStatus_ = status;
    };

    /** @override */
    TestAboutPageBrowserProxy.prototype.refreshTPMFirmwareUpdateStatus =
        function() {
      this.methodCalled('refreshTPMFirmwareUpdateStatus');
      cr.webUIListenerCallback(
          'tpm-firmware-update-status-changed', this.tpmFirmwareUpdateStatus_);
    };
  }


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
        loadTimeData.overrideValues({
          aboutObsoleteNowOrSoon: false,
          aboutObsoleteEndOfTheLine: false,
        });
      });

      /** @return {!Promise} */
      function initNewPage() {
        aboutBrowserProxy.reset();
        lifetimeBrowserProxy.reset();
        PolymerTest.clearBody();
        page = document.createElement('settings-about-page');
        settings.navigateTo(settings.routes.ABOUT);
        document.body.appendChild(page);
        if (!cr.isChromeOS) {
          return aboutBrowserProxy.whenCalled('refreshUpdateStatus');
        } else {
          return Promise.all([
            aboutBrowserProxy.whenCalled('getChannelInfo'),
            aboutBrowserProxy.whenCalled('refreshUpdateStatus'),
            aboutBrowserProxy.whenCalled('refreshTPMFirmwareUpdateStatus'),
          ]);
        }
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

      if (!cr.isChromeOS) {
        /**
         * Test that when the current platform has been marked as deprecated
         * (but not end of the line) a deprecation warning message is displayed,
         * without interfering with the update status message and icon.
         */
        test('ObsoleteSystem', function() {
          loadTimeData.overrideValues({
            aboutObsoleteNowOrSoon: true,
            aboutObsoleteEndOfTheLine: false,
          });

          return initNewPage().then(function() {
            const icon = page.$$('iron-icon');
            assertTrue(!!icon);
            assertTrue(!!page.$.updateStatusMessage);
            assertTrue(!!page.$.deprecationWarning);
            assertFalse(page.$.deprecationWarning.hidden);

            fireStatusChanged(UpdateStatus.CHECKING);
            assertEquals(SPINNER_ICON, icon.src);
            assertEquals(null, icon.getAttribute('icon'));
            assertFalse(page.$.deprecationWarning.hidden);
            assertFalse(page.$.updateStatusMessage.hidden);

            fireStatusChanged(UpdateStatus.UPDATING);
            assertEquals(SPINNER_ICON, icon.src);
            assertEquals(null, icon.getAttribute('icon'));
            assertFalse(page.$.deprecationWarning.hidden);
            assertFalse(page.$.updateStatusMessage.hidden);

            fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
            assertEquals(null, icon.src);
            assertEquals('settings:check-circle', icon.icon);
            assertFalse(page.$.deprecationWarning.hidden);
            assertFalse(page.$.updateStatusMessage.hidden);
          });
        });

        /**
         * Test that when the current platform has reached the end of the line,
         * a deprecation warning message and an error icon is displayed.
         */
        test('ObsoleteSystemEndOfLine', function() {
          loadTimeData.overrideValues({
            aboutObsoleteNowOrSoon: true,
            aboutObsoleteEndOfTheLine: true,
          });
          return initNewPage().then(function() {
            const icon = page.$$('iron-icon');
            assertTrue(!!icon);
            assertTrue(!!page.$.deprecationWarning);
            assertTrue(!!page.$.updateStatusMessage);

            assertFalse(page.$.deprecationWarning.hidden);
            assertFalse(page.$.deprecationWarning.hidden);

            fireStatusChanged(UpdateStatus.CHECKING);
            assertEquals(null, icon.src);
            assertEquals('cr:error', icon.icon);
            assertFalse(page.$.deprecationWarning.hidden);
            assertTrue(page.$.updateStatusMessage.hidden);

            fireStatusChanged(UpdateStatus.FAILED);
            assertEquals(null, icon.src);
            assertEquals('cr:error', icon.icon);
            assertFalse(page.$.deprecationWarning.hidden);
            assertTrue(page.$.updateStatusMessage.hidden);

            fireStatusChanged(UpdateStatus.UPDATED);
            assertEquals(null, icon.src);
            assertEquals('cr:error', icon.icon);
            assertFalse(page.$.deprecationWarning.hidden);
            assertTrue(page.$.updateStatusMessage.hidden);
          });
        });
      }

      test('Relaunch', function() {
        let relaunch = page.$.relaunch;
        assertTrue(!!relaunch);
        assertTrue(relaunch.hidden);

        fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
        assertFalse(relaunch.hidden);

        relaunch = page.$.relaunch;
        assertTrue(!!relaunch);
        relaunch.click();
        return lifetimeBrowserProxy.whenCalled('relaunch');
      });

      if (cr.isChromeOS) {
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
          const relaunch = page.$.relaunch;
          const checkForUpdates = page.$.checkForUpdates;
          const relaunchAndPowerwash = page.$.relaunchAndPowerwash;

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
        test('ButtonsUpdate_BetaToStable', function() {
          aboutBrowserProxy.setChannels(
              BrowserChannel.BETA, BrowserChannel.STABLE);
          aboutBrowserProxy.setUpdateStatus(UpdateStatus.NEARLY_UPDATED);

          return initNewPage().then(function() {
            assertTrue(!!page.$.relaunch);
            assertTrue(!!page.$.relaunchAndPowerwash);

            assertTrue(page.$.relaunch.hidden);
            assertFalse(page.$.relaunchAndPowerwash.hidden);

            page.$.relaunchAndPowerwash.click();
            return lifetimeBrowserProxy.whenCalled('factoryReset')
                .then((requestTpmFirmwareUpdate) => {
                  assertFalse(requestTpmFirmwareUpdate);
                });
          });
        });

        /**
         * Test that buttons update according to incoming
         * 'update-status-changed' events for the case where the target channel
         * is less stable than current channel.
         */
        test('ButtonsUpdate_StableToBeta', function() {
          aboutBrowserProxy.setChannels(
              BrowserChannel.STABLE, BrowserChannel.BETA);
          aboutBrowserProxy.setUpdateStatus(UpdateStatus.NEARLY_UPDATED);

          return initNewPage().then(function() {
            assertTrue(!!page.$.relaunch);
            assertTrue(!!page.$.relaunchAndPowerwash);

            assertFalse(page.$.relaunch.hidden);
            assertTrue(page.$.relaunchAndPowerwash.hidden);

            page.$.relaunch.click();
            return lifetimeBrowserProxy.whenCalled('relaunch');
          });
        });

        /**
         * Test that buttons update as a result of receiving a
         * 'target-channel-changed' event (normally fired from
         * <settings-channel-switcher-dialog>).
         */
        test('ButtonsUpdate_TargetChannelChangedEvent', function() {
          aboutBrowserProxy.setChannels(
              BrowserChannel.BETA, BrowserChannel.BETA);
          aboutBrowserProxy.setUpdateStatus(UpdateStatus.NEARLY_UPDATED);

          return initNewPage().then(function() {
            assertFalse(page.$.relaunch.hidden);
            assertTrue(page.$.relaunchAndPowerwash.hidden);

            page.fire('target-channel-changed', BrowserChannel.DEV);
            assertFalse(page.$.relaunch.hidden);
            assertTrue(page.$.relaunchAndPowerwash.hidden);

            page.fire('target-channel-changed', BrowserChannel.STABLE);
            assertTrue(page.$.relaunch.hidden);
            assertFalse(page.$.relaunchAndPowerwash.hidden);
          });
        });

        test('RegulatoryInfo', function() {
          let regulatoryInfo = null;

          /**
           * Checks the visibility of the "regulatory info" section.
           * @param {boolean} isShowing Whether the section is expected to be
           *     visible.
           * @return {!Promise}
           */
          function checkRegulatoryInfo(isShowing) {
            return aboutBrowserProxy.whenCalled('getRegulatoryInfo')
                .then(function() {
                  const regulatoryInfoEl = page.$.regulatoryInfo;
                  assertTrue(!!regulatoryInfoEl);
                  assertEquals(isShowing, !regulatoryInfoEl.hidden);

                  if (isShowing) {
                    const img = regulatoryInfoEl.querySelector('img');
                    assertTrue(!!img);
                    assertEquals(regulatoryInfo.text, img.getAttribute('alt'));
                    assertEquals(regulatoryInfo.url, img.getAttribute('src'));
                  }
                });
          }

          return checkRegulatoryInfo(false)
              .then(function() {
                regulatoryInfo = {text: 'foo', url: 'bar'};
                aboutBrowserProxy.setRegulatoryInfo(regulatoryInfo);
                return initNewPage();
              })
              .then(function() {
                return checkRegulatoryInfo(true);
              });
        });

        test('TPMFirmwareUpdate', function() {
          return initNewPage()
              .then(function() {
                assertTrue(page.$.aboutTPMFirmwareUpdate.hidden);
                aboutBrowserProxy.setTPMFirmwareUpdateStatus(
                    {updateAvailable: true});
                aboutBrowserProxy.refreshTPMFirmwareUpdateStatus();
              })
              .then(function() {
                assertFalse(page.$.aboutTPMFirmwareUpdate.hidden);
                page.$.aboutTPMFirmwareUpdate.click();
              })
              .then(function() {
                const dialog = page.$$('settings-powerwash-dialog');
                assertTrue(!!dialog);
                assertTrue(dialog.$.dialog.open);
                dialog.$$('#powerwash').click();
                return lifetimeBrowserProxy.whenCalled('factoryReset')
                    .then((requestTpmFirmwareUpdate) => {
                      assertTrue(requestTpmFirmwareUpdate);
                    });
              });
        });

        test('DeviceEndOfLife', function() {
          /**
           * Checks the visibility of the end of life message and icon.
           * @param {boolean} isShowing Whether the end of life UI is expected
           *     to be visible.
           * @return {!Promise}
           */
          function checkHasEndOfLife(isShowing) {
            return aboutBrowserProxy.whenCalled('getHasEndOfLife')
                .then(function() {
                  const endOfLifeMessageContainer =
                      page.$.endOfLifeMessageContainer;
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
                    assertEquals('settings:end-of-life', icon.icon);

                    const checkForUpdates = page.$.checkForUpdates;
                    assertTrue(!!checkForUpdates);
                    assertTrue(checkForUpdates.hidden);
                  }
                });
          }

          // Force test proxy to not respond to JS requests.
          // End of life message should still be hidden in this case.
          aboutBrowserProxy.setHasEndOfLife(new Promise(function(res, rej) {}));
          return initNewPage()
              .then(function() {
                return checkHasEndOfLife(false);
              })
              .then(function() {
                aboutBrowserProxy.setHasEndOfLife(true);
                return initNewPage();
              })
              .then(function() {
                return checkHasEndOfLife(true);
              })
              .then(function() {
                aboutBrowserProxy.setHasEndOfLife(false);
                return initNewPage();
              })
              .then(function() {
                return checkHasEndOfLife(false);
              });
        });
      }

      if (!cr.isChromeOS) {
        /*
         * Test that the "Relaunch" button updates according to incoming
         * 'update-status-changed' events.
         */
        test('ButtonsUpdate', function() {
          const relaunch = page.$.relaunch;
          assertTrue(!!relaunch);

          fireStatusChanged(UpdateStatus.CHECKING);
          assertTrue(relaunch.hidden);

          fireStatusChanged(UpdateStatus.UPDATING);
          assertTrue(relaunch.hidden);

          fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
          assertFalse(relaunch.hidden);

          fireStatusChanged(UpdateStatus.UPDATED);
          assertTrue(relaunch.hidden);

          fireStatusChanged(UpdateStatus.FAILED);
          assertTrue(relaunch.hidden);

          fireStatusChanged(UpdateStatus.DISABLED);
          assertTrue(relaunch.hidden);

          fireStatusChanged(UpdateStatus.DISABLED_BY_ADMIN);
          assertTrue(relaunch.hidden);
        });
      }

      test('GetHelp', function() {
        assertTrue(!!page.$.help);
        page.$.help.click();
        return aboutBrowserProxy.whenCalled('openHelpPage');
      });
    });
  }

  function registerOfficialBuildTests() {
    suite('AboutPageTest_OfficialBuild', function() {
      let page = null;
      let browserProxy = null;

      setup(function() {
        browserProxy = new TestAboutPageBrowserProxy();
        settings.AboutPageBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        page = document.createElement('settings-about-page');
        document.body.appendChild(page);
      });

      test('ReportAnIssue', function() {
        assertTrue(!!page.$.reportIssue);
        page.$.reportIssue.click();
        return browserProxy.whenCalled('openFeedbackDialog');
      });

      if (cr.isMac) {
        /**
         * A list of possible scenarios for the promoteUpdater.
         * @enum {!PromoteUpdaterStatus}
         */
        const PromoStatusScenarios = {
          CANT_PROMOTE: {
            hidden: true,
            disabled: true,
            actionable: false,
          },
          CAN_PROMOTE: {
            hidden: false,
            disabled: false,
            actionable: true,
          },
          IN_BETWEEN: {
            hidden: false,
            disabled: true,
            actionable: true,
          },
          PROMOTED: {
            hidden: false,
            disabled: true,
            actionable: false,
          },
        };

        /**
         * @param {!PromoteUpdaterStatus} status
         */
        function firePromoteUpdaterStatusChanged(status) {
          cr.webUIListenerCallback('promotion-state-changed', status);
        }

        /**
         * Tests that the button's states are wired up to the status correctly.
         */
        test('PromoteUpdaterButtonCorrectStates', function() {
          let item = page.$$('#promoteUpdater');
          let arrow = page.$$('#promoteUpdater button');
          assertFalse(!!item);
          assertFalse(!!arrow);

          firePromoteUpdaterStatusChanged(PromoStatusScenarios.CANT_PROMOTE);
          Polymer.dom.flush();
          item = page.$$('#promoteUpdater');
          arrow = page.$$('#promoteUpdater button');
          assertFalse(!!item);
          assertFalse(!!arrow);

          firePromoteUpdaterStatusChanged(PromoStatusScenarios.CAN_PROMOTE);
          Polymer.dom.flush();

          item = page.$$('#promoteUpdater');
          assertTrue(!!item);
          assertFalse(item.hasAttribute('disabled'));
          assertTrue(item.hasAttribute('actionable'));

          arrow = page.$$('#promoteUpdater button');
          assertTrue(!!arrow);
          assertEquals('PAPER-ICON-BUTTON-LIGHT', arrow.parentElement.tagName);
          assertFalse(arrow.parentElement.hidden);
          assertFalse(arrow.hasAttribute('disabled'));

          firePromoteUpdaterStatusChanged(PromoStatusScenarios.IN_BETWEEN);
          Polymer.dom.flush();
          item = page.$$('#promoteUpdater');
          assertTrue(!!item);
          assertTrue(item.hasAttribute('disabled'));
          assertTrue(item.hasAttribute('actionable'));

          arrow = page.$$('#promoteUpdater button');
          assertTrue(!!arrow);
          assertEquals('PAPER-ICON-BUTTON-LIGHT', arrow.parentElement.tagName);
          assertFalse(arrow.parentElement.hidden);
          assertTrue(arrow.hasAttribute('disabled'));

          firePromoteUpdaterStatusChanged(PromoStatusScenarios.PROMOTED);
          Polymer.dom.flush();
          item = page.$$('#promoteUpdater');
          assertTrue(!!item);
          assertTrue(item.hasAttribute('disabled'));
          assertFalse(item.hasAttribute('actionable'));

          arrow = page.$$('#promoteUpdater button');
          assertTrue(!!arrow);
          assertEquals('PAPER-ICON-BUTTON-LIGHT', arrow.parentElement.tagName);
          assertTrue(arrow.parentElement.hidden);
          assertTrue(arrow.hasAttribute('disabled'));
        });

        test('PromoteUpdaterButtonWorksWhenEnabled', function() {
          firePromoteUpdaterStatusChanged(PromoStatusScenarios.CAN_PROMOTE);
          Polymer.dom.flush();
          const item = page.$$('#promoteUpdater');
          assertTrue(!!item);

          item.click();

          return browserProxy.whenCalled('promoteUpdater');
        });
      }
    });
  }

  if (cr.isChromeOS) {
    function registerDetailedBuildInfoTests() {
      suite('DetailedBuildInfoTest', function() {
        let page = null;
        let browserProxy = null;

        setup(function() {
          browserProxy = new TestAboutPageBrowserProxy();
          settings.AboutPageBrowserProxyImpl.instance_ = browserProxy;
          PolymerTest.clearBody();
        });

        teardown(function() {
          page.remove();
          page = null;
        });

        test('Initialization', function() {
          const versionInfo = {
            arcVersion: 'dummyArcVersion',
            osFirmware: 'dummyOsFirmware',
            osVersion: 'dummyOsVersion',
          };
          browserProxy.setVersionInfo(versionInfo);

          page = document.createElement('settings-detailed-build-info');
          document.body.appendChild(page);

          return Promise
              .all([
                browserProxy.whenCalled('pageReady'),
                browserProxy.whenCalled('getVersionInfo'),
                browserProxy.whenCalled('getChannelInfo'),
              ])
              .then(function() {
                assertEquals(
                    versionInfo.arcVersion, page.$.arcVersion.textContent);
                assertEquals(
                    versionInfo.osVersion, page.$.osVersion.textContent);
                assertEquals(
                    versionInfo.osFirmware, page.$.osFirmware.textContent);
              });
        });

        /**
         * Checks whether the "change channel" button state (enabled/disabled)
         * correctly reflects whether the user is allowed to change channel (as
         * dictated by the browser via loadTimeData boolean).
         * @param {boolean} canChangeChannel Whether to simulate the case where
         *     changing channels is allowed.
         * @return {!Promise}
         */
        function checkChangeChannelButton(canChangeChannel) {
          browserProxy.setCanChangeChannel(canChangeChannel);
          page = document.createElement('settings-detailed-build-info');
          document.body.appendChild(page);
          return browserProxy.whenCalled('getChannelInfo').then(function() {
            const changeChannelButton = page.$$('paper-button');
            assertTrue(!!changeChannelButton);
            assertEquals(canChangeChannel, !changeChannelButton.disabled);
          });
        }

        test('ChangeChannel_Enabled', function() {
          return checkChangeChannelButton(true);
        });

        test('ChangeChannel_Disabled', function() {
          return checkChangeChannelButton(false);
        });
      });
    }

    function registerChannelSwitcherDialogTests() {
      suite('ChannelSwitcherDialogTest', function() {
        let dialog = null;
        let radioButtons = null;
        let browserProxy = null;
        const currentChannel = BrowserChannel.BETA;

        setup(function() {
          browserProxy = new TestAboutPageBrowserProxy();
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
        test('ChangeChannel_LessStable', function() {
          assertEquals(BrowserChannel.DEV, radioButtons.item(2).name);
          radioButtons.item(2).click();
          Polymer.dom.flush();

          return browserProxy.whenCalled('getChannelInfo').then(function() {
            assertEquals(dialog.$.warningSelector.selected, 2);
            // Check that only the "Change channel" button becomes visible.
            assertTrue(dialog.$.changeChannelAndPowerwash.hidden);
            assertFalse(dialog.$.changeChannel.hidden);

            const whenTargetChannelChangedFired =
                test_util.eventToPromise('target-channel-changed', dialog);

            dialog.$.changeChannel.click();
            return browserProxy.whenCalled('setChannel')
                .then(function(args) {
                  assertEquals(BrowserChannel.DEV, args[0]);
                  assertFalse(args[1]);
                  return whenTargetChannelChangedFired;
                })
                .then(function(event) {
                  assertEquals(BrowserChannel.DEV, event.detail);
                });
          });
        });

        // Test case where user switches to a more stable channel.
        test('ChangeChannel_MoreStable', function() {
          assertEquals(BrowserChannel.STABLE, radioButtons.item(0).name);
          radioButtons.item(0).click();
          Polymer.dom.flush();

          return browserProxy.whenCalled('getChannelInfo').then(function() {
            assertEquals(dialog.$.warningSelector.selected, 1);
            // Check that only the "Change channel and Powerwash" button becomes
            // visible.
            assertFalse(dialog.$.changeChannelAndPowerwash.hidden);
            assertTrue(dialog.$.changeChannel.hidden);

            const whenTargetChannelChangedFired =
                test_util.eventToPromise('target-channel-changed', dialog);

            dialog.$.changeChannelAndPowerwash.click();
            return browserProxy.whenCalled('setChannel')
                .then(function(args) {
                  assertEquals(BrowserChannel.STABLE, args[0]);
                  assertTrue(args[1]);
                  return whenTargetChannelChangedFired;
                })
                .then(function(event) {
                  assertEquals(BrowserChannel.STABLE, event.detail);
                });
          });
        });
      });
    }
  }

  return {
    registerTests: function() {
      if (cr.isChromeOS) {
        registerDetailedBuildInfoTests();
        registerChannelSwitcherDialogTests();
      }
      registerAboutPageTests();
    },
    registerOfficialBuildTests: registerOfficialBuildTests,
  };
});
