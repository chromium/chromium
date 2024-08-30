// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://graduation/js/graduation_ui.js';
import 'chrome://graduation/strings.m.js';

import {GraduationUi} from 'chrome://graduation/js/graduation_ui.js';
import {PaperSpinnerLiteElement} from 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';


suite('GraduationUiTest', function() {
  let graduationUi: GraduationUi;

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

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    graduationUi = new GraduationUi();
    document.body.appendChild(graduationUi);
    flush();
  });

  teardown(() => {
    graduationUi.remove();
  });

  test('HideSpinnerOnContentLoad', function() {
    assertFalse(getSpinner().hidden);
    assertTrue(getWebview().hidden);
    graduationUi.shadowRoot!.querySelector('webview')!.dispatchEvent(
        new CustomEvent('contentload'));
    assertTrue(getSpinner().hidden);
    assertFalse(getWebview().hidden);
  });

  test('HideSpinnerOnLoadAbort', function() {
    assertFalse(getSpinner().hidden);
    assertTrue(getWebview().hidden);
    graduationUi.shadowRoot!.querySelector('webview')!.dispatchEvent(
        new CustomEvent('loadabort'));
    assertTrue(getSpinner().hidden);
    assertFalse(getWebview().hidden);
  });
});
