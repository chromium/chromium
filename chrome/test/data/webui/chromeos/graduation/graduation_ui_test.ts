// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://graduation/js/graduation_ui.js';
import 'chrome://graduation/strings.m.js';

import {GraduationUi} from 'chrome://graduation/js/graduation_ui.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {PaperSpinnerLiteElement} from 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';


suite('GraduationUiTest', function() {
  let graduationUi: GraduationUi;

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
    graduationUi = new GraduationUi();
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
  });

  test('HideSpinnerOnLoadAbort', function() {
    assertFalse(getSpinner().hidden);
    assertTrue(getWebview().hidden);
    getWebview().dispatchEvent(new CustomEvent('loadabort'));
    assertTrue(getSpinner().hidden);
    assertFalse(getWebview().hidden);
  });
});
