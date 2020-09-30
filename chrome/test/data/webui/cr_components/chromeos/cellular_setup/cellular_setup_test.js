// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/cellular_setup.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/setup_selection_flow.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/psim_flow_ui.m.js';

// #import {CellularSetupPageName} from 'chrome://resources/cr_components/chromeos/cellular_setup/cellular_types.m.js';
// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertTrue} from '../../../chai_assert.js';
// #import {FakeCellularSetupDelegate} from './fake_cellular_setup_delegate.m.js';
// clang-format on

suite('CrComponentsCellularSetupTest', function() {
  let cellularSetupPage;
  setup(function() {
    cellularSetupPage = document.createElement('cellular-setup');
    cellularSetupPage.delegate = new cellular_setup.FakeCellularSetupDelegate();
    document.body.appendChild(cellularSetupPage);
    Polymer.dom.flush();
  });

  test('Base test', function() {
    const ironPage = cellularSetupPage.$$('iron-pages');
    assertTrue(!!ironPage);
  });

  test('Page selection change', function() {
    assertTrue(
        cellularSetupPage.currentPageName_ ===
        cellularSetup.CellularSetupPageName.SETUP_FLOW_SELECTION);

    const selectionFlow = cellularSetupPage.$$('setup-selection-flow');
    assertTrue(!!selectionFlow);

    const psimBtn = selectionFlow.$$('#psimFlowUiBtn');
    assertTrue(!!psimBtn);

    psimBtn.click();
    assertTrue(
        cellularSetupPage.selectedFlow_ ===
        cellularSetup.CellularSetupPageName.PSIM_FLOW_UI);
  });
});
