// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://graduation/js/graduation_takeout_ui.js';
import 'chrome://graduation/strings.m.js';

import {ScreenSwitchEvents} from 'chrome://graduation/js/graduation_app.js';
import {GraduationTakeoutUi, WebviewReloadHelper} from 'chrome://graduation/js/graduation_takeout_ui.js';
import {AuthResult} from 'chrome://graduation/mojom/graduation_ui.mojom-webui.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PaperSpinnerLiteElement} from 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

let graduationUi: GraduationTakeoutUi;

function getBackButton(): CrButtonElement {
  const backButton =
      graduationUi.shadowRoot!.querySelector<CrButtonElement>('#backButton');
  assertTrue(!!backButton);
  return backButton;
}

function getSpinner(): PaperSpinnerLiteElement {
  const spinner =
      graduationUi.shadowRoot!.querySelector<PaperSpinnerLiteElement>(
          '.spinner-container');
  assertTrue(!!spinner);
  return spinner;
}

function getWebview(): chrome.webviewTag.WebView {
  const webview =
      graduationUi.shadowRoot!.querySelector<chrome.webviewTag.WebView>(
          'webview');
  assertTrue(!!webview);
  return webview;
}

function getDoneButton(): CrButtonElement {
  const doneButton =
      graduationUi.shadowRoot!.querySelector<CrButtonElement>('#doneButton');
  assertTrue(!!doneButton);
  return doneButton;
}

function assertLoadingScreenActive(): void {
  assertFalse(getSpinner().hidden);
  assertTrue(getWebview().hidden);
}

function assertLoadingScreenHidden(): void {
  assertTrue(getSpinner().hidden);
  assertFalse(getWebview().hidden);
}

function simulateAuthenticationSuccess(): void {
  graduationUi.onAuthComplete(AuthResult.kSuccess);
  assertLoadingScreenActive();
}

suite('GraduationTakeoutUiTest.WebviewUrl', function() {
  const isEmbeddedEndpointEnabled =
      loadTimeData.getBoolean('isEmbeddedEndpointEnabled');

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    graduationUi = new GraduationTakeoutUi();
    document.body.appendChild(graduationUi);
    flush();
  });

  teardown(() => {
    graduationUi.remove();
  });

  // The webview source needs to be tested in a separate suite before
  // GraduationTakeoutUiTest runs because that suite replaces the webview source
  // with an empty string in the load-time data.
  test('The initial webview source has the expected base URL', function() {
    // Simulate that authentication has succeeded.
    graduationUi.onAuthComplete(AuthResult.kSuccess);

    if (isEmbeddedEndpointEnabled) {
      assertTrue(getWebview().src.startsWith(
          'https://takeout.google.com/embedded/transfer'));
    } else {
      assertTrue(
          getWebview().src.startsWith('https://takeout.google.com/transfer'));
    }
  });
});

suite('GraduationTakeoutUiTest', function() {
  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    graduationUi = new GraduationTakeoutUi();

    // Set an empty webview URL to avoid an unintentional loadabort event in the
    // webview.
    loadTimeData.overrideValues({startTransferUrl: ''});

    document.body.appendChild(graduationUi);
    flush();
  });

  teardown(() => {
    graduationUi.remove();
  });

  test(
      'Spinner is hidden when authentication is complete and the webview loads',
      function() {
        assertLoadingScreenActive();
        assertTrue(isVisible(getBackButton()));
        assertTrue(getDoneButton().hidden);

        simulateAuthenticationSuccess();

        assertLoadingScreenActive();
        assertTrue(isVisible(getBackButton()));
        assertTrue(getDoneButton().hidden);

        getWebview().dispatchEvent(new CustomEvent('contentload'));

        assertLoadingScreenHidden();
        assertTrue(isVisible(getBackButton()));
        assertFalse(getDoneButton().hidden);
        assertTrue(getDoneButton().disabled);
      });

  test('Error screen is triggered if authentication has failed', function() {
    let errorPageTriggered = false;
    assertLoadingScreenActive();

    graduationUi.addEventListener(ScreenSwitchEvents.SHOW_ERROR, () => {
      errorPageTriggered = true;
    });

    // Simulate that authentication has failed.
    graduationUi.onAuthComplete(AuthResult.kError);
    assertLoadingScreenHidden();

    assertTrue(errorPageTriggered);
  });

  test('Welcome page is triggered on Back button click', function() {
    let welcomePageTriggered = false;
    assertTrue(isVisible(getBackButton()));

    graduationUi.addEventListener(ScreenSwitchEvents.SHOW_WELCOME, () => {
      welcomePageTriggered = true;
    });
    getBackButton().click();
    assertTrue(welcomePageTriggered);
  });
});

