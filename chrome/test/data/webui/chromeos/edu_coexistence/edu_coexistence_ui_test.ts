// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://chrome-signin/edu_coexistence/edu_coexistence_ui.js';

import {EduCoexistenceBrowserProxyImpl} from 'chrome://chrome-signin/edu_coexistence/edu_coexistence_browser_proxy.js';
import {EduCoexistenceButton} from 'chrome://chrome-signin/edu_coexistence/edu_coexistence_button.js';
import {EduCoexistenceUi} from 'chrome://chrome-signin/edu_coexistence/edu_coexistence_ui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestEduCoexistenceBrowserProxy} from './edu_coexistence_test_browser_proxy.js';

suite('EduCoexistenceUiTest', function() {
  let coexistenceUi: EduCoexistenceUi;
  let testBrowserProxy: TestEduCoexistenceBrowserProxy;
  let webview: chrome.webviewTag.WebView;

  setup(function() {
    testBrowserProxy = new TestEduCoexistenceBrowserProxy();
    EduCoexistenceBrowserProxyImpl.setInstance(testBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    coexistenceUi = new EduCoexistenceUi();
    document.body.appendChild(coexistenceUi);
    // The webview needs to be set explicitly in for the test because
    // the component itself doesn't initialize the webview until
    // too late.  This is OK because we just need a webview in there
    // to access the back() and focus() methods.
    webview = document.createElement('webview') as chrome.webviewTag.WebView;
    coexistenceUi.setWebviewForTest(webview);
    flush();
  });

  test('DisableGaiaBackButtonAfterClick', function() {
    // Fake out the relevant webview methods.
    let backCalled = false;
    webview.back = () => {
      backCalled = true;
    };

    const backButton = coexistenceUi.shadowRoot!.querySelector(
                           '#gaia-back-button')! as EduCoexistenceButton;
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
