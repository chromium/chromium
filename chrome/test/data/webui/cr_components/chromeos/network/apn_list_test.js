// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/apn_list.js';

import {ApnDetailDialogMode} from '//resources/ash/common/network/cellular_utils.js';
import {ApnList} from 'chrome://resources/ash/common/network/apn_list.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ApnProperties, ApnState} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('ApnListTest', function() {
  /** @type {ApnList} */
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

  /** @type {ApnProperties} */
  const customApn1 = {
    accessPointName: 'Custom Access Point 1',
    name: 'AP-name-custom-1',
  };

  /** @type {ApnProperties} */
  const customApn2 = {
    accessPointName: 'Custom Access Point 2',
    name: 'AP-name-custom-2',
  };

  /** @type {ApnProperties} */
  const customApn3 = {
    accessPointName: 'Custom Access Point 3',
    name: 'AP-name-custom-3',
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

  test('There is no Connected APN and no custom APNs', async function() {
    apnList.managedCellularProperties = {};
    await flushTasks();
    const apns = apnList.shadowRoot.querySelectorAll('apn-list-item');
    assertEquals(apns.length, 0);
  });

  test(
      'There are custom APNs and there is no Connected APN ', async function() {
        apnList.managedCellularProperties = {
          customApnList: [customApn1, customApn2],
        };
        await flushTasks();
        const apns = apnList.shadowRoot.querySelectorAll('apn-list-item');
        assertEquals(apns.length, 2);
        assertTrue(OncMojo.apnMatch(apns[0].apn, customApn1));
        assertTrue(OncMojo.apnMatch(apns[1].apn, customApn2));
        assertFalse(apns[0].isConnected);
        assertFalse(apns[0].isAutoDetected);
      });

  test(
      'Connected APN is inside apnList and there are no custom APNs',
      async function() {
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

  test(
      'Connected APN is inside apnList and there are custom APNs.',
      async function() {
        apnList.managedCellularProperties = {
          connectedApn: connectedApn,
          apnList: {
            activeValue: [apn1, apn2, connectedApn],
          },
          customApnList: [customApn1, customApn2],
        };
        await flushTasks();
        const apns = apnList.shadowRoot.querySelectorAll('apn-list-item');
        assertEquals(apns.length, 3);
        assertTrue(OncMojo.apnMatch(apns[0].apn, connectedApn));
        assertTrue(OncMojo.apnMatch(apns[1].apn, customApn1));
        assertTrue(OncMojo.apnMatch(apns[2].apn, customApn2));
        assertTrue(apns[0].isConnected);
        assertTrue(apns[0].isAutoDetected);
      });

  test('Connected APN is inside custom APN list.', async function() {
    apnList.managedCellularProperties = {
      connectedApn: connectedApn,
      apnList: {
        activeValue: [apn1, apn2],
      },
      customApnList: [customApn1, customApn2, customApn3, connectedApn],
    };
    await flushTasks();
    const apns = apnList.shadowRoot.querySelectorAll('apn-list-item');
    assertEquals(apns.length, 4);
    assertTrue(OncMojo.apnMatch(apns[0].apn, connectedApn));
    assertTrue(OncMojo.apnMatch(apns[1].apn, customApn1));
    assertTrue(OncMojo.apnMatch(apns[2].apn, customApn2));
    assertTrue(OncMojo.apnMatch(apns[3].apn, customApn3));
    assertTrue(apns[0].isConnected);
    assertFalse(apns[0].isAutoDetected);
  });

  test('Connected APN is the only apn in custom APN list.', async function() {
    apnList.managedCellularProperties = {
      connectedApn: connectedApn,
      customApnList: [connectedApn],
    };
    await flushTasks();
    const apns = apnList.shadowRoot.querySelectorAll('apn-list-item');
    assertEquals(apns.length, 1);
    assertTrue(OncMojo.apnMatch(apns[0].apn, connectedApn));
    assertTrue(apns[0].isConnected);
    assertFalse(apns[0].isAutoDetected);
  });

  test(
      'Calling openApnDetailDialogInCreateMode() opens APN detail dialog',
      async function() {
        const getApnDetailDialog = () =>
            apnList.shadowRoot.querySelector('apn-detail-dialog');
        apnList.guid = 'fake-guid';
        assertFalse(!!getApnDetailDialog());
        apnList.openApnDetailDialogInCreateMode();
        await flushTasks();
        assertTrue(!!getApnDetailDialog());
        assertEquals(ApnDetailDialogMode.CREATE, getApnDetailDialog().mode);
        assertEquals(apnList.guid, getApnDetailDialog().guid);
        assertFalse(!!getApnDetailDialog().apnProperties);
      });
});