suite('GraduationTakeoutUiTest.WebviewReload', function() {
  let errorPageTriggered = false;

  function exhaustReloadAttempts(): void {
    for (let attempts = 1; attempts < WebviewReloadHelper.MAX_RELOAD_ATTEMPTS;
         attempts++) {
      getWebview().dispatchEvent(new CustomEvent('loadabort'));
      assertLoadingScreenActive();
      assertFalse(errorPageTriggered);
    }
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    graduationUi = new GraduationTakeoutUi();

    // Set an empty webview URL to avoid an unintentional loadabort event in the
    // webview.
    loadTimeData.overrideValues({startTransferUrl: ''});

    document.body.appendChild(graduationUi);
    flush();
  });

  teardown(() => {
    graduationUi.remove();
  });

  test('UI is shown if reload succeeds after a loadabort event', function() {
    errorPageTriggered = false;
    assertLoadingScreenActive();

    graduationUi.addEventListener(ScreenSwitchEvents.SHOW_ERROR, () => {
      errorPageTriggered = true;
    });

    simulateAuthenticationSuccess();

    getWebview().dispatchEvent(new CustomEvent('loadabort'));

    assertLoadingScreenActive();
    assertFalse(errorPageTriggered);

    getWebview().dispatchEvent(new CustomEvent('contentload'));

    assertLoadingScreenHidden();
    assertFalse(errorPageTriggered);
  });

  test(
      'Error screen is shown after the maximum allowed failed reload attempts',
      function() {
        errorPageTriggered = false;
        assertLoadingScreenActive();

        graduationUi.addEventListener(ScreenSwitchEvents.SHOW_ERROR, () => {
          errorPageTriggered = true;
        });

        simulateAuthenticationSuccess();

        getWebview().dispatchEvent(new CustomEvent('loadabort'));
        assertLoadingScreenActive();
        assertFalse(errorPageTriggered);

        exhaustReloadAttempts();

        // The error screen should surface when the last allowed consecutive
        // reload attempt fails.
        getWebview().dispatchEvent(new CustomEvent('loadabort'));

        assertLoadingScreenHidden();
        assertTrue(errorPageTriggered);
      });

  test('Reload occurs on Back button click', function() {
    assertLoadingScreenActive();

    simulateAuthenticationSuccess();

    getWebview().dispatchEvent(new CustomEvent('contentload'));

    assertLoadingScreenHidden();

    assertTrue(isVisible(getBackButton()));
    getBackButton().click();

    assertLoadingScreenActive();
  });

  test('Reload helpers are reset on Back button click', function() {
    errorPageTriggered = false;

    graduationUi.addEventListener(ScreenSwitchEvents.SHOW_ERROR, () => {
      errorPageTriggered = true;
    });

    simulateAuthenticationSuccess();

    getWebview().dispatchEvent(new CustomEvent('loadabort'));
    exhaustReloadAttempts();

    assertTrue(isVisible(getBackButton()));
    getBackButton().click();

    // Clicking the back button should have cleared the reload counter, so the
    // error screen should not be shown until the attempt limit is reached.
    getWebview().dispatchEvent(new CustomEvent('loadabort'));
    exhaustReloadAttempts();

    getWebview().dispatchEvent(new CustomEvent('loadabort'));
    assertTrue(errorPageTriggered);
  });
});
