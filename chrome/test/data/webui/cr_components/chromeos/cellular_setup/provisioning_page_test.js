// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/cellular_setup/provisioning_page.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertTrue} from '../../../chromeos/chai_assert.js';

import {FakeCellularSetupDelegate} from './fake_cellular_setup_delegate.js';

suite('CrComponentsProvisioningPageTest', function() {
  let provisioningPage;
  setup(function() {
    provisioningPage = document.createElement('provisioning-page');
    provisioningPage.delegate = new FakeCellularSetupDelegate();
    document.body.appendChild(provisioningPage);
    flush();
  });

  test('Base test', function() {
    const basePage = provisioningPage.$$('base-page');
    assertTrue(!!basePage);
  });
});
