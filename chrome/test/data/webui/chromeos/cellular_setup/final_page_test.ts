// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cellular_setup/final_page.js';

import type {FinalPageElement} from 'chrome://resources/ash/common/cellular_setup/final_page.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

import {FakeCellularSetupDelegate} from './fake_cellular_setup_delegate.js';

suite('CrComponentsFinalPageTest', function() {
  let finalPage: FinalPageElement;

  setup(function() {
    finalPage = document.createElement('final-page');
    finalPage.delegate = new FakeCellularSetupDelegate();
    document.body.appendChild(finalPage);
    flush();
  });

  test('Base test', function() {
    const basePage = finalPage.shadowRoot!.querySelector('base-page');
    assertTrue(!!basePage);
  });
});
