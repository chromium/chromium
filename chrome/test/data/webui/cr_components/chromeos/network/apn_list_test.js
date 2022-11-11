// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/apn_list.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../../chai_assert.js';

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

  test('Check if APN list count is correct', async function() {
    apnList.apns = [
      {name: 'apn1'},
      {name: 'apn2'},
      {name: 'apn3'},
      {name: 'apn4'},
      {name: 'apn5'},
      {name: 'apn6'},
    ];
    await flushTasks();
    assertEquals(
        apnList.shadowRoot.querySelectorAll('apn-list-item').length,
        apnList.apns.length);
  });
});
