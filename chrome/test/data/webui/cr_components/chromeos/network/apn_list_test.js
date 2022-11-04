// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/apn_list.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertTrue} from '../../../chai_assert.js';

suite('ApnListTest', function() {
  /** @type {ApnListElement} */
  let apnList = null;

  setup(function() {
    apnList = document.createElement('apn-list');
    document.body.appendChild(apnList);
    flush();
  });

  test('Check if APN description exists', async function() {
    assertTrue(!!apnList);
    assertTrue(!!apnList.shadowRoot.querySelector('localized-link'));
  });
});
