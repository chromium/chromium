// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {CellularSetupPageName} from 'chrome://resources/cr_components/chromeos/cellular_setup/cellular_types.m.js';
import {setESimManagerRemoteForTesting} from 'chrome://resources/cr_components/chromeos/cellular_setup/mojo_interface_provider.m.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.js';
import {FakeESimManagerRemote} from 'chrome://test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.m.js';
import {isVisible, waitAfterNextRender} from 'chrome://test/test_util.js';

suite('InternetPage', function() {
  /** @type {?InternetPageElement} */
  let internetPage = null;

  /** @type {?NetworkSummaryElement} */
  let networkSummary_ = null;

  /** @type {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  let mojoApi_ = null;

  /** @type {?ash.cellularSetup.mojom.ESimManagerRemote} */
  let eSimManagerRemote;

  suiteSetup(function() {
    // Disable animations so sub-pages open within one event loop.
    testing.Test.disableAnimationsAndTransitions();
  });

  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  function setNetworksForTest(networks) {
    mojoApi_.resetForTest();
    mojoApi_.addNetworksForTest(networks);
  }

  /**
   * @param {boolean} showPSimFlow
   * @param {boolean} isCellularEnabled
   * @return {!Promise<function()>}
   */
  function navigateToCellularSetupDialog(showPSimFlow, isCellularEnabled) {
    const params = new URLSearchParams();
    params.append('guid', 'cellular_guid');
    params.append('type', 'Cellular');
    params.append('name', 'cellular');
    params.append('showCellularSetup', 'true');
    if (showPSimFlow) {
      params.append('showPsimFlow', 'true');
    }

    // Pretend that we initially started on the INTERNET_NETWORKS route with the
    // params.
    Router.getInstance().navigateTo(routes.INTERNET_NETWORKS, params);
    internetPage.currentRouteChanged(routes.INTERNET_NETWORKS, undefined);

    // Update the device state here to trigger an
    // attemptShowCellularSetupDialog_() call.
    mojoApi_.setNetworkTypeEnabledState(
        chromeos.networkConfig.mojom.NetworkType.kCellular, isCellularEnabled);

    return flushAsync();
  }

  /**
   * @param {DivElement} warningMessage
   */
  async function assertWarningMessageVisibility(warningMessage) {
    assertTrue(!!warningMessage);

    // Warning message should be initially hidden.
    assertTrue(warningMessage.hidden);

    // Add a pSIM network.
    const mojom = chromeos.networkConfig.mojom;
    mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kCellular, true);
    const pSimNetwork = OncMojo.getDefaultManagedProperties(
        mojom.NetworkType.kCellular, 'cellular1');
    pSimNetwork.connectionState = mojom.ConnectionStateType.kConnected;
    mojoApi_.setManagedPropertiesForTest(pSimNetwork);
    await flushAsync();

    // Warning message should now be showing.
    assertFalse(warningMessage.hidden);

    // Disconnect from the pSIM network.
    pSimNetwork.connectionState = mojom.ConnectionStateType.kNotConnected;
    mojoApi_.setManagedPropertiesForTest(pSimNetwork);
    await flushAsync();
    // Warning message should be hidden.
    assertTrue(warningMessage.hidden);

    // Add an eSIM network.
    const eSimNetwork = OncMojo.getDefaultManagedProperties(
        mojom.NetworkType.kCellular, 'cellular2');
    eSimNetwork.connectionState = mojom.ConnectionStateType.kConnected;
    eSimNetwork.typeProperties.cellular.eid = 'eid';
    mojoApi_.setManagedPropertiesForTest(eSimNetwork);
    await flushAsync();

    // Warning message should be showing again.
    assertFalse(warningMessage.hidden);
  }

  async function navigateToCellularDetailPage() {
    await init();

    const mojom = chromeos.networkConfig.mojom;
    const cellularNetwork = OncMojo.getDefaultManagedProperties(
        mojom.NetworkType.kCellular, 'cellular1', 'name1');
    cellularNetwork.typeProperties.cellular.eid = 'eid';
    mojoApi_.setManagedPropertiesForTest(cellularNetwork);

    const params = new URLSearchParams();
    params.append('guid', cellularNetwork.guid);
    Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);
    return flushAsync();
  }

  function init() {
    loadTimeData.overrideValues({
      bypassConnectivityCheck: false,
    });
    internetPage = document.createElement('settings-internet-page');
    assertTrue(!!internetPage);
    mojoApi_.resetForTest();
    document.body.appendChild(internetPage);
    networkSummary_ = internetPage.$$('network-summary');
    assertTrue(!!networkSummary_);
    return flushAsync().then(() => {
      return Promise.all([
        mojoApi_.whenCalled('getNetworkStateList'),
        mojoApi_.whenCalled('getDeviceStateList'),
      ]);
    });
  }

  setup(function() {
    loadTimeData.overrideValues({
      internetAddConnection: 'internetAddConnection',
      internetAddConnectionExpandA11yLabel:
          'internetAddConnectionExpandA11yLabel',
      internetAddConnectionNotAllowed: 'internetAddConnectionNotAllowed',
      internetAddThirdPartyVPN: 'internetAddThirdPartyVPN',
      internetAddVPN: 'internetAddVPN',
      internetAddWiFi: 'internetAddWiFi',
      internetDetailPageTitle: 'internetDetailPageTitle',
      internetKnownNetworksPageTitle: 'internetKnownNetworksPageTitle',
    });

    mojoApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(eSimManagerRemote);

    PolymerTest.clearBody();
  });

  teardown(function() {
    const subPage = internetPage.$$('settings-internet-subpage');
    if (subPage) {
      subPage.remove();
    }
    const detailPage = internetPage.$$('settings-internet-detail-page');
    if (detailPage) {
      detailPage.remove();
    }
    internetPage.remove();
    internetPage = null;
    Router.getInstance().resetRouteForTesting();
  });

  suite('MainPage', function() {
    test('Ethernet', async function() {
      await init();
      // Default fake device state is Ethernet enabled only.
      const ethernet = networkSummary_.$$('#Ethernet');
      assertTrue(!!ethernet);
      assertEquals(1, ethernet.networkStateList.length);
      assertEquals(null, networkSummary_.$$('#Cellular'));
      assertEquals(null, networkSummary_.$$('#VPN'));
      assertEquals(null, networkSummary_.$$('#WiFi'));
    });

    test('WiFi', async function() {
      await init();
      const mojom = chromeos.networkConfig.mojom;
      setNetworksForTest([
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi1'),
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi2'),
      ]);
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kWiFi, true);
      return flushAsync().then(() => {
        const wifi = networkSummary_.$$('#WiFi');
        assertTrue(!!wifi);
        assertEquals(2, wifi.networkStateList.length);
      });
    });

    test('WiFiToggle', async function() {
      await init();
      const mojom = chromeos.networkConfig.mojom;
      // Make WiFi an available but disabled technology.
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kWiFi, false);
      return flushAsync().then(() => {
        const wifi = networkSummary_.$$('#WiFi');
        assertTrue(!!wifi);

        // Ensure that the initial state is disabled and the toggle is
        // enabled but unchecked.
        const wifiDevice =
            mojoApi_.getDeviceStateForTest(mojom.NetworkType.kWiFi);
        assertTrue(!!wifiDevice);
        assertEquals(mojom.DeviceStateType.kDisabled, wifiDevice.deviceState);
        const toggle = wifi.$$('#deviceEnabledButton');
        assertTrue(!!toggle);
        assertFalse(toggle.disabled);
        assertFalse(toggle.checked);

        // Tap the enable toggle button and ensure the state becomes enabling.
        toggle.click();
        return flushAsync().then(() => {
          assertTrue(toggle.checked);
          const wifiDevice =
              mojoApi_.getDeviceStateForTest(mojom.NetworkType.kWiFi);
          assertTrue(!!wifiDevice);
          assertEquals(mojom.DeviceStateType.kEnabling, wifiDevice.deviceState);
        });
      });
    });

    test('Deep link to WiFiToggle', async () => {
      await init();
      const mojom = chromeos.networkConfig.mojom;
      // Make WiFi an available but disabled technology.
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kWiFi, false);

      const params = new URLSearchParams();
      params.append('settingId', '4');
      Router.getInstance().navigateTo(routes.INTERNET, params);

      await flushAsync();

      const deepLinkElement =
          networkSummary_.$$('#WiFi').$$('#deviceEnabledButton');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Toggle WiFi should be focused for settingId=4.');
    });

    suite('VPN', function() {
      test('VpnProviders', async function() {
        await init();
        const mojom = chromeos.networkConfig.mojom;
        mojoApi_.setVpnProvidersForTest([
          {
            type: mojom.VpnType.kExtension,
            providerId: 'extension_id1',
            providerName: 'MyExtensionVPN1',
            appId: 'extension_id1',
            lastLaunchTime: {internalValue: 0},
          },
          {
            type: mojom.VpnType.kArc,
            providerId: 'vpn.app.package1',
            providerName: 'MyArcVPN1',
            appId: 'arcid1',
            lastLaunchTime: {internalValue: 1},
          },
          {
            type: mojom.VpnType.kArc,
            providerId: 'vpn.app.package2',
            providerName: 'MyArcVPN2',
            appId: 'arcid2',
            lastLaunchTime: {internalValue: 2},
          }
        ]);
        return flushAsync().then(() => {
          assertEquals(3, internetPage.vpnProviders_.length);
          // Ensure providers are sorted by type and lastLaunchTime.
          assertEquals(
              'extension_id1', internetPage.vpnProviders_[0].providerId);
          assertEquals(
              'vpn.app.package2', internetPage.vpnProviders_[1].providerId);
          assertEquals(
              'vpn.app.package1', internetPage.vpnProviders_[2].providerId);
        });
      });

      function clickAddConnectionsButton() {
        const button = internetPage.$$('#expandAddConnections');
        assertTrue(!!button);
        button.expanded = true;
      }

      test(
          'should show add VPN button when allow only policy WiFi networks ' +
              'to connect is enabled',
          async function() {
            await init();
            internetPage.globalPolicy_ = {
              allowOnlyPolicyWifiNetworksToConnect: true,
            };
            clickAddConnectionsButton();

            const mojom = chromeos.networkConfig.mojom;
            setNetworksForTest([
              OncMojo.getDefaultNetworkState(mojom.NetworkType.kVPN, 'vpn'),
            ]);
            mojoApi_.setDeviceStateForTest({
              type: mojom.NetworkType.kVPN,
              deviceState: mojom.DeviceStateType.kEnabled
            });

            return flushAsync().then(() => {
              assertTrue(isVisible(internetPage.$$('#add-vpn-label')));
            });
          });

      test(
          'should show VPN policy indicator when VPN is disabled',
          async function() {
            await init();
            clickAddConnectionsButton();

            const mojom = chromeos.networkConfig.mojom;
            setNetworksForTest([
              OncMojo.getDefaultNetworkState(mojom.NetworkType.kVPN, 'vpn'),
            ]);
            mojoApi_.setDeviceStateForTest({
              type: mojom.NetworkType.kVPN,
              deviceState: mojom.DeviceStateType.kProhibited
            });

            return flushAsync().then(() => {
              assertTrue(isVisible(internetPage.$$('#vpnPolicyIndicator')));
              assertTrue(
                  isVisible(networkSummary_.$$('#VPN').$$('#policyIndicator')));
            });
          });

      test(
          'should not show VPN policy indicator when VPN is enabled',
          async function() {
            await init();
            clickAddConnectionsButton();

            const mojom = chromeos.networkConfig.mojom;
            setNetworksForTest([
              OncMojo.getDefaultNetworkState(mojom.NetworkType.kVPN, 'vpn'),
            ]);
            mojoApi_.setDeviceStateForTest({
              type: mojom.NetworkType.kVPN,
              deviceState: mojom.DeviceStateType.kEnabled
            });

            return flushAsync().then(() => {
              assertFalse(isVisible(internetPage.$$('#vpnPolicyIndicator')));
              assertFalse(
                  isVisible(networkSummary_.$$('#VPN').$$('#policyIndicator')));
            });
          });
    });

    test('Deep link to mobile on/off toggle', async () => {
      await init();
      const mojom = chromeos.networkConfig.mojom;
      // Make WiFi an available but disabled technology.
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kCellular, false);

      const params = new URLSearchParams();
      params.append('settingId', '13');
      Router.getInstance().navigateTo(routes.INTERNET, params);

      await flushAsync();

      const deepLinkElement =
          networkSummary_.$$('#Cellular').$$('#deviceEnabledButton');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Toggle mobile on/off should be focused for settingId=13.');
    });

    test('Show rename esim profile dialog', async function() {
      await init();
      eSimManagerRemote.addEuiccForTest(1);
      await flushAsync();

      let renameDialog = internetPage.$$('#esimRenameDialog');
      assertFalse(!!renameDialog);

      const event = new CustomEvent(
          'show-esim-profile-rename-dialog', {detail: {iccid: '1'}});
      internetPage.dispatchEvent(event);

      await flushAsync();
      renameDialog = internetPage.$$('#esimRenameDialog');
      assertTrue(!!renameDialog);

      await assertWarningMessageVisibility(renameDialog.$.warningMessage);
    });

    test('Show remove esim profile dialog', async function() {
      await init();
      eSimManagerRemote.addEuiccForTest(1);
      await flushAsync();

      let removeDialog = internetPage.$$('#esimRemoveProfileDialog');
      assertFalse(!!removeDialog);

      const event = new CustomEvent(
          'show-esim-remove-profile-dialog', {detail: {iccid: '1'}});
      internetPage.dispatchEvent(event);

      await flushAsync();
      removeDialog = internetPage.$$('#esimRemoveProfileDialog');
      assertTrue(!!removeDialog);

      await assertWarningMessageVisibility(removeDialog.$.warningMessage);
    });
  });

  test(
      'Show pSIM flow cellular setup dialog if route params' +
          'contain showCellularSetup and showPsimFlow',
      async function() {
        await init();

        let cellularSetupDialog = internetPage.$$('#cellularSetupDialog');
        assertFalse(!!cellularSetupDialog);

        await navigateToCellularSetupDialog(
            /*showPSimFlow=*/ true, /*isCellularEnabled=*/ true);

        cellularSetupDialog = internetPage.$$('#cellularSetupDialog');
        assertTrue(!!cellularSetupDialog);
        const psimFlow =
            cellularSetupDialog.shadowRoot.querySelector('cellular-setup')
                .shadowRoot.querySelector('#psim-flow-ui');
        assertTrue(!!psimFlow);
      });

  test(
      'Show eSIM flow cellular setup dialog if route params' +
          'contains showCellularSetup, does not contain showPsimFlow,' +
          'connected to a non-cellular network, and cellular enabled',
      async function() {
        await init();
        eSimManagerRemote.addEuiccForTest(1);

        const mojom = chromeos.networkConfig.mojom;
        const wifiNetwork =
            OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi');
        wifiNetwork.connectionState = mojom.ConnectionStateType.kOnline;
        mojoApi_.addNetworksForTest([wifiNetwork]);
        await flushAsync();

        let cellularSetupDialog = internetPage.$$('#cellularSetupDialog');
        assertFalse(!!cellularSetupDialog);

        await navigateToCellularSetupDialog(
            /*showPSimFlow=*/ false, /*isCellularEnabled=*/ true);

        cellularSetupDialog = internetPage.$$('#cellularSetupDialog');
        assertTrue(!!cellularSetupDialog);
        const esimFlow =
            cellularSetupDialog.shadowRoot.querySelector('cellular-setup')
                .shadowRoot.querySelector('#esim-flow-ui');
        assertTrue(!!esimFlow);
      });

  test(
      'Show no connection toast if route params' +
          'contain showCellularSetup, does not contain showPsimFlow,' +
          'cellular is enabled, but not connected to a non-cellular network',
      async function() {
        await init();
        eSimManagerRemote.addEuiccForTest(1);

        assertFalse(!!internetPage.$$('#cellularSetupDialog'));

        await navigateToCellularSetupDialog(
            /*showPSimFlow=*/ false, /*isCellularEnabled=*/ true);

        assertTrue(internetPage.$.errorToast.open);
        assertEquals(
            internetPage.$.errorToastMessage.innerHTML,
            internetPage.i18n('eSimNoConnectionErrorToast'));
        assertFalse(!!internetPage.$$('#cellularSetupDialog'));
      });

  test(
      'Show eSIM flow cellular setup dialog if route params ' +
          'contain showCellularSetup, does not contain showPsimFlow, ' +
          'cellular is enabled, not connected to a non-cellular network ' +
          'but cellular bypass esim installation connectivity flag is enabled',
      async function() {
        await init();
        loadTimeData.overrideValues({
          bypassConnectivityCheck: true,
        });
        eSimManagerRemote.addEuiccForTest(1);

        let cellularSetupDialog = internetPage.$$('#cellularSetupDialog');
        assertFalse(!!cellularSetupDialog);

        await navigateToCellularSetupDialog(
            /*showPSimFlow=*/ false, /*isCellularEnabled=*/ true);

        cellularSetupDialog = internetPage.$$('#cellularSetupDialog');
        assertTrue(!!cellularSetupDialog);
        const esimFlow =
            cellularSetupDialog.shadowRoot.querySelector('cellular-setup')
                .shadowRoot.querySelector('#esim-flow-ui');
        assertTrue(!!esimFlow);
      });

  test(
      'Show mobile data not enabled toast if route params' +
          'contains showCellularSetup, does not contain showPsimFlow,' +
          'connected to a non-cellular network, but cellular not enabled',
      async function() {
        await init();
        eSimManagerRemote.addEuiccForTest(1);

        const mojom = chromeos.networkConfig.mojom;
        const wifiNetwork =
            OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi');
        wifiNetwork.connectionState = mojom.ConnectionStateType.kOnline;
        mojoApi_.addNetworksForTest([wifiNetwork]);
        await flushAsync();

        assertFalse(!!internetPage.$$('#cellularSetupDialog'));

        await navigateToCellularSetupDialog(
            /*showPSimFlow=*/ false, /*isCellularEnabled=*/ false);

        assertTrue(internetPage.$.errorToast.open);
        assertEquals(
            internetPage.$.errorToastMessage.innerHTML,
            internetPage.i18n('eSimMobileDataNotEnabledErrorToast'));
        assertFalse(!!internetPage.$$('#cellularSetupDialog'));
      });

  test(
      'Show profile limit reached toast if route params' +
          'contains showCellularSetup, does not contain showPsimFlow,' +
          'connected to a non-cellular network, cellular enabled,' +
          'but profile limit is reached',
      async function() {
        await init();
        eSimManagerRemote.addEuiccForTest(/*numProfiles=*/ 5);

        const mojom = chromeos.networkConfig.mojom;
        const wifiNetwork =
            OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi');
        wifiNetwork.connectionState = mojom.ConnectionStateType.kOnline;
        mojoApi_.addNetworksForTest([wifiNetwork]);
        await flushAsync();

        const cellularSetupDialog = internetPage.$$('#cellularSetupDialog');
        assertFalse(!!cellularSetupDialog);

        await navigateToCellularSetupDialog(
            /*showPSimFlow=*/ false, /*isCellularEnabled=*/ true);

        assertTrue(internetPage.$.errorToast.open);
        assertEquals(
            internetPage.$.errorToastMessage.innerHTML,
            internetPage.i18n('eSimProfileLimitReachedErrorToast', 5));
        assertFalse(!!internetPage.$$('#cellularSetupDialog'));
      });

  test('Show sim lock dialog through URL parameters', async () => {
    await init();

    const mojom = chromeos.networkConfig.mojom;
    const params = new URLSearchParams();
    params.append(
        'type', OncMojo.getNetworkTypeString(mojom.NetworkType.kCellular));
    params.append('showSimLockDialog', true);

    // Pretend that we initially started on the INTERNET_NETWORKS route with the
    // params.
    Router.getInstance().navigateTo(routes.INTERNET_NETWORKS, params);
    internetPage.currentRouteChanged(routes.INTERNET_NETWORKS, undefined);

    // Update the device state here to trigger an onDeviceStatesChanged_() call.
    mojoApi_.setDeviceStateForTest({
      type: mojom.NetworkType.kCellular,
      deviceState: mojom.DeviceStateType.kEnabled,
      inhibitReason: mojom.InhibitReason.kNotInhibited,
      simLockStatus: {
        lockEnabled: true,
      },
    });
    await flushAsync();

    const simLockDialogs = internetPage.$$('sim-lock-dialogs');
    assertTrue(!!simLockDialogs);
    assertTrue(simLockDialogs.isDialogOpen);
  });

  test(
      'Show no connection toast if receive show-cellular-setup' +
          'event and not connected to non-cellular network',
      async function() {
        await init();
        eSimManagerRemote.addEuiccForTest(/*numProfiles=*/ 1);
        mojoApi_.setNetworkTypeEnabledState(
            chromeos.networkConfig.mojom.NetworkType.kCellular, true);
        await flushAsync();

        assertFalse(internetPage.$.errorToast.open);

        // Send event, toast should show, dialog hidden.
        const event = new CustomEvent(
            'show-cellular-setup',
            {detail: {pageName: CellularSetupPageName.ESIM_FLOW_UI}});
        internetPage.dispatchEvent(event);
        await flushAsync();
        assertTrue(internetPage.$.errorToast.open);
        assertEquals(
            internetPage.$.errorToastMessage.innerHTML,
            internetPage.i18n('eSimNoConnectionErrorToast'));
        assertFalse(!!internetPage.$$('#cellularSetupDialog'));

        // Hide the toast
        internetPage.$.errorToast.hide();
        assertFalse(internetPage.$.errorToast.open);

        // Connect to non-cellular network.
        const mojom = chromeos.networkConfig.mojom;
        const wifiNetwork =
            OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi');
        wifiNetwork.connectionState = mojom.ConnectionStateType.kOnline;
        mojoApi_.addNetworksForTest([wifiNetwork]);
        await flushAsync();

        // Send event, toast should be hidden, dialog open.
        internetPage.dispatchEvent(event);
        await flushAsync();
        assertFalse(internetPage.$.errorToast.open);
        assertTrue(!!internetPage.$$('#cellularSetupDialog'));
      });

  test('Show toast on show-error-toast event', async function() {
    await init();
    assertFalse(internetPage.$.errorToast.open);

    const message = 'Toast message';
    const event = new CustomEvent('show-error-toast', {detail: message});
    internetPage.dispatchEvent(event);
    await flushAsync();
    assertTrue(internetPage.$.errorToast.open);
    assertEquals(internetPage.$.errorToastMessage.innerHTML, message);
  });

  test('Internet detail menu renders', async () => {
    await navigateToCellularDetailPage();

    const internetDetailMenu = internetPage.$$('settings-internet-detail-menu');
    assertTrue(!!internetDetailMenu);
  });

  test(
      'Update global policy when triggering OnPoliciesApplied()',
      async function() {
        await navigateToCellularDetailPage();

        const detailPage = internetPage.$$('settings-internet-detail-page');
        assertTrue(!!detailPage);
        assertTrue(!!detailPage.globalPolicy);
        assertFalse(
            detailPage.globalPolicy.allow_only_policy_cellular_networks);

        // Set global policy should also update the global policy
        const globalPolicy = {
          allow_only_policy_cellular_networks: true,
        };
        mojoApi_.setGlobalPolicy(globalPolicy);
        await flushAsync();

        assertTrue(!!detailPage);
        assertTrue(!!detailPage.globalPolicy);
        assertTrue(detailPage.globalPolicy.allow_only_policy_cellular_networks);
      });

  test(
      'Navigating to Known Networks without network-type parameters ' +
          'defaults to Wi-Fi',
      async function() {
        await init();

        const mojom = chromeos.networkConfig.mojom;
        const params = new URLSearchParams();
        params.append('type', '');

        // Navigate straight to Known Networks while passing in parameters
        // with an empty type.
        Router.getInstance().navigateTo(routes.KNOWN_NETWORKS, params);
        internetPage.currentRouteChanged(routes.KNOWN_NETWORKS, undefined);

        const knownNetworksPage =
            internetPage.$$('settings-internet-known-networks-page');

        // Confirm that the knownNetworkType_ was set to kWiFi.
        assertTrue(!!knownNetworksPage);
        assertEquals(knownNetworksPage.networkType, mojom.NetworkType.kWiFi);
      });

  // TODO(stevenjb): Figure out a way to reliably test navigation. Currently
  // such tests are flaky.
});
