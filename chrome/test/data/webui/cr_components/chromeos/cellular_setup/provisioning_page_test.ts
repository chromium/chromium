// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/ash/common/cellular_setup/provisioning_page.js';

import type {ProvisioningPageElement} from 'chrome://resources/ash/common/cellular_setup/provisioning_page.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

import {FakeCellularSetupDelegate} from './fake_cellular_setup_delegate.js';

suite('CrComponentsProvisioningPageTest', function() {
  let provisioningPage: ProvisioningPageElement;

  setup(function() {
    provisioningPage = document.createElement('provisioning-page');
    provisioningPage.delegate = new FakeCellularSetupDelegate();
    document.body.appendChild(provisioningPage);
    flush();
  });

  test('Base test', function() {
    const basePage = provisioningPage.shadowRoot!.querySelector('base-page');
    assertTrue(!!basePage);
  });
});
