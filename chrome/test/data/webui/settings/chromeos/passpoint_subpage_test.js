// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {MojoConnectivityProvider} from 'chrome://resources/ash/common/connectivity/mojo_connectivity_provider.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {AppType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {FakePasspointService} from 'chrome://webui-test/chromeos/fake_passpoint_service_mojom.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {setupFakeHandler} from './app_management/test_util.js';

suite('PasspointSubpage', () => {
  /** @type {?FakeNetworkConfig} */
  let networkConfigApi_ = null;
  /** @type {?FakePasspointService} */
  let passpointServiceApi_ = null;
  /** @type {?FakePageHandler} */
  let fakeHandler = null;
  /** @type {?SettingPasspointSubpageElement} */
  let passpointSubpage_ = null;

  const kCaHash = 'CAHASH';
  const kCaPem = 'test-pem';
  const kCaCN = 'Passpoint Example Certificate Authority';

  /**
   * @return {!Promise<unknown>}
   * @private
   */
  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  /**
   * @param {!PasspointSubscription} sub
   * @return {!Promise<unknown>}
   */
  async function init(sub) {
    const serverCas = [];
    serverCas.push({
      hash: kCaHash,
      pemOrId: kCaPem,
      issuedTo: kCaCN,
    });
    networkConfigApi_.setCertificatesForTest(serverCas, []);
    passpointServiceApi_.addSubscription(sub);
    await waitAfterNextRender(passpointSubpage_);

    const params = new URLSearchParams();
    params.append('id', sub.id);
    Router.getInstance().navigateTo(routes.PASSPOINT_DETAIL, params);
    return flushAsync();
  }

  function getDomainsListItems() {
    const domains =
        passpointSubpage_.shadowRoot.querySelector('#passpointDomainsList');
    assertTrue(!!domains);
    return domains.querySelectorAll('div.list-item');
  }

  function getExpirationDateItem() {
    return passpointSubpage_.shadowRoot.querySelector(
        '#passpointExpirationDate');
  }

  function getSourceText() {
    const div =
        passpointSubpage_.shadowRoot.querySelector('#passpointSourceText');
    assertTrue(!!div);
    return div.textContent.trim();
  }

  function getCertificateName() {
    const div =
        passpointSubpage_.shadowRoot.querySelector('#passpointCertificateName');
    assertTrue(!!div);
    return div.textContent.trim();
  }

  function getElement(id) {
    const element = passpointSubpage_.shadowRoot.querySelector(`#${id}`);
    assertTrue(!!element);
    return element;
  }

  function getRemovalDialog() {
    return passpointSubpage_.shadowRoot.querySelector('#removalDialog');
  }

  suiteSetup(() => {
    loadTimeData.overrideValues({
      isPasspointEnabled: true,
      isPasspointSettingsEnabled: true,
    });
    networkConfigApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().remote_ = networkConfigApi_;
    passpointServiceApi_ = new FakePasspointService();
    MojoConnectivityProvider.getInstance().setPasspointServiceForTest(
        passpointServiceApi_);
    fakeHandler = setupFakeHandler();

    // Disable animations so sub-pages open within one event loop.
    testing.Test.disableAnimationsAndTransitions();
  });

  setup(async () => {
    PolymerTest.clearBody();
    passpointSubpage_ = document.createElement('settings-passpoint-subpage');
    assert(passpointSubpage_);

    networkConfigApi_.resetForTest();
    passpointServiceApi_.resetForTest();

    document.body.appendChild(passpointSubpage_);
    await waitAfterNextRender(passpointSubpage_);
  });

  teardown(function() {
    passpointSubpage_.remove();
    passpointSubpage_ = null;
    Router.getInstance().resetRouteForTesting();
  });

  test('Show Passpoint subscription', async () => {
    const sub = {
      id: 'sub-id',
      domains: ['passpoint.example.com'],
      friendlyName: 'Passpoint Example Ltd.',
      provisioningSource: 'app.passpoint.example.com',
      trustedCa: kCaPem,
      expirationEpochMs: 0n,
    };
    await init(sub);

    // No app registered, package name should be displayed.
    assertEquals(sub.provisioningSource, getSourceText());
    // Only one domain in the list.
    const list = getDomainsListItems();
    assertEquals(1, list.length);
    // No expiration time.
    const item = getExpirationDateItem();
    assertFalse(!!item);
    // Certificate has a common name.
    assertEquals(kCaCN, getCertificateName());
  });

  test(
      'Show Passpoint subscription with domains and expiration time',
      async () => {
        // Create a date 7 days ahead of now.
        const date = new Date(Date.now() + 7 * 24 * 60 * 60 * 1000);

        const sub = {
          id: 'sub-id',
          domains: ['passpoint.example.com', 'passpoint2.example.com'],
          friendlyName: 'Passpoint Example Ltd.',
          provisioningSource: 'app.passpoint.example.com',
          trustedCa: kCaPem,
          expirationEpochMs: BigInt(date.getTime()),
        };
        await init(sub);

        // No app registered, package name should be displayed.
        assertEquals(sub.provisioningSource, getSourceText());
        // Only one domain in the list.
        const list = getDomainsListItems();
        assertEquals(2, list.length);
        // Expiration time is displayed.
        const item = getExpirationDateItem();
        assertTrue(!!item);
        // Certificate has a common name.
        assertEquals(kCaCN, getCertificateName());
      });

  test('Show Passpoint subscription without certificate', async () => {
    const sub = {
      id: 'sub-id',
      domains: ['passpoint.example.com'],
      friendlyName: 'Passpoint Example Ltd.',
      provisioningSource: 'app.passpoint.example.com',
      expirationEpochMs: 0n,
    };
    await init(sub);

    // Certificate has a common name.
    assertEquals(
        passpointSubpage_.i18n('passpointSystemCALabel'), getCertificateName());
  });

  test('Show Passpoint subscription with app', async () => {
    const sub = {
      id: 'sub-id',
      domains: ['passpoint.example.com'],
      friendlyName: 'Passpoint Example Ltd.',
      provisioningSource: 'app.passpoint.example.com',
      trustedCa: kCaPem,
      expirationEpochMs: 0n,
    };
    const appTitle = 'My Passpoint App';
    const app = {
      type: AppType.kArc,
      title: appTitle,
      publisherId: sub.provisioningSource,
    };
    fakeHandler.setApps([app]);
    await init(sub);

    // Only one domain in the list.
    const list = getDomainsListItems();
    assertEquals(1, list.length);
    // No expiration time.
    const item = getExpirationDateItem();
    assertFalse(!!item);
    // App name is displayed as subscription source.
    assertEquals(appTitle, getSourceText());
    // Certificate has a common name.
    assertEquals(kCaCN, getCertificateName());
  });

  test('Subscription removal', async () => {
    const subId = 'sub-id';
    const sub = {
      id: subId,
      domains: ['passpoint.example.com'],
      friendlyName: 'Passpoint Example Ltd.',
      provisioningSource: 'app.passpoint.example.com',
      expirationEpochMs: 0n,
    };
    await init(sub);

    // Trigger a remove.
    const removeButton = getElement('removeButton');
    removeButton.click();
    await waitAfterNextRender(removeButton);
    let dialog = getRemovalDialog();
    assertTrue(!!dialog);

    // Cancel the dialog.
    const cancelButton = getElement('removalCancelButton');
    cancelButton.click();
    await waitAfterNextRender(cancelButton);
    dialog = getRemovalDialog();
    assertFalse(!!dialog);

    // Trigger a remove again.
    removeButton.click();
    await waitAfterNextRender(removeButton);
    dialog = getRemovalDialog();
    assertTrue(!!dialog);

    // Confirm the removal.
    const confirmButton = getElement('removalConfirmButton');
    confirmButton.click();
    await waitAfterNextRender(confirmButton);

    // Check the subscription is removed.
    dialog = getRemovalDialog();
    assertFalse(!!dialog);
    const resp = await passpointServiceApi_.getPasspointSubscription(subId);
    assertEquals(null, resp.result);
  });
});
