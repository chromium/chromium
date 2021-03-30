// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/setup_loading_page.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertFalse, assertTrue} from '../../../chai_assert.js';
// #import {LoadingPageState} from 'chrome://resources/cr_components/chromeos/cellular_setup/setup_loading_page.m.js';
// #import {FakeCellularSetupDelegate} from './fake_cellular_setup_delegate.m.js';
// clang-format on

suite('CrComponentsSetupLoadingPageTest', function() {
  let simDetectPage;
  let basePage;
  let messageIcon;

  setup(function() {
    simDetectPage = document.createElement('setup-loading-page');
    simDetectPage.delegate = new cellular_setup.FakeCellularSetupDelegate();
    document.body.appendChild(simDetectPage);
    Polymer.dom.flush();

    basePage = simDetectPage.$$('base-page');
    assertTrue(!!basePage);
    messageIcon = basePage.$$('iron-icon');
    assertTrue(!!messageIcon);
  });

  test('No message is shown', function() {
    simDetectPage.state = LoadingPageState.LOADING;
    assertFalse(!!basePage.message);
    assertTrue(messageIcon.hidden);
  });

  test('Warning message is shown', function() {
    simDetectPage.state = LoadingPageState.CELLULAR_DISCONNECT_WARNING;
    assertEquals(
        basePage.message,
        'This may cause a brief cellular network disconnection.');
    assertFalse(messageIcon.hidden);
  });

  test('Retry error message is shown', function() {
    simDetectPage.state = LoadingPageState.SIM_DETECT_ERROR;
    assertEquals(
        basePage.message, simDetectPage.i18n('simDetectPageErrorMessage'));
    assertTrue(messageIcon.hidden);
  });

  test('Final error message is shown', function() {
    simDetectPage.state = LoadingPageState.FINAL_SIM_DETECT_ERROR;
    assertEquals(
        basePage.message, simDetectPage.i18n('simDetectPageFinalErrorMessage'));
    assertTrue(messageIcon.hidden);
  });
});
