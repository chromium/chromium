// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsPasspointSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {CrLinkRowElement, Router, routes} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {MojoConnectivityProvider} from 'chrome://resources/ash/common/connectivity/mojo_connectivity_provider.js';
import {PasspointSubscription} from 'chrome://resources/ash/common/connectivity/passpoint.mojom-webui.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {AppType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CertificateType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {assertEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {FakePasspointService} from 'chrome://webui-test/chromeos/fake_passpoint_service_mojom.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {FakePageHandler} from '../app_management/fake_page_handler.js';
import {setupFakeHandler} from '../app_management/test_util.js';

suite('PasspointSubpage', () => {
  let networkConfigApi_: FakeNetworkConfig;
  let passpointServiceApi_: FakePasspointService;
  let fakeHandler_: FakePageHandler;
  let passpointSubpage_: SettingsPasspointSubpageElement;

  const CA_HASH = 'CAHASH';
  const CA_PEM = 'test-pem';
  const CA_CN = 'Passpoint Example Certificate Authority';

  async function init(sub: PasspointSubscription) {
    const serverCas = [];
    serverCas.push({
      hash: CA_HASH,
      type: CertificateType.kServerCA,
      pemOrId: CA_PEM,
      issuedTo: CA_CN,
      issuedBy: CA_CN,
      availableForNetworkAuth: false,
      hardwareBacked: false,
      deviceWide: false,
    });
    networkConfigApi_.setCertificatesForTest(serverCas, []);
    passpointServiceApi_.addSubscription(sub);
    await flushTasks();

    const params = new URLSearchParams();
    params.append('id', sub.id);
    Router.getInstance().navigateTo(routes.PASSPOINT_DETAIL, params);
    await flushTasks();
  }

  function getListItems(id: string) {
    const div =
        passpointSubpage_.shadowRoot!.querySelector<HTMLDivElement>(`#${id}`);
    assertTrue(!!div);
    return div!.querySelectorAll('div.list-item');
  }

  function getExpirationDateItem(): HTMLDivElement|null {
    return passpointSubpage_.shadowRoot!.querySelector<HTMLDivElement>(
        '#passpointExpirationDate');
  }

  function getSourceText(): string {
    const div = passpointSubpage_.shadowRoot!.querySelector<HTMLDivElement>(
        '#passpointSourceText');
    assertTrue(!!div);
    return div!.textContent!.trim();
  }

  function getCertificateName(): string {
    const div = passpointSubpage_.shadowRoot!.querySelector<HTMLDivElement>(
        '#passpointCertificateName');
    assertTrue(!!div);
    return div!.textContent!.trim();
  }

  function getRemovalDialog(): HTMLDialogElement|null {
    return passpointSubpage_.shadowRoot!.querySelector<HTMLDialogElement>(
        '#removalDialog');
  }

  function getButton(id: string): HTMLButtonElement {
    const element =
        passpointSubpage_.shadowRoot!.querySelector<HTMLButtonElement>(
            `#${id}`);
    assertTrue(!!element);
    return element;
  }

  suiteSetup(() => {
    loadTimeData.overrideValues({
      isPasspointSettingsEnabled: true,
    });
    networkConfigApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
        networkConfigApi_);
    passpointServiceApi_ = new FakePasspointService();
    MojoConnectivityProvider.getInstance().setPasspointServiceForTest(
        passpointServiceApi_);
    fakeHandler_ = setupFakeHandler();
  });

  setup(async () => {
    passpointSubpage_ = document.createElement('settings-passpoint-subpage');
    assert(passpointSubpage_);

    networkConfigApi_.resetForTest();
    passpointServiceApi_.resetForTest();

    document.body.appendChild(passpointSubpage_);
    await waitAfterNextRender(passpointSubpage_);
  });

  teardown(() => {
    passpointSubpage_.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Show Passpoint subscription', async () => {
    const sub = {
      id: 'sub-id',
      domains: ['passpoint.example.com'],
      friendlyName: 'Passpoint Example Ltd.',
      provisioningSource: 'app.passpoint.example.com',
      trustedCa: CA_PEM,
      expirationEpochMs: 0n,
    };
    await init(sub);

    // No app registered, package name should be displayed.
    assertEquals(sub.provisioningSource, getSourceText());
    // Only one domain in the list.
    const list = getListItems('passpointDomainsList');
    assertEquals(1, list.length);
    // No expiration time.
    const item = getExpirationDateItem();
    assertEquals(null, item);
    // Certificate has a common name.
    assertEquals(CA_CN, getCertificateName());
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
          trustedCa: CA_PEM,
          expirationEpochMs: BigInt(date.getTime()),
        };
        await init(sub);

        // No app registered, package name should be displayed.
        assertEquals(sub.provisioningSource, getSourceText());
        // Only one domain in the list.
        const list = getListItems('passpointDomainsList');
        assertEquals(2, list.length);
        // Expiration time is displayed.
        const item = getExpirationDateItem();
        assertTrue(!!item);
        // Certificate has a common name.
        assertEquals(CA_CN, getCertificateName());
      });

  test('Show Passpoint subscription without certificate', async () => {
    const sub = {
      id: 'sub-id',
      domains: ['passpoint.example.com'],
      friendlyName: 'Passpoint Example Ltd.',
      provisioningSource: 'app.passpoint.example.com',
      expirationEpochMs: 0n,
      trustedCa: null,
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
      trustedCa: CA_PEM,
      expirationEpochMs: 0n,
    };
    const appTitle = 'My Passpoint App';
    const app = FakePageHandler.createApp('app_id', {
      type: AppType.kArc,
      title: appTitle,
      publisherId: sub.provisioningSource,
    });
    fakeHandler_!.setApps([app]);
    await init(sub);

    // Only one domain in the list.
    const list = getListItems('passpointDomainsList');
    assertEquals(1, list.length);
    // No expiration time.
    const item = getExpirationDateItem();
    assertEquals(null, item);
    // App name is displayed as subscription source.
    assertEquals(appTitle, getSourceText());
    // Certificate has a common name.
    assertEquals(CA_CN, getCertificateName());
  });

  test('Subscription removal', async () => {
    const subId = 'sub-id';
    const sub = {
      id: subId,
      domains: ['passpoint.example.com'],
      friendlyName: 'Passpoint Example Ltd.',
      provisioningSource: 'app.passpoint.example.com',
      expirationEpochMs: 0n,
      trustedCa: null,
    };
    await init(sub);

    // Trigger a remove.
    const removeButton = getButton('removeButton');
    removeButton.click();
    await waitAfterNextRender(removeButton);
    let dialog = getRemovalDialog();
    assertTrue(!!dialog);

    // Check the dialog message contains the subscription name.
    const link = dialog!.querySelector('localized-link');
    assertTrue(!!link);
    const span = link!.shadowRoot!.querySelector('span');
    assertTrue(!!span);
    assertEquals(
        span.textContent,
        sub.friendlyName +
            ' will be removed from this device only. To make changes to your ' +
            'subscription, contact the subscription provider. ');

    // Cancel the dialog.
    const cancelButton = getButton('removalCancelButton');
    cancelButton.click();
    await waitAfterNextRender(cancelButton);
    dialog = getRemovalDialog();
    assertEquals(null, dialog);

    // Trigger a remove again.
    removeButton.click();
    await waitAfterNextRender(removeButton);
    dialog = getRemovalDialog();
    assertTrue(!!dialog);

    // Confirm the removal.
    const confirmButton = getButton('removalConfirmButton');
    confirmButton.click();
    await waitAfterNextRender(confirmButton);

    // Check the subscription is removed.
    dialog = getRemovalDialog();
    assertEquals(null, dialog);
    const resp = await passpointServiceApi_!.getPasspointSubscription(subId);
    assertEquals(null, resp.result);
  });

  test('Subscription with no associated networks', async () => {
    const subId = 'sub-id';
    const sub = {
      id: subId,
      domains: ['passpoint.example.com'],
      friendlyName: 'Passpoint Example Ltd.',
      provisioningSource: 'app.passpoint.example.com',
      trustedCa: CA_PEM,
      expirationEpochMs: 0n,
    };
    networkConfigApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
    const wifi = OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi');
    networkConfigApi_.addNetworksForTest([wifi]);
    await init(sub);

    const elem = passpointSubpage_.shadowRoot!.querySelector<HTMLDivElement>(
        '#passpointNetworksList');
    assertNull(elem);
  });

  test('Subscription with multiple associated networks', async () => {
    const subId = 'sub-id';
    const sub = {
      id: subId,
      domains: ['passpoint.example.com'],
      friendlyName: 'Passpoint Example Ltd.',
      provisioningSource: 'app.passpoint.example.com',
      trustedCa: CA_PEM,
      expirationEpochMs: 0n,
    };
    networkConfigApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
    const wifi1 = OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi1');
    wifi1.typeState!.wifi!.passpointId = subId;
    const wifi2 = OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi2');
    wifi2.typeState!.wifi!.passpointId = subId;
    networkConfigApi_.addNetworksForTest([
      wifi1,
      wifi2,
    ]);
    await init(sub);

    const list = getListItems('passpointNetworksList');
    assertEquals(2, list.length);
    const row = list[0]!.querySelector<CrLinkRowElement>('cr-link-row');
    assertTrue(!!row);

    const showDetailPromise = eventToPromise('show-detail', window);
    row!.click();
    const showDetailEvent = await showDetailPromise;
    assertEquals('wifi1_guid', showDetailEvent.detail.guid);
  });
});
