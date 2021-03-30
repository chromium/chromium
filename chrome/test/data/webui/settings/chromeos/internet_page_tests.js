// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.m.js';
// #import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// #import {setESimManagerRemoteForTesting} from 'chrome://resources/cr_components/chromeos/cellular_setup/mojo_interface_provider.m.js';
// #import {FakeESimManagerRemote} from 'chrome://test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.m.js';
// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// #import {CellularSetupPageName} from 'chrome://resources/cr_components/chromeos/cellular_setup/cellular_types.m.js';
// #import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {isVisible, waitAfterNextRender} from 'chrome://test/test_util.m.js';
// clang-format on

suite('InternetPage', function() {
  /** @type {?InternetPageElement} */
  let internetPage = null;

  /** @type {?NetworkSummaryElement} */
  let networkSummary_ = null;

  /** @type {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  let mojoApi_ = null;

  /** @type {?chromeos.cellularSetup.mojom.ESimManagerRemote} */
  let eSimManagerRemote;

  suiteSetup(function() {
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
    network_config.MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;

    eSimManagerRemote = new cellular_setup.FakeESimManagerRemote();
    cellular_setup.setESimManagerRemoteForTesting(eSimManagerRemote);

    // Disable animations so sub-pages open within one event loop.
    testing.Test.disableAnimationsAndTransitions();
  });

  function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  function setNetworksForTest(networks) {
    mojoApi_.resetForTest();
    mojoApi_.addNetworksForTest(networks);
  }

  setup(function() {
    PolymerTest.clearBody();
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
    settings.Router.getInstance().resetRouteForTesting();
  });

  suite('MainPage', function() {
    test('Ethernet', function() {
      // Default fake device state is Ethernet enabled only.
      const ethernet = networkSummary_.$$('#Ethernet');
      assertTrue(!!ethernet);
      assertEquals(1, ethernet.networkStateList.length);
      assertEquals(null, networkSummary_.$$('#Cellular'));
      assertEquals(null, networkSummary_.$$('#VPN'));
      assertEquals(null, networkSummary_.$$('#WiFi'));
    });

    test('WiFi', function() {
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

    test('WiFiToggle', function() {
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
      const mojom = chromeos.networkConfig.mojom;
      // Make WiFi an available but disabled technology.
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kWiFi, false);

      const params = new URLSearchParams;
      params.append('settingId', '4');
      settings.Router.getInstance().navigateTo(
          settings.routes.INTERNET, params);

      await flushAsync();

      const deepLinkElement =
          networkSummary_.$$('#WiFi').$$('#deviceEnabledButton');
      assertTrue(!!deepLinkElement);
      await test_util.waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Toggle WiFi should be focused for settingId=4.');
    });

    suite('VPN', function() {
      test('VpnProviders', function() {
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

      test('should show VPN policy indicator when VPN is disabled', function() {
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
          assertTrue(
              test_util.isVisible(internetPage.$$('#vpnPolicyIndicator')));
          assertTrue(test_util.isVisible(
              networkSummary_.$$('#VPN').$$('#policyIndicator')));
        });
      });

      test(
          'should not show VPN policy indicator when VPN is enabled',
          function() {
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
              assertFalse(
                  test_util.isVisible(internetPage.$$('#vpnPolicyIndicator')));
              assertFalse(test_util.isVisible(
                  networkSummary_.$$('#VPN').$$('#policyIndicator')));
            });
          });
    });

    test('Deep link to mobile on/off toggle', async () => {
      const mojom = chromeos.networkConfig.mojom;
      // Make WiFi an available but disabled technology.
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kCellular, false);

      const params = new URLSearchParams;
      params.append('settingId', '13');
      settings.Router.getInstance().navigateTo(
          settings.routes.INTERNET, params);

      await flushAsync();

      const deepLinkElement =
          networkSummary_.$$('#Cellular').$$('#deviceEnabledButton');
      assertTrue(!!deepLinkElement);
      await test_util.waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Toggle mobile on/off should be focused for settingId=13.');
    });

    test('Show rename esim profile dialog', async function() {
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
    });

    test('Show remove esim profile dialog', async function() {
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
    });
  });

  test(
      'Show pSIM flow cellular setup dialog if route params' +
          'contain showCellularSetup and showPsimFlow',
      async function() {
        loadTimeData.overrideValues({
          updatedCellularActivationUi: true,
        });
        eSimManagerRemote.addEuiccForTest(1);

        const mojom = chromeos.networkConfig.mojom;
        mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kCellular, true);
        const cellularNetwork = OncMojo.getDefaultManagedProperties(
            mojom.NetworkType.kCellular, 'cellular_guid');
        cellularNetwork.connectable = false;
        mojoApi_.setManagedPropertiesForTest(cellularNetwork);

        await flushAsync();

        let cellularSetupDialog = internetPage.$$('#cellularSetupDialog');
        assertFalse(!!cellularSetupDialog);

        const params = new URLSearchParams;
        params.append('guid', 'cellular_guid');
        params.append('type', 'Cellular');
        params.append('name', 'cellular');
        params.append('showCellularSetup', 'true');
        params.append('showPsimFlow', 'true');
        settings.Router.getInstance().navigateTo(
            settings.routes.INTERNET_NETWORKS, params);

        await flushAsync();
        cellularSetupDialog = internetPage.$$('#cellularSetupDialog');
        assertTrue(!!cellularSetupDialog);
        const psimFlow =
            cellularSetupDialog.shadowRoot.querySelector('cellular-setup')
                .shadowRoot.querySelector('#psim-flow-ui');
        assertTrue(!!psimFlow);
      });

  test('Show sim lock dialog through URL parameters', async () => {
    loadTimeData.overrideValues({
      updatedCellularActivationUi: true,
    });
    const mojom = chromeos.networkConfig.mojom;
    mojoApi_.setDeviceStateForTest({
      type: mojom.NetworkType.kCellular,
      deviceState: mojom.DeviceStateType.kEnabled,
      inhibitReason: mojom.InhibitReason.kNotInhibited
    });

    const params = new URLSearchParams;
    params.append(
        'type', OncMojo.getNetworkTypeString(mojom.NetworkType.kCellular));
    params.append('showSimLockDialog', true);
    settings.Router.getInstance().navigateTo(
        settings.routes.INTERNET_NETWORKS, params);

    await flushAsync();

    const simLockDialogs = internetPage.$$('sim-lock-dialogs');
    assertTrue(!!simLockDialogs);
    assertTrue(simLockDialogs.isDialogOpen);
  });

  test(
      'Show no connection toast if receive show-cellular-setup' +
          'event and not connected to non-cellular network',
      async function() {
        assertFalse(internetPage.$.errorToast.open);

        // Send event, toast should show, dialog hidden.
        const event = new CustomEvent('show-cellular-setup', {
          detail: {pageName: cellularSetup.CellularSetupPageName.ESIM_FLOW_UI}
        });
        internetPage.dispatchEvent(event);
        await flushAsync();
        assertTrue(internetPage.$.errorToast.open);
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
    assertFalse(internetPage.$.errorToast.open);

    const message = 'Toast message';
    const event = new CustomEvent('show-error-toast', {detail: message});
    internetPage.dispatchEvent(event);
    await flushAsync();
    assertTrue(internetPage.$.errorToast.open);
    assertEquals(internetPage.$.errorToastMessage.innerHTML, message);
  });

  // TODO(stevenjb): Figure out a way to reliably test navigation. Currently
  // such tests are flaky.
});
