// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/os_settings.js';

import {RegisterReceiveSurfaceResult} from 'chrome://os-settings/mojo/nearby_share.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

suite('NearbyShare', function() {
  let nearbyShareHighVisibilityPage;

  setup(function() {
    PolymerTest.clearBody();

    nearbyShareHighVisibilityPage =
        document.createElement('nearby-share-high-visibility-page');

    document.body.appendChild(nearbyShareHighVisibilityPage);
    flush();
  });

  async function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Renders help text by default', async function() {
    assertTrue(
        !!nearbyShareHighVisibilityPage.shadowRoot.querySelector('#helpText'));
    assertEquals(
        null,
        nearbyShareHighVisibilityPage.shadowRoot.querySelector('#errorTitle'));
    assertEquals(
        null,
        nearbyShareHighVisibilityPage.shadowRoot.querySelector(
            '#errorDescription'));
  });

  test('Handles Timed out', async function() {
    nearbyShareHighVisibilityPage.set('remainingTimeInSeconds_', 0);
    nearbyShareHighVisibilityPage.set('shutoffTimestamp', 1);
    await flushAsync();

    assertEquals(
        null,
        nearbyShareHighVisibilityPage.shadowRoot.querySelector('#helpText'));
    const errorTitle =
        nearbyShareHighVisibilityPage.shadowRoot.querySelector('#errorTitle');
    assertTrue(!!errorTitle);
    assertEquals(
        errorTitle.innerHTML.trim(),
        nearbyShareHighVisibilityPage.i18n('nearbyShareErrorTimeOut'));
    assertTrue(!!nearbyShareHighVisibilityPage.shadowRoot.querySelector(
        '#errorDescription'));
  });

  test('Handles No Connection Medium error', async function() {
    nearbyShareHighVisibilityPage.set(
        'registerResult', RegisterReceiveSurfaceResult['kNoConnectionMedium']);
    await flushAsync();

    assertEquals(
        null,
        nearbyShareHighVisibilityPage.shadowRoot.querySelector('#helpText'));
    const errorTitle =
        nearbyShareHighVisibilityPage.shadowRoot.querySelector('#errorTitle');
    assertTrue(!!errorTitle);
    assertEquals(
        errorTitle.innerHTML.trim(),
        nearbyShareHighVisibilityPage.i18n(
            'nearbyShareErrorNoConnectionMedium'));
    assertTrue(!!nearbyShareHighVisibilityPage.shadowRoot.querySelector(
        '#errorDescription'));
  });

  test('Handles Transfer in Progress error', async function() {
    nearbyShareHighVisibilityPage.set(
        'registerResult', RegisterReceiveSurfaceResult['kTransferInProgress']);
    await flushAsync();

    assertEquals(
        null,
        nearbyShareHighVisibilityPage.shadowRoot.querySelector('#helpText'));
    const errorTitle =
        nearbyShareHighVisibilityPage.shadowRoot.querySelector('#errorTitle');
    assertTrue(!!errorTitle);
    assertEquals(
        errorTitle.innerHTML.trim(),
        nearbyShareHighVisibilityPage.i18n(
            'nearbyShareErrorTransferInProgressTitle'));
    assertTrue(!!nearbyShareHighVisibilityPage.shadowRoot.querySelector(
        '#errorDescription'));
  });

  test('Handles Failure error', async function() {
    nearbyShareHighVisibilityPage.set(
        'registerResult', RegisterReceiveSurfaceResult['kFailure']);
    await flushAsync();

    assertEquals(
        null,
        nearbyShareHighVisibilityPage.shadowRoot.querySelector('#helpText'));
    const errorTitle =
        nearbyShareHighVisibilityPage.shadowRoot.querySelector('#errorTitle');
    assertTrue(!!errorTitle);
    assertEquals(
        errorTitle.innerHTML.trim(),
        nearbyShareHighVisibilityPage.i18n('nearbyShareErrorCantReceive'));
    assertTrue(!!nearbyShareHighVisibilityPage.shadowRoot.querySelector(
        '#errorDescription'));
  });

  test('Handles Nearby process stopped', async function() {
    nearbyShareHighVisibilityPage.set('nearbyProcessStopped', true);
    await flushAsync();

    assertEquals(
        null,
        nearbyShareHighVisibilityPage.shadowRoot.querySelector('#helpText'));
    const errorTitle =
        nearbyShareHighVisibilityPage.shadowRoot.querySelector('#errorTitle');
    assertTrue(!!errorTitle);
    assertEquals(
        errorTitle.innerHTML.trim(),
        nearbyShareHighVisibilityPage.i18n('nearbyShareErrorCantReceive'));
    assertTrue(!!nearbyShareHighVisibilityPage.shadowRoot.querySelector(
        '#errorDescription'));
  });

  test('Handles Start advertising failed', async function() {
    nearbyShareHighVisibilityPage.set('startAdvertisingFailed', true);
    await flushAsync();

    assertEquals(
        null,
        nearbyShareHighVisibilityPage.shadowRoot.querySelector('#helpText'));
    const errorTitle =
        nearbyShareHighVisibilityPage.shadowRoot.querySelector('#errorTitle');
    assertTrue(!!errorTitle);
    assertEquals(
        errorTitle.innerHTML.trim(),
        nearbyShareHighVisibilityPage.i18n('nearbyShareErrorCantReceive'));
    assertTrue(!!nearbyShareHighVisibilityPage.shadowRoot.querySelector(
        '#errorDescription'));
  });
});
