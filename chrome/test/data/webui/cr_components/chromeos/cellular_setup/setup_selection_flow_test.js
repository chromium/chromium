// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/setup_selection_flow.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertTrue} from '../../../chai_assert.js';
// #import {FakeCellularSetupDelegate} from './fake_cellular_setup_delegate.m.js';
// clang-format on

suite('CrComponentsSetupSelectionFlowTest', function() {
  let setupSelectionFlow;
  setup(function() {
    setupSelectionFlow = document.createElement('setup-selection-flow');
    setupSelectionFlow.delegate =
        new cellular_setup.FakeCellularSetupDelegate();
    document.body.appendChild(setupSelectionFlow);
    Polymer.dom.flush();
  });

  test('Base test', function() {
    const crRadio = setupSelectionFlow.$$('cr-radio-group');
    assertTrue(!!crRadio);
  });
});
