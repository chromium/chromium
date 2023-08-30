// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://chrome-signin/edu_coexistence_ui.js';

import {EduCoexistenceBrowserProxyImpl} from 'chrome://chrome-signin/edu_coexistence_browser_proxy.js';
import {AuthMode, AuthParams} from 'chrome://chrome-signin/gaia_auth_host/authenticator.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getFakeAccountsList, TestEduCoexistenceBrowserProxy} from './edu_coexistence_test_util.js';

window.edu_coexistence_ui_tests = {};
edu_coexistence_ui_tests.suiteName = 'EduCoexistenceUiTest';

/** @enum {string} */
edu_coexistence_ui_tests.TestNames = {
  DisableGaiaBackButtonAfterClick: 'Disable Gaia Back Button after Click',
};

suite(edu_coexistence_ui_tests.suiteName, function() {
  let appComponent;
  let testBrowserProxy;
  setup(function() {
    testBrowserProxy = new TestEduCoexistenceBrowserProxy();
    EduCoexistenceBrowserProxyImpl.setInstance(testBrowserProxy);
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
    appComponent = document.createElement('edu-coexistence-ui');
    document.body.appendChild(appComponent);
    // The webview needs to be set explicitly in for the test because
    // the component itself doesn't initialize the webview until
    // too late.  This is OK because we just need a webview in there
    // to access the back() and focus() methods.
    appComponent.webview_ = document.createElement('webview');
    flush();
  });

  test(
      assert(
          edu_coexistence_ui_tests.TestNames.DisableGaiaBackButtonAfterClick),
      function() {
        // Fake out the relevant webview methods.
        let backCalled = false;
        appComponent.webview_.back = (success) => {
          backCalled = true;
        };

        const backButton = appComponent.root.getElementById('gaia-back-button');
        // Simulate being on the Gaia signin page by enabling the
        // Gaia back button
        backButton.disabled = false;

        // Call the back button action.
        appComponent.handleGaiaLoginGoBack_(new Event('click'));

        // Should have called webview_.back() and disabled the button.
        assertTrue(backCalled);
        assertTrue(backButton.disabled);

        // Reset the signal in the fake.
        backCalled = false;

        // Simulate a rapid double-click by immediately calling
        // the actiona gain.
        appComponent.handleGaiaLoginGoBack_(new Event('click'));

        // The webview_.back() method should not be called again.
        assertFalse(backCalled);
      });
});
