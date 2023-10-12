// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://chrome-signin/edu_coexistence_app.js';

import {Screens} from 'chrome://chrome-signin/edu_coexistence_app.js';
import {EduCoexistenceBrowserProxyImpl} from 'chrome://chrome-signin/edu_coexistence_browser_proxy.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {TestEduCoexistenceBrowserProxy} from './edu_coexistence_test_browser_proxy.js';

suite('EduCoexistenceAppTest', function() {
  let appComponent;
  let testBrowserProxy;

  setup(function() {
    testBrowserProxy = new TestEduCoexistenceBrowserProxy();
    EduCoexistenceBrowserProxyImpl.setInstance(testBrowserProxy);
    testBrowserProxy.setDialogArguments(
        {isAvailableInArc: true, showArcAvailabilityPicker: false});
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

    document.body.innerHTML = window.trustedTypes.emptyHTML;
    appComponent = document.createElement('edu-coexistence-app');
    document.body.appendChild(appComponent);
    flush();
  });

  test('InitOnline', function() {
    window.dispatchEvent(new Event('online'));
    assertEquals(appComponent.getCurrentScreenForTest(), Screens.ONLINE_FLOW);
  });

  test('InitOffline', function() {
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

  test('ShowOffline', function() {
    window.dispatchEvent(new Event('online'));
    assertEquals(appComponent.getCurrentScreenForTest(), Screens.ONLINE_FLOW);

    window.dispatchEvent(new Event('offline'));
    assertEquals(appComponent.getCurrentScreenForTest(), Screens.OFFLINE);
  });

  test('ShowOnline', function() {
    window.dispatchEvent(new Event('offline'));
    assertEquals(appComponent.getCurrentScreenForTest(), Screens.OFFLINE);

    window.dispatchEvent(new Event('online'));
    assertEquals(appComponent.getCurrentScreenForTest(), Screens.ONLINE_FLOW);
  });

  test('ShowError', function() {
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

  test('DontSwitchViewIfDisplayingError', function() {
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
      'ShowErrorScreenImmediatelyOnLoadAbort', function() {
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
