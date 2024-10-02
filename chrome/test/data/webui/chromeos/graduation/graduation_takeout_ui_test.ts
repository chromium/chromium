// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://graduation/js/graduation_takeout_ui.js';
import 'chrome://graduation/strings.m.js';

import {ScreenSwitchEvents} from 'chrome://graduation/js/graduation_app.js';
import {GraduationTakeoutUi} from 'chrome://graduation/js/graduation_takeout_ui.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {PaperSpinnerLiteElement} from 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';


suite('GraduationTakeoutUiTest', function() {
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

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    graduationUi = new GraduationTakeoutUi();
    document.body.appendChild(graduationUi);
    flush();
  });

  teardown(() => {
    graduationUi.remove();
  });

  test('HideSpinnerAndShowUiOnContentLoad', function() {
    assertFalse(getSpinner().hidden);
    assertTrue(getWebview().hidden);
    assertTrue(getBackButton().hidden);
    assertTrue(getDoneButton().hidden);
    getWebview().dispatchEvent(new CustomEvent('contentload'));
    assertTrue(getSpinner().hidden);
    assertFalse(getWebview().hidden);
    assertFalse(getBackButton().hidden);
    assertTrue(getDoneButton().hidden);
    assertEquals(
        'https://takeout.google.com/transfer?hl=en-US', getWebview().src);
  });

  test('TriggerErrorScreenOnLoadAbort', function() {
    let errorPageTriggered = false;
    assertFalse(getSpinner().hidden);
    assertTrue(getWebview().hidden);

    graduationUi.addEventListener(ScreenSwitchEvents.SHOW_ERROR, () => {
      errorPageTriggered = true;
    });
    getWebview().dispatchEvent(new CustomEvent('loadabort'));
    assertTrue(getSpinner().hidden);
    assertFalse(getWebview().hidden);
    assertTrue(errorPageTriggered);
  });

  test('TriggerWelcomePageOnBackButtonClick', function() {
    let welcomePageTriggered = false;
    getWebview().dispatchEvent(new CustomEvent('contentload'));
    assertFalse(getBackButton().hidden);

    graduationUi.addEventListener(ScreenSwitchEvents.SHOW_WELCOME, () => {
      welcomePageTriggered = true;
    });
    getBackButton().click();
    assertTrue(welcomePageTriggered);
  });
});
