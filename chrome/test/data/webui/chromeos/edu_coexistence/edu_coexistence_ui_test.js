// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://chrome-signin/edu_coexistence_ui.js';

import {EduCoexistenceBrowserProxyImpl} from 'chrome://chrome-signin/edu_coexistence_browser_proxy.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestEduCoexistenceBrowserProxy} from './edu_coexistence_test_browser_proxy.js';

suite('EduCoexistenceUiTest', function() {
  let coexistenceUi;
  let testBrowserProxy;
  let webview;
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
    coexistenceUi = document.createElement('edu-coexistence-ui');
    document.body.appendChild(coexistenceUi);
    // The webview needs to be set explicitly in for the test because
    // the component itself doesn't initialize the webview until
    // too late.  This is OK because we just need a webview in there
    // to access the back() and focus() methods.
    webview = document.createElement('webview');
    coexistenceUi.setWebviewForTest(webview);
    flush();
  });

  test(
      'DisableGaiaBackButtonAfterClick', function() {
        // Fake out the relevant webview methods.
        let backCalled = false;
        webview.back = (success) => {
          backCalled = true;
        };

        const backButton =
            coexistenceUi.shadowRoot.querySelector('#gaia-back-button');
        // Simulate being on the Gaia signin page by enabling the
        // Gaia back button
        backButton.disabled = false;

        // Call the back button action.
        backButton.dispatchEvent(new CustomEvent('go-back'));

        // Should have called webview_.back() and disabled the button.
        assertTrue(backCalled);
        assertTrue(backButton.disabled);

        // Reset the signal in the fake.
        backCalled = false;

        // Simulate a rapid double-click by immediately calling
        // the action again.
        backButton.dispatchEvent(new CustomEvent('go-back'));

        // The webview_.back() method should not be called again.
        assertFalse(backCalled);
      });
});
