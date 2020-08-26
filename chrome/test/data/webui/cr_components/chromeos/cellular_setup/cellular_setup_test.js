// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/cellular_setup.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertTrue} from '../../../chai_assert.js';
// clang-format on

suite('CrComponentsCellularSetupTest', function() {
  let cellularSetup;
  setup(function() {
    cellularSetup = document.createElement('cellular-setup');
    document.body.appendChild(cellularSetup);
    Polymer.dom.flush();
  });

  test('Base test', function() {
    const ironPage = cellularSetup.$$('iron-pages');
    assertTrue(!!ironPage);
  });
});
