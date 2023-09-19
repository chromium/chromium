// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://chrome-signin/edu_coexistence_app.js';

import {Screens} from 'chrome://chrome-signin/edu_coexistence_app.js';
import {EduCoexistenceBrowserProxyImpl} from 'chrome://chrome-signin/edu_coexistence_browser_proxy.js';
import {AuthMode, AuthParams} from 'chrome://chrome-signin/gaia_auth_host/authenticator.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getFakeAccountsNotAvailableInArcList, setTestArcAccountPickerBrowserProxy, TestArcAccountPickerBrowserProxy} from '../arc_account_picker/test_util.js';

import {getFakeAccountsList, TestEduCoexistenceBrowserProxy} from './edu_coexistence_test_util.js';

window.edu_coexistence_app_tests = {};
edu_coexistence_app_tests.suiteName = 'EduCoexistenceAppTest';

/** @enum {string} */
edu_coexistence_app_tests.TestNames = {
  InitOnline: 'Init in the online state',
  InitOffline: 'Init in the offline state',
  ShowOffline: 'Show offline',
  ShowOnline: 'Show online',
  ShowError: 'Show error',
  DontSwitchViewIfDisplayingError: 'No view switch after error',
  ShowErrorScreenImmediatelyOnLoadAbort:
      'Show error screen immediately on loadabort in webview',
  ShowArcPicker: 'ShowArcPicker',
  ArcPickerSwitchToNormalSignin: 'ArcPickerSwitchToNormalSignin',
};

