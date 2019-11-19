// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of integration tests for MultiDevice setup WebUI. */
cr.define('multidevice_setup', () => {
  /** @implements {multidevice_setup.MultiDeviceSetupDelegate} */
  class FakeDelegate {
    constructor() {
      /** @private {boolean} */
      this.isPasswordRequiredToSetHost_ = true;

      /** @private {boolean} */
      this.shouldSetHostSucceed_ = true;

      this.numSetHostDeviceCalls = 0;
    }

    set isPasswordRequired(isPasswordRequired) {
      this.isPasswordRequiredToSetHost_ = isPasswordRequired;
    }

    /** @override */
    isPasswordRequiredToSetHost() {
      return this.isPasswordRequiredToSetHost_;
    }

    set shouldSetHostSucceed(shouldSetHostSucceed) {
      this.shouldSetHostSucceed_ = shouldSetHostSucceed;
    }

    /** @override */
    setHostDevice(hostDeviceId, opt_authToken) {
      return new Promise((resolve) => {
        this.numSetHostDeviceCalls++;
        resolve({success: this.shouldSetHostSucceed_});
      });
    }

    set shouldExitSetupFlowAfterHostSet(shouldExitSetupFlowAfterHostSet) {
      this.shouldExitSetupFlowAfterSettingHost_ =
          shouldExitSetupFlowAfterHostSet;
    }

    /** @override */
    shouldExitSetupFlowAfterSettingHost() {
      return this.shouldExitSetupFlowAfterSettingHost_;
    }

    /** @override */
    getStartSetupCancelButtonTextId() {
      return 'cancel';
    }
  }

  /** @implements {multidevice_setup.MojoInterfaceProvider} */
  class FakeMojoInterfaceProviderImpl {
    /** @param {!FakeMojoService} fakeMojoService */
    constructor(fakeMojoService) {
      /** @private {!FakeMojoService} */
      this.fakeMojoService_ = fakeMojoService;
    }

    /** @override */
    getInterfacePtr() {
      return this.fakeMojoService_;
    }
  }

  function registerIntegrationTests() {
    suite('MultiDeviceSetup', () => {
      /**
       * MultiDeviceSetup created before each test. Defined in setUp.
       * @type {MultiDeviceSetup|undefined}
       */
      let multiDeviceSetupElement;

      /**
       * Forward navigation button. Defined in setUp.
       * @type {PaperButton|undefined}
       */
      let forwardButton;

      /**
       * Cancel button. Defined in setUp.
       * @type {PaperButton|undefined}
       */
      let cancelButton;

      /**
       * Backward navigation button. Defined in setUp.
       * @type {PaperButton|undefined}
       */
      let backwardButton;

      /** @type {!FakeMojoService} */
      let fakeMojoService;

      /** @type {!settings.FakeQuickUnlockPrivate} */
      let fakeQuickUnlockPrivate;

      /** @type {?TestMultideviceSetupBrowserProxy} */
      let browserProxy = null;

      const PASSWORD = 'password-page';
      const SUCCESS = 'setup-succeeded-page';
      const START = 'start-setup-page';

      const CORRECT_PASSWORD = 'correctPassword';
      const WRONG_PASSWORD = 'wrongPassword';

      setup(() => {
        browserProxy = new TestMultideviceSetupBrowserProxy();
        multidevice_setup.BrowserProxyImpl.instance_ = browserProxy;

        multiDeviceSetupElement = document.createElement('multidevice-setup');
        multiDeviceSetupElement.delegate = new FakeDelegate();
        fakeMojoService = new FakeMojoService();
        multiDeviceSetupElement.mojoInterfaceProvider_ =
            new FakeMojoInterfaceProviderImpl(fakeMojoService);

        document.body.appendChild(multiDeviceSetupElement);
        Polymer.dom.flush();

        forwardButton = multiDeviceSetupElement.$$('button-bar').$$('#forward');
        cancelButton = multiDeviceSetupElement.$$('button-bar').$$('#cancel');
        backwardButton =
            multiDeviceSetupElement.$$('button-bar').$$('#backward');

        fakeQuickUnlockPrivate = new settings.FakeQuickUnlockPrivate();
        fakeQuickUnlockPrivate.accountPassword = CORRECT_PASSWORD;
        multiDeviceSetupElement.$$(PASSWORD).quickUnlockPrivate_ =
            fakeQuickUnlockPrivate;
      });

      /** @param {boolean} isOobeMode */
      function setMode(isOobeMode) {
        multiDeviceSetupElement.delegate.isPasswordRequired = !isOobeMode;
        multiDeviceSetupElement.delegate.shouldExitSetupFlowAfterHostSet =
            isOobeMode;
      }

      /**
       * @param {string} visiblePageName
       * @return {!Promise} Promise that resolves when the page renders.
       */
      function setVisiblePage(visiblePageName) {
        multiDeviceSetupElement.visiblePageName = visiblePageName;
        Polymer.dom.flush();
        return test_util.waitBeforeNextRender(
            multiDeviceSetupElement.$$(visiblePageName));
      }

      /**
       * @param {string} input
       * @return {!Promise} Promise that resolves when the page renders.
       */
      function enterPassword(input) {
        multiDeviceSetupElement.$$(PASSWORD).$$('#passwordInput').value = input;
        Polymer.dom.flush();
        return test_util.waitBeforeNextRender(multiDeviceSetupElement);
      }

      function getNumSetHostDeviceCalls() {
        return multiDeviceSetupElement.delegate.numSetHostDeviceCalls;
      }

      // *** From SetupSucceededPage ***

      test('SetupSucceededPage buttons: forward', () => {
        return setVisiblePage(SUCCESS).then(() => {
          assertFalse(forwardButton.hidden);
          assertTrue(cancelButton.hidden);
          assertTrue(backwardButton.hidden);
        });
      });

      test('SetupSucceededPage forward button closes UI', () => {
        return setVisiblePage(SUCCESS).then(() => {
          const whenSetupExits =
              test_util.eventToPromise('setup-exited', multiDeviceSetupElement);
          forwardButton.click();
          return whenSetupExits;
        });
      });

      // Post-OOBE

      test('SetupSucceededPage Settings link closes UI (post-OOBE)', () => {
        setMode(false /* isOobeMode */);
        return setVisiblePage(SUCCESS).then(() => {
          const whenSetupExits =
              test_util.eventToPromise('setup-exited', multiDeviceSetupElement);
          multiDeviceSetupElement.$$(SUCCESS).$$('#settings-link').click();
          return whenSetupExits;
        });
      });

      // *** From StartSetupPage ***

      test('StartSetupPage buttons: forward, cancel', () => {
        return setVisiblePage(START).then(() => {
          assertFalse(forwardButton.hidden);
          assertFalse(cancelButton.hidden);
          assertTrue(backwardButton.hidden);
        });
      });

      // OOBE

      test('StartSetupPage cancel button exits OOBE (OOBE)', () => {
        setMode(true /* isOobeMode */);

        return setVisiblePage(START)
            .then(() => {
              const whenSetupExits = test_util.eventToPromise(
                  'setup-exited', multiDeviceSetupElement);
              cancelButton.click();
              return whenSetupExits;
            })
            .then(() => {
              assertEquals(0, getNumSetHostDeviceCalls());
            });
      });

      test(
          'StartSetupPage forward button sets host in background and ' +
              'exits OOBE (OOBE)',
          () => {
            setMode(true /* isOobeMode */);

            return setVisiblePage(START)
                .then(() => {
                  multiDeviceSetupElement.delegate.shouldSetHostSucceed = true;
                  const whenSetupExits = test_util.eventToPromise(
                      'setup-exited', multiDeviceSetupElement);
                  forwardButton.click();
                  return whenSetupExits;
                })
                .then(() => {
                  assertEquals(1, getNumSetHostDeviceCalls());
                });
          });

      // Post-OOBE

      test('StartSetupPage cancel button closes UI (post-OOBE)', () => {
        setMode(false /* isOobeMode */);

        return setVisiblePage(START)
            .then(() => {
              const whenSetupExits = test_util.eventToPromise(
                  'setup-exited', multiDeviceSetupElement);
              cancelButton.click();
              return whenSetupExits;
            })
            .then(() => {
              assertEquals(0, getNumSetHostDeviceCalls());
            });
      });

      // *** From PasswordPage ***

      // Post-OOBE

      test(
          'PasswordPage buttons: forward, cancel, backward (post-OOBE)', () => {
            return setVisiblePage(PASSWORD).then(() => {
              assertFalse(forwardButton.hidden);
              assertFalse(cancelButton.hidden);
              assertFalse(backwardButton.hidden);
            });
          });

      test('PasswordPage cancel button closes UI (post-OOBE)', () => {
        setMode(false /* isOobeMode */);

        return setVisiblePage(PASSWORD)
            .then(() => {
              const whenSetupExits = test_util.eventToPromise(
                  'setup-exited', multiDeviceSetupElement);
              cancelButton.click();
              return whenSetupExits;
            })
            .then(() => {
              assertEquals(0, getNumSetHostDeviceCalls());
            });
      });

      test(
          'PasswordPage backward button goes to start page (post-OOBE)', () => {
            setMode(false /* isOobeMode */);

            return setVisiblePage(PASSWORD)
                .then(() => {
                  const whenPageChanges = test_util.eventToPromise(
                      'visible-page-name-changed', multiDeviceSetupElement);
                  backwardButton.click();
                  return whenPageChanges;
                })
                .then(() => {
                  assertEquals(START, multiDeviceSetupElement.visiblePageName);
                  assertEquals(0, getNumSetHostDeviceCalls());
                });
          });

      test(
          'PasswordPage forward button goes to success page if mojo works ' +
              '(post-OOBE)',
          () => {
            setMode(false /* isOobeMode */);

            return setVisiblePage(PASSWORD)
                .then(() => {
                  return enterPassword(CORRECT_PASSWORD);
                })
                .then(() => {
                  multiDeviceSetupElement.delegate.shouldSetHostSucceed = true;
                  const whenPageChanges = test_util.eventToPromise(
                      'visible-page-name-changed', multiDeviceSetupElement);
                  forwardButton.click();
                  return whenPageChanges;
                })
                .then(() => {
                  assertEquals(
                      SUCCESS, multiDeviceSetupElement.visiblePageName);
                  assertEquals(1, getNumSetHostDeviceCalls());
                });
          });

      test(
          'PasswordPage forward button does nothing if invalid password ' +
              '(post-OOBE)',
          () => {
            setMode(false /* isOobeMode */);

            return setVisiblePage(PASSWORD)
                .then(() => {
                  return enterPassword(WRONG_PASSWORD);
                })
                .then(() => {
                  multiDeviceSetupElement.delegate.shouldSetHostSucceed = true;
                  forwardButton.click();
                  Polymer.dom.flush();
                  return test_util.waitBeforeNextRender(
                      multiDeviceSetupElement);
                })
                .then(() => {
                  assertEquals(
                      PASSWORD, multiDeviceSetupElement.visiblePageName);
                  assertEquals(0, getNumSetHostDeviceCalls());
                });
          });

      test(
          'PasswordPage forward button is disabled if invalid password ' +
              '(post-OOBE)',
          () => {
            const whenMultiDeviceSetupLoads = setMode(false /* isOobeMode */);

            return setVisiblePage(PASSWORD)
                .then(() => {
                  return enterPassword(WRONG_PASSWORD);
                })
                .then(() => {
                  forwardButton.click();
                  Polymer.dom.flush();
                  return test_util.waitBeforeNextRender(
                      multiDeviceSetupElement);
                })
                .then(() => {
                  assertTrue(multiDeviceSetupElement.forwardButtonDisabled);
                });
          });
    });
  }
  return {registerIntegrationTests: registerIntegrationTests};
});
