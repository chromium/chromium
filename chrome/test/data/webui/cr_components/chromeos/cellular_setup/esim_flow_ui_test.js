// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/esim_flow_ui.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {ButtonState} from 'chrome://resources/cr_components/chromeos/cellular_setup/cellular_types.m.js';
// #import {ESimPageName} from 'chrome://resources/cr_components/chromeos/cellular_setup/esim_flow_ui.m.js';
// #import {assertTrue} from '../../../chai_assert.js';
// #import {FakeCellularSetupDelegate} from './fake_cellular_setup_delegate.m.js';
// clang-format on

suite('CrComponentsEsimFlowUiTest', function() {
  let eSimPage;

  setup(function() {
    eSimPage = document.createElement('esim-flow-ui');
    eSimPage.delegate = new cellular_setup.FakeCellularSetupDelegate();
    eSimPage.initSubflow();
    document.body.appendChild(eSimPage);
    Polymer.dom.flush();
  });

  test('Forward navigation goes to final page', function() {
    const profileDiscoveryPage = eSimPage.$$('#profileDiscoveryPage');
    const finalPage = eSimPage.$$('#finalPage');

    assertTrue(!!profileDiscoveryPage);
    assertTrue(!!finalPage);

    assertTrue(
        eSimPage.selectedESimPageName_ ===
            cellular_setup.ESimPageName.PROFILE_DISCOVERY &&
        eSimPage.selectedESimPageName_ === profileDiscoveryPage.id);

    eSimPage.navigateForward();
    Polymer.dom.flush();

    // TODO(crbug.com/1093185) Update this test when the navigation between
    // profile discovery and activation code pages is wired up.
    assertTrue(
        eSimPage.selectedESimPageName_ === cellular_setup.ESimPageName.FINAL &&
        eSimPage.selectedESimPageName_ === finalPage.id);
  });


  // TODO(crbug.com/1093185) Update this test when the navigation between
  // profile discovery and activation code pages is wired up.
  test('Enable done button', function() {
    assertTrue(eSimPage.buttonState.done === cellularSetup.ButtonState.HIDDEN);

    const profileDiscoveryPage = eSimPage.$$('#profileDiscoveryPage');
    const profileList = profileDiscoveryPage.$$('#profileList');
    profileList.selectItem(profileList.items[0]);
    Polymer.dom.flush();

    assertTrue(
        eSimPage.buttonState.done ===
        cellularSetup.ButtonState.SHOWN_AND_ENABLED);
  });
});