suite(edu_coexistence_app_tests.suiteName, function() {
  let appComponent;
  let testBrowserProxy;
  let testArcBrowserProxy;

  /**
   * @param {?AccountAdditionOptions} dialogArgs
   */
  function setupWithParams(dialogArgs) {
    testBrowserProxy = new TestEduCoexistenceBrowserProxy();
    EduCoexistenceBrowserProxyImpl.setInstance(testBrowserProxy);
    testBrowserProxy.setDialogArguments(dialogArgs);
    testBrowserProxy.setInitializeEduArgsResponse(async function() {
      return {
        url: 'https://foo.example.com/supervision/coexistence/intro',
        hl: 'en-US',
        sourceUi: 'oobe',
        clientId: 'test-client-id',
        clientVersion: ' test-client-version',
        eduCoexistenceId: ' test-edu-coexistence-id',
        platformVersion: ' test-platform-version',
        releaseChannel: 'test-release-channel',
        deviceId: 'test-device-id',
      };
    });

    if (loadTimeData.getBoolean('isArcAccountRestrictionsEnabled')) {
      testArcBrowserProxy = new TestArcAccountPickerBrowserProxy();
      testArcBrowserProxy.setAccountsNotAvailableInArc(
          getFakeAccountsNotAvailableInArcList());
      setTestArcAccountPickerBrowserProxy(testArcBrowserProxy);
    }

    document.body.innerHTML = window.trustedTypes.emptyHTML;
    appComponent = document.createElement('edu-coexistence-app');
    document.body.appendChild(appComponent);
    flush();
  }

  async function waitForSwitchViewPromise() {
    return new Promise(
        resolve => appComponent.addEventListener(
            'switch-view-notify-for-testing', () => resolve()));
  }

  setup(function() {
    setupWithParams({isAvailableInArc: true, showArcAvailabilityPicker: false});
  });

  test(assert(edu_coexistence_app_tests.TestNames.InitOnline), function() {
    window.dispatchEvent(new Event('online'));
    assertEquals(appComponent.getCurrentScreenForTest(), Screens.ONLINE_FLOW);
  });

  test(assert(edu_coexistence_app_tests.TestNames.InitOffline), function() {
    window.dispatchEvent(new Event('offline'));
    assertEquals(appComponent.getCurrentScreenForTest(), Screens.OFFLINE);

    const offlineScreen =
        appComponent.shadowRoot.querySelector('edu-coexistence-offline');
    const nextButton =
        offlineScreen.shadowRoot.querySelector('edu-coexistence-button')
            .shadowRoot.querySelector('cr-button');
    nextButton.click();

    assertEquals(testBrowserProxy.getCallCount('dialogClose'), 1);
  });

  test(assert(edu_coexistence_app_tests.TestNames.ShowOffline), function() {
    window.dispatchEvent(new Event('online'));
    assertEquals(appComponent.getCurrentScreenForTest(), Screens.ONLINE_FLOW);

    window.dispatchEvent(new Event('offline'));
    assertEquals(appComponent.getCurrentScreenForTest(), Screens.OFFLINE);
  });

  test(assert(edu_coexistence_app_tests.TestNames.ShowOnline), function() {
    window.dispatchEvent(new Event('offline'));
    assertEquals(appComponent.getCurrentScreenForTest(), Screens.OFFLINE);

    window.dispatchEvent(new Event('online'));
    assertEquals(appComponent.getCurrentScreenForTest(), Screens.ONLINE_FLOW);
  });

  test(assert(edu_coexistence_app_tests.TestNames.ShowError), function() {
    window.dispatchEvent(new Event('online'));
    assertEquals(appComponent.getCurrentScreenForTest(), Screens.ONLINE_FLOW);

    appComponent.dispatchEvent(new CustomEvent('go-error'));
    assertEquals(appComponent.getCurrentScreenForTest(), Screens.ERROR);

    const errorScreen =
        appComponent.shadowRoot.querySelector('edu-coexistence-error');
    const nextButton =
        errorScreen.shadowRoot.querySelector('edu-coexistence-button')
            .shadowRoot.querySelector('cr-button');
    nextButton.click();

    assertEquals(testBrowserProxy.getCallCount('dialogClose'), 1);
  });

  test(
      assert(edu_coexistence_app_tests.TestNames.ShowArcPicker),
      async function() {
        setupWithParams(
            {isAvailableInArc: true, showArcAvailabilityPicker: true});
        const switchViewPromise = waitForSwitchViewPromise();
        window.dispatchEvent(new Event('online'));
        await switchViewPromise;
        assertEquals(
            appComponent.getCurrentScreenForTest(), Screens.ARC_ACCOUNT_PICKER);

        window.dispatchEvent(new Event('offline'));
        assertEquals(
            appComponent.getCurrentScreenForTest(), Screens.ARC_ACCOUNT_PICKER);

        window.dispatchEvent(new Event('online'));
        assertEquals(
            appComponent.getCurrentScreenForTest(), Screens.ARC_ACCOUNT_PICKER);

        appComponent.dispatchEvent(new CustomEvent('go-error'));
        assertEquals(appComponent.getCurrentScreenForTest(), Screens.ERROR);
      });

  test(
      assert(edu_coexistence_app_tests.TestNames.ArcPickerSwitchToNormalSignin),
      async function() {
        setupWithParams(
            {isAvailableInArc: true, showArcAvailabilityPicker: true});
        const switchViewPromise = waitForSwitchViewPromise();
        window.dispatchEvent(new Event('online'));
        await switchViewPromise;
        assertEquals(
            appComponent.getCurrentScreenForTest(), Screens.ARC_ACCOUNT_PICKER);

        const arcAccountPickerComponent =
            /** @type {ArcAccountPickerAppElement} */ (
                appComponent.shadowRoot.querySelector(
                    'arc-account-picker-app'));
        arcAccountPickerComponent.shadowRoot.querySelector('#addAccountButton')
            .click();
        assertEquals(
            appComponent.getCurrentScreenForTest(), Screens.ONLINE_FLOW);
      });

  test(
      assert(
          edu_coexistence_app_tests.TestNames.DontSwitchViewIfDisplayingError),
      function() {
        appComponent.dispatchEvent(new CustomEvent('go-error'));
        assertEquals(appComponent.getCurrentScreenForTest(), Screens.ERROR);

        window.dispatchEvent(new Event('offline'));
        // Should still show error screen.
        assertEquals(appComponent.getCurrentScreenForTest(), Screens.ERROR);

        window.dispatchEvent(new Event('online'));
        // Should still show error screen.
        assertEquals(appComponent.getCurrentScreenForTest(), Screens.ERROR);
      });

  test(
      assert(edu_coexistence_app_tests.TestNames
                 .ShowErrorScreenImmediatelyOnLoadAbort),
      function() {
        assertEquals(
            appComponent.getCurrentScreenForTest(), Screens.ONLINE_FLOW);
        const coexistenceUi =
            appComponent.shadowRoot.querySelector('edu-coexistence-ui');
        coexistenceUi.shadowRoot.querySelector('webview').dispatchEvent(
            new CustomEvent('loadabort'));

        // Should show error screen.
        assertEquals(appComponent.getCurrentScreenForTest(), Screens.ERROR);
      });
});
