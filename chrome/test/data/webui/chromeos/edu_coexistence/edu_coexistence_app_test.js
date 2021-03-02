// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://chrome-signin/edu_coexistence_app.js';

import {Screens} from 'chrome://chrome-signin/edu_coexistence_app.js';
import {EduCoexistenceBrowserProxyImpl} from 'chrome://chrome-signin/edu_coexistence_browser_proxy.js';
import {AuthMode, AuthParams} from 'chrome://chrome-signin/gaia_auth_host/authenticator.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

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
};

suite(edu_coexistence_app_tests.suiteName, function() {
  let appComponent;
  let testBrowserProxy;
  setup(function() {
    testBrowserProxy = new TestEduCoexistenceBrowserProxy();
    EduCoexistenceBrowserProxyImpl.instance_ = testBrowserProxy;
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


    PolymerTest.clearBody();
    appComponent = document.createElement('edu-coexistence-app');
    document.body.appendChild(appComponent);
    flush();
  });

  test(assert(edu_coexistence_app_tests.TestNames.InitOnline), function() {
    appComponent.setInitialScreen_(true /** online **/);
    assertEquals(appComponent.currentScreen_, Screens.ONLINE_FLOW);
  });

  test(assert(edu_coexistence_app_tests.TestNames.InitOffline), function() {
    appComponent.setInitialScreen_(false /** online **/);
    assertEquals(appComponent.currentScreen_, Screens.OFFLINE);
  });

  test(assert(edu_coexistence_app_tests.TestNames.ShowOffline), function() {
    appComponent.setInitialScreen_(true /** online **/);
    assertEquals(appComponent.currentScreen_, Screens.ONLINE_FLOW);

    window.dispatchEvent(new Event('offline'));
    assertEquals(appComponent.currentScreen_, Screens.OFFLINE);
  });

  test(assert(edu_coexistence_app_tests.TestNames.ShowOnline), function() {
    appComponent.setInitialScreen_(false /** online **/);
    assertEquals(appComponent.currentScreen_, Screens.OFFLINE);

    window.dispatchEvent(new Event('online'));
    assertEquals(appComponent.currentScreen_, Screens.ONLINE_FLOW);
  });

  test(assert(edu_coexistence_app_tests.TestNames.ShowError), function() {
    appComponent.setInitialScreen_(true /** online **/);
    assertEquals(appComponent.currentScreen_, Screens.ONLINE_FLOW);

    appComponent.fire('go-error');
    assertEquals(appComponent.currentScreen_, Screens.ERROR);
  });

  test(
      assert(
          edu_coexistence_app_tests.TestNames.DontSwitchViewIfDisplayingError),
      function() {
        appComponent.fire('go-error');
        assertEquals(appComponent.currentScreen_, Screens.ERROR);

        window.dispatchEvent(new Event('offline'));
        // Should still show error screen.
        assertEquals(appComponent.currentScreen_, Screens.ERROR);

        window.dispatchEvent(new Event('online'));
        // Should still show error screen.
        assertEquals(appComponent.currentScreen_, Screens.ERROR);
      });

  test(
      assert(edu_coexistence_app_tests.TestNames
                 .ShowErrorScreenImmediatelyOnLoadAbort),
      function() {
        assertEquals(appComponent.currentScreen_, Screens.ONLINE_FLOW);
        appComponent.$$('edu-coexistence-ui')
            .webview_.dispatchEvent(new Event('loadabort'));

        // Should show error screen.
        assertEquals(appComponent.currentScreen_, Screens.ERROR);
      });
});
