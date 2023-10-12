// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://chrome-signin/edu_coexistence_app.js';

import {Screens} from 'chrome://chrome-signin/edu_coexistence_app.js';
import {EduCoexistenceBrowserProxyImpl} from 'chrome://chrome-signin/edu_coexistence_browser_proxy.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {getFakeAccountsNotAvailableInArcList, setTestArcAccountPickerBrowserProxy, TestArcAccountPickerBrowserProxy} from '../arc_account_picker/test_util.js';

import {TestEduCoexistenceBrowserProxy} from './edu_coexistence_test_browser_proxy.js';

suite('EduCoexistenceAppWithArcPickerTest', function() {
  let appComponent;
  let testBrowserProxy;
  let testArcBrowserProxy;

  async function waitForSwitchViewPromise() {
    return new Promise(
        resolve => appComponent.addEventListener(
            'switch-view-notify-for-testing', () => resolve()));
  }

  setup(function() {
    testBrowserProxy = new TestEduCoexistenceBrowserProxy();
    EduCoexistenceBrowserProxyImpl.setInstance(testBrowserProxy);
    testBrowserProxy.setDialogArguments(
        {isAvailableInArc: true, showArcAvailabilityPicker: true});
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

    testArcBrowserProxy = new TestArcAccountPickerBrowserProxy();
    testArcBrowserProxy.setAccountsNotAvailableInArc(
        getFakeAccountsNotAvailableInArcList());
    setTestArcAccountPickerBrowserProxy(testArcBrowserProxy);

    document.body.innerHTML = window.trustedTypes.emptyHTML;
    appComponent = document.createElement('edu-coexistence-app');
    document.body.appendChild(appComponent);
    flush();
  });

  test('ShowArcPicker', async function() {
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

  test('ArcPickerSwitchToNormalSignin', async function() {
    const switchViewPromise = waitForSwitchViewPromise();
    window.dispatchEvent(new Event('online'));
    await switchViewPromise;
    assertEquals(
        appComponent.getCurrentScreenForTest(), Screens.ARC_ACCOUNT_PICKER);

    const arcAccountPickerComponent =
        /** @type {ArcAccountPickerAppElement} */ (
            appComponent.shadowRoot.querySelector('arc-account-picker-app'));
    arcAccountPickerComponent.shadowRoot.querySelector('#addAccountButton')
        .click();
    assertEquals(appComponent.getCurrentScreenForTest(), Screens.ONLINE_FLOW);
  });
});
