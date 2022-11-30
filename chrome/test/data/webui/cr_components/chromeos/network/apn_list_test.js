// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/apn_list.js';

import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ApnProperties, ApnState} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('ApnListTest', function() {
  /** @type {ApnListElement} */
  let apnList = null;

  /** @type {ApnProperties} */
  const connectedApn = {
    accessPointName: 'Access Point',
    name: 'AP-name',
  };
  /** @type {ApnProperties} */
  const apn1 = {
    accessPointName: 'Access Point 1',
    name: 'AP-name-1',
  };

  /** @type {ApnProperties} */
  const apn2 = {
    accessPointName: 'Access Point 2',
    name: 'AP-name-2',
  };

  setup(async function() {
    apnList = document.createElement('apn-list');
    document.body.appendChild(apnList);
    await flushTasks();
  });

  test('Check if APN description exists', async function() {
    assertTrue(!!apnList);
    const getDescriptionWithLink = () =>
        apnList.shadowRoot.querySelector('localized-link');
    assertTrue(!!getDescriptionWithLink());
    const getDescriptionWithoutLink = () =>
        apnList.shadowRoot.querySelector('#descriptionNoLink');
    assertFalse(!!getDescriptionWithoutLink());

    apnList.shouldOmitLinks = true;
    await flushTasks();
    assertFalse(!!getDescriptionWithLink());
    assertTrue(!!getDescriptionWithoutLink());
  });

  test('No managedCellularProperties', async function() {
    apnList.managedCellularProperties = undefined;
    await flushTasks();
    assertEquals(
        apnList.shadowRoot.querySelectorAll('apn-list-item').length, 0);
  });

  test('Connected APN is inside apnList', async function() {
    apnList.managedCellularProperties = {
      connectedApn: connectedApn,
      apnList: {
        activeValue: [apn1, apn2, connectedApn],
      },
    };
    await flushTasks();
    const apns = apnList.shadowRoot.querySelectorAll('apn-list-item');
    assertEquals(apns.length, 1);
    assertTrue(OncMojo.apnMatch(apns[0].apn, connectedApn));
    assertTrue(apns[0].isConnected);
    assertTrue(apns[0].isAutoDetected);
  });
});
