// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';
import 'chrome://os-settings/lazy_load.js';

import {Router, routes} from 'chrome://os-settings/os_settings.js';
import {CellularSetupPageName} from 'chrome://resources/ash/common/cellular_setup/cellular_types.js';
import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import {MojoConnectivityProvider} from 'chrome://resources/ash/common/connectivity/mojo_connectivity_provider.js';
import {setHotspotConfigForTesting} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.js';
import {HotspotAllowStatus, HotspotState} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.mojom-webui.js';
import {FakeHotspotConfig} from 'chrome://resources/ash/common/hotspot/fake_hotspot_config.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {ESimManagerRemote} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {CrosNetworkConfigRemote, InhibitReason, MAX_NUM_CUSTOM_APNS, VpnType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, DeviceStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {FakePasspointService} from 'chrome://webui-test/chromeos/fake_passpoint_service_mojom.js';
import {FakeESimManagerRemote} from 'chrome://webui-test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.js';
import {waitAfterNextRender, waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

suite('InternetPage', function() {
  /** @type {?InternetPageElement} */
  let internetPage = null;

  /** @type {?NetworkSummaryElement} */
  let networkSummary_ = null;

  /** @type {?CrosNetworkConfigRemote} */
  let mojoApi_ = null;

  /** @type {?ESimManagerRemote} */
  let eSimManagerRemote;

  /** @type {PasspointServiceInterface} */
  let passpointService_ = null;

  /** @type {?CrosHotspotConfigInterface} */
  let hotspotConfig = null;

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
        NetworkType.kCellular, isCellularEnabled);

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
    mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, true);
    const pSimNetwork =
        OncMojo.getDefaultManagedProperties(NetworkType.kCellular, 'cellular1');
    pSimNetwork.connectionState = ConnectionStateType.kConnected;
    mojoApi_.setManagedPropertiesForTest(pSimNetwork);
    await flushAsync();

    // Warning message should now be showing.
    assertFalse(warningMessage.hidden);

    // Disconnect from the pSIM network.
    pSimNetwork.connectionState = ConnectionStateType.kNotConnected;
    mojoApi_.setManagedPropertiesForTest(pSimNetwork);
    await flushAsync();
    // Warning message should be hidden.
    assertTrue(warningMessage.hidden);

    // Add an eSIM network.
    const eSimNetwork =
        OncMojo.getDefaultManagedProperties(NetworkType.kCellular, 'cellular2');
    eSimNetwork.connectionState = ConnectionStateType.kConnected;
    eSimNetwork.typeProperties.cellular.eid = 'eid';
    mojoApi_.setManagedPropertiesForTest(eSimNetwork);
    await flushAsync();

    // Warning message should be showing again.
    assertFalse(warningMessage.hidden);
  }

  async function navigateToCellularDetailPage() {
    await init();

    const cellularNetwork = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular1', 'name1');
    cellularNetwork.typeProperties.cellular.eid = 'eid';
    mojoApi_.setManagedPropertiesForTest(cellularNetwork);

    const params = new URLSearchParams();
    params.append('guid', cellularNetwork.guid);
    Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);
    return flushAsync();
  }

  async function navigateToApnSubpage() {
    await navigateToCellularDetailPage();
    internetPage.shadowRoot.querySelector('settings-internet-detail-subpage')
        .shadowRoot.querySelector('#apnSubpageButton')
        .click();
    return await flushAsync();
  }

  function init() {
    loadTimeData.overrideValues({
      bypassConnectivityCheck: false,
    });
    internetPage = document.createElement('settings-internet-page');
    assertTrue(!!internetPage);
    mojoApi_.resetForTest();
    document.body.appendChild(internetPage);
    networkSummary_ = internetPage.shadowRoot.querySelector('network-summary');
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
      isApnRevampEnabled: false,
    });

    mojoApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(eSimManagerRemote);
    passpointService_ = new FakePasspointService();
    MojoConnectivityProvider.getInstance().setPasspointServiceForTest(
        passpointService_);
    hotspotConfig = new FakeHotspotConfig();
    setHotspotConfigForTesting(hotspotConfig);

    PolymerTest.clearBody();
  });

  teardown(function() {
    const subPage =
        internetPage.shadowRoot.querySelector('settings-internet-subpage');
    if (subPage) {
      subPage.remove();
    }
    const detailPage = internetPage.shadowRoot.querySelector(
        'settings-internet-detail-subpage');
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
      const ethernet = networkSummary_.shadowRoot.querySelector('#Ethernet');
      assertTrue(!!ethernet);
      assertEquals(1, ethernet.networkStateList.length);
      assertEquals(null, networkSummary_.shadowRoot.querySelector('#Cellular'));
      assertEquals(null, networkSummary_.shadowRoot.querySelector('#VPN'));
      assertEquals(null, networkSummary_.shadowRoot.querySelector('#WiFi'));
    });

    test('WiFi', async function() {
      await init();
      setNetworksForTest([
        OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi1'),
        OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi2'),
      ]);
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      return flushAsync().then(() => {
        const wifi = networkSummary_.shadowRoot.querySelector('#WiFi');
        assertTrue(!!wifi);
        assertEquals(2, wifi.networkStateList.length);
      });
    });

    test('WiFiToggle', async function() {
      await init();
      // Make WiFi an available but disabled technology.
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, false);
      return flushAsync().then(() => {
        const wifi = networkSummary_.shadowRoot.querySelector('#WiFi');
        assertTrue(!!wifi);

        // Ensure that the initial state is disabled and the toggle is
        // enabled but unchecked.
        const wifiDevice = mojoApi_.getDeviceStateForTest(NetworkType.kWiFi);
        assertTrue(!!wifiDevice);
        assertEquals(DeviceStateType.kDisabled, wifiDevice.deviceState);
        const toggle = wifi.shadowRoot.querySelector('#deviceEnabledButton');
        assertTrue(!!toggle);
        assertFalse(toggle.disabled);
        assertFalse(toggle.checked);

        // Tap the enable toggle button and ensure the state becomes enabling.
        toggle.click();
        return flushAsync().then(() => {
          assertTrue(toggle.checked);
          const wifiDevice = mojoApi_.getDeviceStateForTest(NetworkType.kWiFi);
          assertTrue(!!wifiDevice);
          assertEquals(DeviceStateType.kEnabling, wifiDevice.deviceState);
        });
      });
    });

    test('Deep link to WiFiToggle', async () => {
      await init();
      // Make WiFi an available but disabled technology.
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, false);

      const params = new URLSearchParams();
      params.append('settingId', '4');
      Router.getInstance().navigateTo(routes.INTERNET, params);

      await flushAsync();

      const deepLinkElement =
          networkSummary_.shadowRoot.querySelector('#WiFi')
              .shadowRoot.querySelector('#deviceEnabledButton');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Toggle WiFi should be focused for settingId=4.');
    });

    suite('VPN', function() {
      test('VpnProviders', async function() {
        await init();
        mojoApi_.setVpnProvidersForTest([
          {
            type: VpnType.kExtension,
            providerId: 'extension_id1',
            providerName: 'MyExtensionVPN1',
            appId: 'extension_id1',
            lastLaunchTime: {internalValue: 0},
          },
          {
            type: VpnType.kArc,
            providerId: 'vpn.app.package1',
            providerName: 'MyArcVPN1',
            appId: 'arcid1',
            lastLaunchTime: {internalValue: 1},
          },
          {
            type: VpnType.kArc,
            providerId: 'vpn.app.package2',
            providerName: 'MyArcVPN2',
            appId: 'arcid2',
            lastLaunchTime: {internalValue: 2},
          },
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
        const button =
            internetPage.shadowRoot.querySelector('#expandAddConnections');
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

            setNetworksForTest([
              OncMojo.getDefaultNetworkState(NetworkType.kVPN, 'vpn'),
            ]);
            mojoApi_.setDeviceStateForTest({
              type: NetworkType.kVPN,
              deviceState: DeviceStateType.kEnabled,
            });

            return flushAsync().then(() => {
              assertTrue(isVisible(
                  internetPage.shadowRoot.querySelector('#add-vpn-label')));
            });
          });

      test(
          'should show VPN policy indicator when VPN is disabled',
          async function() {
            await init();
            clickAddConnectionsButton();

            setNetworksForTest([
              OncMojo.getDefaultNetworkState(NetworkType.kVPN, 'vpn'),
            ]);
            mojoApi_.setDeviceStateForTest({
              type: NetworkType.kVPN,
              deviceState: DeviceStateType.kProhibited,
            });

            return flushAsync().then(() => {
              assertTrue(isVisible(internetPage.shadowRoot.querySelector(
                  '#vpnPolicyIndicator')));
              assertTrue(
                  isVisible(networkSummary_.shadowRoot.querySelector('#VPN')
                                .shadowRoot.querySelector('#policyIndicator')));
            });
          });

      test(
          'should not show VPN policy indicator when VPN is enabled',
          async function() {
            await init();
            clickAddConnectionsButton();

            setNetworksForTest([
              OncMojo.getDefaultNetworkState(NetworkType.kVPN, 'vpn'),
            ]);
            mojoApi_.setDeviceStateForTest({
              type: NetworkType.kVPN,
              deviceState: DeviceStateType.kEnabled,
            });

            return flushAsync().then(() => {
              assertFalse(isVisible(internetPage.shadowRoot.querySelector(
                  '#vpnPolicyIndicator')));
              assertFalse(
                  isVisible(networkSummary_.shadowRoot.querySelector('#VPN')
                                .shadowRoot.querySelector('#policyIndicator')));
            });
          });
    });

    test('Deep link to mobile on/off toggle', async () => {
      await init();
      // Make WiFi an available but disabled technology.
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, false);

      const params = new URLSearchParams();
      params.append('settingId', '13');
      Router.getInstance().navigateTo(routes.INTERNET, params);

      await flushAsync();

      const deepLinkElement =
          networkSummary_.shadowRoot.querySelector('#Cellular')
              .shadowRoot.querySelector('#deviceEnabledButton');
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

      let renameDialog =
          internetPage.shadowRoot.querySelector('#esimRenameDialog');
      assertFalse(!!renameDialog);

      const event = new CustomEvent(
          'show-esim-profile-rename-dialog', {detail: {iccid: '1'}});
      internetPage.dispatchEvent(event);

      await flushAsync();
      renameDialog = internetPage.shadowRoot.querySelector('#esimRenameDialog');
      assertTrue(!!renameDialog);

      await assertWarningMessageVisibility(renameDialog.$.warningMessage);
    });

    test('Show remove esim profile dialog', async function() {
      await init();
      eSimManagerRemote.addEuiccForTest(1);
      await flushAsync();

      let removeDialog =
          internetPage.shadowRoot.querySelector('#esimRemoveProfileDialog');
      assertFalse(!!removeDialog);

      const event = new CustomEvent(
          'show-esim-remove-profile-dialog', {detail: {iccid: '1'}});
      internetPage.dispatchEvent(event);

      await flushAsync();
      removeDialog =
          internetPage.shadowRoot.querySelector('#esimRemoveProfileDialog');
      assertTrue(!!removeDialog);

      await assertWarningMessageVisibility(removeDialog.$.warningMessage);
    });
  });

  test(
      'Show pSIM flow cellular setup dialog if route params' +
          'contain showCellularSetup and showPsimFlow',
      async function() {
        await init();

        let cellularSetupDialog =
            internetPage.shadowRoot.querySelector('#cellularSetupDialog');
        assertFalse(!!cellularSetupDialog);

        await navigateToCellularSetupDialog(
            /*showPSimFlow=*/ true, /*isCellularEnabled=*/ true);

        cellularSetupDialog =
            internetPage.shadowRoot.querySelector('#cellularSetupDialog');
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

        const wifiNetwork =
            OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi');
        wifiNetwork.connectionState = ConnectionStateType.kOnline;
        mojoApi_.addNetworksForTest([wifiNetwork]);
        await flushAsync();

        let cellularSetupDialog =
            internetPage.shadowRoot.querySelector('#cellularSetupDialog');
        assertFalse(!!cellularSetupDialog);

        await navigateToCellularSetupDialog(
            /*showPSimFlow=*/ false, /*isCellularEnabled=*/ true);

        cellularSetupDialog =
            internetPage.shadowRoot.querySelector('#cellularSetupDialog');
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

        assertFalse(
            !!internetPage.shadowRoot.querySelector('#cellularSetupDialog'));

        await navigateToCellularSetupDialog(
            /*showPSimFlow=*/ false, /*isCellularEnabled=*/ true);

        assertTrue(internetPage.$.errorToast.open);
        assertEquals(
            internetPage.$.errorToastMessage.innerHTML,
            internetPage.i18n('eSimNoConnectionErrorToast'));
        assertFalse(
            !!internetPage.shadowRoot.querySelector('#cellularSetupDialog'));
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

        let cellularSetupDialog =
            internetPage.shadowRoot.querySelector('#cellularSetupDialog');
        assertFalse(!!cellularSetupDialog);

        await navigateToCellularSetupDialog(
            /*showPSimFlow=*/ false, /*isCellularEnabled=*/ true);

        cellularSetupDialog =
            internetPage.shadowRoot.querySelector('#cellularSetupDialog');
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

        const wifiNetwork =
            OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi');
        wifiNetwork.connectionState = ConnectionStateType.kOnline;
        mojoApi_.addNetworksForTest([wifiNetwork]);
        await flushAsync();

        assertFalse(
            !!internetPage.shadowRoot.querySelector('#cellularSetupDialog'));

        await navigateToCellularSetupDialog(
            /*showPSimFlow=*/ false, /*isCellularEnabled=*/ false);

        assertTrue(internetPage.$.errorToast.open);
        assertEquals(
            internetPage.$.errorToastMessage.innerHTML,
            internetPage.i18n('eSimMobileDataNotEnabledErrorToast'));
        assertFalse(
            !!internetPage.shadowRoot.querySelector('#cellularSetupDialog'));
      });

  test(
      'Show profile limit reached toast if route params' +
          'contains showCellularSetup, does not contain showPsimFlow,' +
          'connected to a non-cellular network, cellular enabled,' +
          'but profile limit is reached',
      async function() {
        await init();
        eSimManagerRemote.addEuiccForTest(/*numProfiles=*/ 5);

        const wifiNetwork =
            OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi');
        wifiNetwork.connectionState = ConnectionStateType.kOnline;
        mojoApi_.addNetworksForTest([wifiNetwork]);
        await flushAsync();

        const cellularSetupDialog =
            internetPage.shadowRoot.querySelector('#cellularSetupDialog');
        assertFalse(!!cellularSetupDialog);

        await navigateToCellularSetupDialog(
            /*showPSimFlow=*/ false, /*isCellularEnabled=*/ true);

        assertTrue(internetPage.$.errorToast.open);
        assertEquals(
            internetPage.$.errorToastMessage.innerHTML,
            internetPage.i18n('eSimProfileLimitReachedErrorToast', 5));
        assertFalse(
            !!internetPage.shadowRoot.querySelector('#cellularSetupDialog'));
      });

  test('Show sim lock dialog through URL parameters', async () => {
    await init();

    const params = new URLSearchParams();
    params.append('type', OncMojo.getNetworkTypeString(NetworkType.kCellular));
    params.append('showSimLockDialog', true);

    // Pretend that we initially started on the INTERNET_NETWORKS route with the
    // params.
    Router.getInstance().navigateTo(routes.INTERNET_NETWORKS, params);
    internetPage.currentRouteChanged(routes.INTERNET_NETWORKS, undefined);

    // Update the device state here to trigger an onDeviceStatesChanged_() call.
    mojoApi_.setDeviceStateForTest({
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason: InhibitReason.kNotInhibited,
      simLockStatus: {
        lockEnabled: true,
      },
    });
    await flushAsync();

    const simLockDialogs =
        internetPage.shadowRoot.querySelector('sim-lock-dialogs');
    assertTrue(!!simLockDialogs);
    assertTrue(simLockDialogs.isDialogOpen);
  });

  test(
      'Show no connection toast if receive show-cellular-setup' +
          'event and not connected to non-cellular network',
      async function() {
        await init();
        eSimManagerRemote.addEuiccForTest(/*numProfiles=*/ 1);
        mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, true);
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
        assertFalse(
            !!internetPage.shadowRoot.querySelector('#cellularSetupDialog'));

        // Hide the toast
        internetPage.$.errorToast.hide();
        assertFalse(internetPage.$.errorToast.open);

        // Connect to non-cellular network.
        const wifiNetwork =
            OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi');
        wifiNetwork.connectionState = ConnectionStateType.kOnline;
        mojoApi_.addNetworksForTest([wifiNetwork]);
        await flushAsync();

        // Send event, toast should be hidden, dialog open.
        internetPage.dispatchEvent(event);
        await flushAsync();
        assertFalse(internetPage.$.errorToast.open);
        assertTrue(
            !!internetPage.shadowRoot.querySelector('#cellularSetupDialog'));
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

    const internetDetailMenu =
        internetPage.shadowRoot.querySelector('settings-internet-detail-menu');
    assertTrue(!!internetDetailMenu);
  });

  test(
      'Update global policy when triggering OnPoliciesApplied()',
      async function() {
        await navigateToCellularDetailPage();

        const detailPage = internetPage.shadowRoot.querySelector(
            'settings-internet-detail-subpage');
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

        const params = new URLSearchParams();
        params.append('type', '');

        // Navigate straight to Known Networks while passing in parameters
        // with an empty type.
        Router.getInstance().navigateTo(routes.KNOWN_NETWORKS, params);
        internetPage.currentRouteChanged(routes.KNOWN_NETWORKS, undefined);

        const knownNetworksPage = internetPage.shadowRoot.querySelector(
            'settings-internet-known-networks-subpage');

        // Confirm that the knownNetworkType_ was set to kWiFi.
        assertTrue(!!knownNetworksPage);
        assertEquals(knownNetworksPage.networkType, NetworkType.kWiFi);
      });

  test('Navigate to/from APN subpage', async function() {
    loadTimeData.overrideValues({isApnRevampEnabled: true});
    await navigateToApnSubpage();
    assertEquals(Router.getInstance().currentRoute, routes.APN);
    assertTrue(!!internetPage.shadowRoot.querySelector('apn-subpage'));

    const windowPopstatePromise = eventToPromise('popstate', window);
    Router.getInstance().navigateToPreviousRoute();
    await windowPopstatePromise;
    await waitBeforeNextRender(internetPage);
    const detailPage = internetPage.shadowRoot.querySelector(
        'settings-internet-detail-subpage');
    await flushAsync();

    assertEquals(
        detailPage.shadowRoot.querySelector('#apnSubpageButton'),
        detailPage.shadowRoot.activeElement,
        'Apn subpage row should be focused');
  });

  test(
      'Create apn button opens dialogs and clicking cancel button removes it',
      async function() {
        loadTimeData.overrideValues({isApnRevampEnabled: true});
        await navigateToApnSubpage();
        const subpage = internetPage.shadowRoot.querySelector('apn-subpage');
        assertTrue(!!subpage);
        const apnList = subpage.shadowRoot.querySelector('apn-list');
        assertTrue(!!apnList);
        const getApnDetailDialog = () =>
            apnList.shadowRoot.querySelector('apn-detail-dialog');

        assertFalse(!!getApnDetailDialog());
        const createCustomApnButton =
            internetPage.shadowRoot.querySelector('#createCustomApnButton');
        assertTrue(!!createCustomApnButton);
        createCustomApnButton.click();
        await flushAsync();

        assertTrue(!!getApnDetailDialog());
        const onCloseEventPromise = eventToPromise('close', apnList);
        const cancelBtn = getApnDetailDialog().shadowRoot.querySelector(
            '#apnDetailCancelBtn');
        cancelBtn.click();
        await onCloseEventPromise;

        assertFalse(!!getApnDetailDialog());
      });

  test(
      'Navigate to APN subpage and remove cellular properties.',
      async function() {
        loadTimeData.overrideValues({isApnRevampEnabled: true});
        await navigateToApnSubpage();
        assertEquals(Router.getInstance().currentRoute, routes.APN);
        assertTrue(!!internetPage.shadowRoot.querySelector('apn-subpage'));
        // We use the same guid as in navigateToCellularDetailPage so that
        // we trigger onNetworkStateChanged
        const network = OncMojo.getDefaultManagedProperties(
            NetworkType.kWiFi, 'cellular1', 'name1');
        const windowPopstatePromise = eventToPromise('popstate', window);
        mojoApi_.setManagedPropertiesForTest(network);
        await windowPopstatePromise;
        await waitBeforeNextRender(internetPage);
        // Because there were no cellular properties we call apn_subpage close
        // which navigates to the previous page.
        assertEquals(Router.getInstance().currentRoute, routes.NETWORK_DETAIL);
      });

  test(
      'Navigate to APN subpage without providing guid as parameter',
      async function() {
        loadTimeData.overrideValues({isApnRevampEnabled: true});
        await navigateToCellularDetailPage();
        const windowPopstatePromise = eventToPromise('popstate', window);
        Router.getInstance().navigateTo(routes.APN);
        await windowPopstatePromise;
        await waitBeforeNextRender(internetPage);
        assertNotEquals(Router.getInstance().currentRoute, routes.APN);
      });

  test(
      'Disable and show tooltip for New APN button when custom APNs limit is' +
          'reached',
      async function() {
        loadTimeData.overrideValues({isApnRevampEnabled: true});
        await navigateToApnSubpage();
        const getApnButton = () =>
            internetPage.shadowRoot.querySelector('#createCustomApnButton');
        const getApnTooltip = () =>
            internetPage.shadowRoot.querySelector('#apnTooltip');

        assertTrue(!!getApnButton());
        assertFalse(!!getApnTooltip());
        assertFalse(getApnButton().disabled);

        let properties = OncMojo.getDefaultManagedProperties(
            NetworkType.kCellular, 'cellular1', 'cellular');
        // We're setting the list of APNs to the max number
        properties.typeProperties.cellular = {
          customApnList:
              Array.apply(null, {length: MAX_NUM_CUSTOM_APNS}).map(_ => {
                return {
                  accessPointName: 'apn',
                };
              }),
        };
        mojoApi_.setManagedPropertiesForTest(properties);
        await flushAsync();

        assertTrue(!!getApnTooltip());
        assertTrue(getApnButton().disabled);
        assertTrue(getApnTooltip().innerHTML.includes(
            internetPage.i18n('customApnLimitReached')));

        properties = OncMojo.getDefaultManagedProperties(
            NetworkType.kCellular, 'cellular1', 'cellular');
        properties.typeProperties.cellular = {
          customApnList: [],
        };
        mojoApi_.setManagedPropertiesForTest(properties);
        await flushAsync();
        assertFalse(!!getApnTooltip());
        assertFalse(getApnButton().disabled);
      });

  test('Nagivate to Passpoint detail page', async () => {
    const subId = 'a_passpoint_id';
    const sub = {
      id: subId,
      domains: ['passpoint.example.com'],
      friendlyName: 'Passpoint Example Ltd.',
      provisioningSource: 'app.passpoint.example.com',
      trustedCa: '',
      expirationEpochMs: 0n,
    };
    passpointService_.addSubscription(sub);
    await init();

    const params = new URLSearchParams();
    params.append('id', subId);

    // Navigate straight to Passpoint detail subpage.
    Router.getInstance().navigateTo(routes.PASSPOINT_DETAIL, params);
    internetPage.currentRouteChanged(routes.PASSPOINT_DETAIL, undefined);

    const passpointDetailPage =
        internetPage.shadowRoot.querySelector('settings-passpoint-subpage');
    assertTrue(!!passpointDetailPage);
  });

  test('Show spinner on hotspot subpage when enabling', async () => {
    loadTimeData.overrideValues({isHotspotEnabled: true});

    const hotspotInfo = {
      state: HotspotState.kDisabled,
      allowStatus: HotspotAllowStatus.kAllowed,
      clientCount: 0,
      config: {
        ssid: 'test_ssid',
        passphrase: 'test_passphrase',
      },
    };
    hotspotConfig.setFakeHotspotInfo(hotspotInfo);
    await init();

    Router.getInstance().navigateTo(routes.HOTSPOT_DETAIL);
    await flushAsync();

    const hotspotDetailPage =
        internetPage.shadowRoot.querySelector('settings-hotspot-subpage');
    assertTrue(!!hotspotDetailPage);

    const hotspotSubpage =
        internetPage.shadowRoot.querySelector('#hotspotSubpage');
    assertTrue(!!hotspotSubpage);
    assertFalse(hotspotSubpage.showSpinner);

    hotspotConfig.setFakeHotspotState(HotspotState.kEnabling);
    await flushAsync();
    assertTrue(hotspotSubpage.showSpinner);

    hotspotConfig.setFakeHotspotState(HotspotState.kDisabling);
    await flushAsync();
    assertTrue(hotspotSubpage.showSpinner);
  });

  // TODO(stevenjb): Figure out a way to reliably test navigation. Currently
  // such tests are flaky.
});
