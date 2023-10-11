// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {NearbyShareHighVisibilityPageElement, nearbyShareMojom} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

const {RegisterReceiveSurfaceResult} = nearbyShareMojom;

suite('<nearby-share-high-visibility-page>', () => {
  let nearbyShareHighVisibilityPage: NearbyShareHighVisibilityPageElement;

  setup(() => {
    nearbyShareHighVisibilityPage =
        document.createElement('nearby-share-high-visibility-page');

    document.body.appendChild(nearbyShareHighVisibilityPage);
    flush();
  });

  test('Renders help text by default', () => {
    assert(
        nearbyShareHighVisibilityPage.shadowRoot!.querySelector('#helpText'));
    assertEquals(
        null,
        nearbyShareHighVisibilityPage.shadowRoot!.querySelector('#errorTitle'));
    assertEquals(
        null,
        nearbyShareHighVisibilityPage.shadowRoot!.querySelector(
            '#errorDescription'));
  });

  test('Handles Timed out', async () => {
    nearbyShareHighVisibilityPage.set('remainingTimeInSeconds_', 0);
    nearbyShareHighVisibilityPage.set('shutoffTimestamp', 1);
    await flushTasks();

    assertEquals(
        null,
        nearbyShareHighVisibilityPage.shadowRoot!.querySelector('#helpText'));
    const errorTitle =
        nearbyShareHighVisibilityPage.shadowRoot!.querySelector('#errorTitle');
    assert(errorTitle);
    assertEquals(
        nearbyShareHighVisibilityPage.i18n('nearbyShareErrorTimeOut'),
        errorTitle.innerHTML.trim());
    assertTrue(!!nearbyShareHighVisibilityPage.shadowRoot!.querySelector(
        '#errorDescription'));
  });

  test('Handles No Connection Medium error', async () => {
    nearbyShareHighVisibilityPage.set(
        'registerResult', RegisterReceiveSurfaceResult['kNoConnectionMedium']);
    await flushTasks();

    assertEquals(
        null,
        nearbyShareHighVisibilityPage.shadowRoot!.querySelector('#helpText'));
    const errorTitle =
        nearbyShareHighVisibilityPage.shadowRoot!.querySelector('#errorTitle');
    assert(errorTitle);
    assertEquals(
        nearbyShareHighVisibilityPage.i18n(
            'nearbyShareErrorNoConnectionMedium'),
        errorTitle.innerHTML.trim());
    assert(nearbyShareHighVisibilityPage.shadowRoot!.querySelector(
        '#errorDescription'));
  });

  test('Handles Transfer in Progress error', async () => {
    nearbyShareHighVisibilityPage.set(
        'registerResult', RegisterReceiveSurfaceResult['kTransferInProgress']);
    await flushTasks();

    assertEquals(
        null,
        nearbyShareHighVisibilityPage.shadowRoot!.querySelector('#helpText'));
    const errorTitle =
        nearbyShareHighVisibilityPage.shadowRoot!.querySelector('#errorTitle');
    assert(errorTitle);
    assertEquals(
        nearbyShareHighVisibilityPage.i18n(
            'nearbyShareErrorTransferInProgressTitle'),
        errorTitle.innerHTML.trim());
    assert(nearbyShareHighVisibilityPage.shadowRoot!.querySelector(
        '#errorDescription'));
  });

  test('Handles Failure error', async () => {
    nearbyShareHighVisibilityPage.set(
        'registerResult', RegisterReceiveSurfaceResult['kFailure']);
    await flushTasks();

    assertEquals(
        null,
        nearbyShareHighVisibilityPage.shadowRoot!.querySelector('#helpText'));
    const errorTitle =
        nearbyShareHighVisibilityPage.shadowRoot!.querySelector('#errorTitle');
    assert(errorTitle);
    assertEquals(
        nearbyShareHighVisibilityPage.i18n('nearbyShareErrorCantReceive'),
        errorTitle.innerHTML.trim());
    assert(nearbyShareHighVisibilityPage.shadowRoot!.querySelector(
        '#errorDescription'));
  });

  test('Handles Nearby process stopped', async () => {
    nearbyShareHighVisibilityPage.set('nearbyProcessStopped', true);
    await flushTasks();

    assertEquals(
        null,
        nearbyShareHighVisibilityPage.shadowRoot!.querySelector('#helpText'));
    const errorTitle =
        nearbyShareHighVisibilityPage.shadowRoot!.querySelector('#errorTitle');
    assert(errorTitle);
    assertEquals(
        nearbyShareHighVisibilityPage.i18n('nearbyShareErrorCantReceive'),
        errorTitle.innerHTML.trim());
    assert(nearbyShareHighVisibilityPage.shadowRoot!.querySelector(
        '#errorDescription'));
  });

  test('Handles Start advertising failed', async () => {
    nearbyShareHighVisibilityPage.set('startAdvertisingFailed', true);
    await flushTasks();

    assertEquals(
        null,
        nearbyShareHighVisibilityPage.shadowRoot!.querySelector('#helpText'));
    const errorTitle =
        nearbyShareHighVisibilityPage.shadowRoot!.querySelector('#errorTitle');
    assert(errorTitle);
    assertEquals(
        nearbyShareHighVisibilityPage.i18n('nearbyShareErrorCantReceive'),
        errorTitle.innerHTML.trim());
    assert(nearbyShareHighVisibilityPage.shadowRoot!.querySelector(
        '#errorDescription'));
  });
});
