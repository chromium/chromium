// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/sim_detect_page.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertFalse, assertTrue} from '../../../chai_assert.js';
// #import {FakeCellularSetupDelegate} from './fake_cellular_setup_delegate.m.js';
// clang-format on

suite('CrComponentsSimDetectPageTest', function() {
  let simDetectPage;
  setup(function() {
    simDetectPage = document.createElement('sim-detect-page');
    simDetectPage.delegate = new cellular_setup.FakeCellularSetupDelegate();
    document.body.appendChild(simDetectPage);
    Polymer.dom.flush();
  });

  test('Base test', function() {
    const basePage = simDetectPage.$$('base-page');
    assertTrue(!!basePage);
  });
});
