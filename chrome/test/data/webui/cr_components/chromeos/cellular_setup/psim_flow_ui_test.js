// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/psim_flow_ui.m.js';

// #import {PSimUIState} from 'chrome://resources/cr_components/chromeos/cellular_setup/psim_flow_ui.m.js';
// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertTrue} from '../../../chai_assert.js';
// #import {FakeCellularSetupDelegate} from './fake_cellular_setup_delegate.m.js';
// clang-format on

suite('CrComponentsPsimFlowUiTest', function() {
  let pSimPage;
  setup(function() {
    pSimPage = document.createElement('psim-flow-ui');
    pSimPage.delegate = new cellular_setup.FakeCellularSetupDelegate();
    document.body.appendChild(pSimPage);
    Polymer.dom.flush();
  });

  test('Base test', function() {
    const ironPage = pSimPage.$$('iron-pages');
    assertTrue(!!ironPage);
  });

  test('forward navigation test', function() {
    pSimPage.state_ = cellularSetup.PSimUIState.WAITING_FOR_PORTAL_TO_LOAD;
    Polymer.dom.flush();
    pSimPage.navigateForward();
    assertTrue(
        pSimPage.state_ ===
        cellularSetup.PSimUIState.WAITING_FOR_ACTIVATION_TO_FINISH);
  });
});
