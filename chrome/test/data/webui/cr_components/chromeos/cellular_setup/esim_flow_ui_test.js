// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/esim_flow_ui.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertTrue} from '../../../chai_assert.js';
// clang-format on

suite('CrComponentsEsimFlowUiTest', function() {
  let eSimPage;
  setup(function() {
    eSimPage = document.createElement('esim-flow-ui');
    document.body.appendChild(eSimPage);
    Polymer.dom.flush();
  });

  test('Base test', function() {
    const crInput = eSimPage.$$('cr-input');
    assertTrue(!!crInput);
  });
});
