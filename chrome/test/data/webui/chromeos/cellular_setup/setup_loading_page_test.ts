// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/cellular_setup/setup_loading_page.js';

import type {BasePageElement} from 'chrome://resources/ash/common/cellular_setup/base_page.js';
import type {SetupLoadingPageElement} from 'chrome://resources/ash/common/cellular_setup/setup_loading_page.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('CrComponentsSetupLoadingPageTest', function() {
  let setupLoadingPage: SetupLoadingPageElement;
  let basePage: BasePageElement|null;

  setup(function() {
    setupLoadingPage = document.createElement('setup-loading-page');
    document.body.appendChild(setupLoadingPage);
    flush();

    basePage = setupLoadingPage.shadowRoot!.querySelector('base-page');
    assertTrue(!!basePage);
  });

  test('Loading animation and error graphic shown correctly', function() {
    setupLoadingPage.isSimDetectError = false;
    flush();
    assertTrue(
        !!setupLoadingPage.shadowRoot!.querySelector('#animationContainer'));
    const simDetectError =
        setupLoadingPage.shadowRoot!.querySelector<HTMLElement>(
            '#simDetectError');
    assertTrue(!!simDetectError);
    assertTrue(simDetectError.hidden);

    setupLoadingPage.isSimDetectError = true;
    flush();
    assertFalse(
        !!setupLoadingPage.shadowRoot!.querySelector('#animationContainer'));
    assertFalse(simDetectError.hidden);
  });
});
