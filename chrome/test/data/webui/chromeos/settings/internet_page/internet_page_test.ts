// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';
import 'chrome://os-settings/lazy_load.js';

import {ApnSubpageElement, CrActionMenuElement, CrExpandButtonElement, CrIconButtonElement, CrToggleElement, EsimRemoveProfileDialogElement, EsimRenameDialogElement, NetworkSummaryElement, NetworkSummaryItemElement, OsSettingsCellularSetupDialogElement, OsSettingsSubpageElement, PaperTooltipElement, Router, routes, settingMojom, SettingsInternetPageElement} from 'chrome://os-settings/os_settings.js';
import {CellularSetupPageName} from 'chrome://resources/ash/common/cellular_setup/cellular_types.js';
import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import {MojoConnectivityProvider} from 'chrome://resources/ash/common/connectivity/mojo_connectivity_provider.js';
import {setHotspotConfigForTesting} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.js';
import {HotspotAllowStatus, HotspotInfo, HotspotState, WiFiBand, WiFiSecurityMode} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.mojom-webui.js';
import {FakeHotspotConfig} from 'chrome://resources/ash/common/hotspot/fake_hotspot_config.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {ApnProperties, DeviceStateProperties, GlobalPolicy, InhibitReason, MAX_NUM_CUSTOM_APNS, VpnType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, DeviceStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {assertEquals, assertFalse, assertNotEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeESimManagerRemote} from 'chrome://webui-test/chromeos/cellular_setup/fake_esim_manager_remote.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {FakePasspointService} from 'chrome://webui-test/chromeos/fake_passpoint_service_mojom.js';
import {flushTasks, waitAfterNextRender, waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

suite('<settings-internet-page>', () => {
  let internetPage: SettingsInternetPageElement;
  let mojoApi: FakeNetworkConfig;
  let eSimManagerRemote: FakeESimManagerRemote;
  let passpointService: FakePasspointService;
  let hotspotConfig: FakeHotspotConfig;

  const fakePrefs = {
    vpn_config_allowed: {
      key: 'vpn_config_allowed',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    arc: {
      vpn: {
        always_on: {
          vpn_package: {
            key: 'vpn_package',
            type: chrome.settingsPrivate.PrefType.STRING,
            value: '',
          },
        },
      },
    },
  };

  function setNetworksForTest(networks: OncMojo.NetworkStateProperties[]):
      void {
    mojoApi.resetForTest();
    mojoApi.addNetworksForTest(networks);
  }

  function setDoesDisconnectProhibitedAlwaysOnVpnPrefs(
      arcVpnAlwaysOnPackageNamePrefValue: string,
      vpnConfigAllowedPrefValue: boolean): void {
    fakePrefs.arc.vpn.always_on.vpn_package.value =
        arcVpnAlwaysOnPackageNamePrefValue;
    fakePrefs.vpn_config_allowed.value = vpnConfigAllowedPrefValue;
    internetPage.prefs = {...fakePrefs};
  }

  async function navigateToCellularSetupDialog(
      showPSimFlow: boolean, isCellularEnabled: boolean): Promise<void> {
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
    mojoApi.setNetworkTypeEnabledState(
        NetworkType.kCellular, isCellularEnabled);

    await flushTasks();
  }

  async function assertWarningMessageVisibility(warningMessage: HTMLElement) {
    assertTrue(!!warningMessage);
    // Warning message should be initially hidden.
    assertTrue(warningMessage.hidden);

    // Add a pSIM network.
    mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);
    const pSimNetwork = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular1', 'foo');
    pSimNetwork.connectionState = ConnectionStateType.kConnected;
    mojoApi.setManagedPropertiesForTest(pSimNetwork);
    await flushTasks();

    // Warning message should now be showing.
    assertFalse(warningMessage.hidden);

    // Disconnect from the pSIM network.
    pSimNetwork.connectionState = ConnectionStateType.kNotConnected;
    mojoApi.setManagedPropertiesForTest(pSimNetwork);
    await flushTasks();
    // Warning message should be hidden.
    assertTrue(warningMessage.hidden);

    // Add an eSIM network.
    const eSimNetwork = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular2', 'foo');
    eSimNetwork.connectionState = ConnectionStateType.kConnected;
    eSimNetwork.typeProperties.cellular!.eid = 'eid';
    mojoApi.setManagedPropertiesForTest(eSimNetwork);
    await flushTasks();

    // Warning message should be showing again.
    assertFalse(warningMessage.hidden);
  }

  async function navigateToCellularDetailPage(): Promise<void> {
    await init();

    const cellularNetwork = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular1', 'name1');
    cellularNetwork.typeProperties.cellular!.eid = 'eid';
    mojoApi.setManagedPropertiesForTest(cellularNetwork);

    const params = new URLSearchParams();
    params.append('guid', cellularNetwork.guid);
    Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);
    await flushTasks();
  }

  async function navigateToApnSubpage(): Promise<void> {
    await navigateToCellularDetailPage();
    internetPage.shadowRoot!.querySelector('settings-internet-detail-subpage')!
        .shadowRoot!.querySelector<HTMLElement>('#apnSubpageButton')!.click();
    await flushTasks();
  }

  function getNetworkSummaryElement(): NetworkSummaryElement {
    const networkSummary =
        internetPage.shadowRoot!.querySelector('network-summary');
    assertTrue(!!networkSummary);
    return networkSummary;
  }

  function queryCellularSetupDialog(): OsSettingsCellularSetupDialogElement|
      null {
    return internetPage.shadowRoot!.querySelector(
        'os-settings-cellular-setup-dialog');
  }

  function queryCreateCustomApnButton(): HTMLButtonElement|null {
    return internetPage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#createCustomApnButton');
  }

  function queryDiscoverMoreApnsButton(): HTMLButtonElement|null {
    return internetPage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#discoverMoreApnsButton');
  }

  function queryApnActionMenuButton(): CrIconButtonElement|null {
    return internetPage.shadowRoot!.querySelector<CrIconButtonElement>(
        '#apnActionMenuButton');
  }

  function queryApnDotsMenu(): CrActionMenuElement|null {
    return internetPage.shadowRoot!.querySelector<CrActionMenuElement>(
        '#apnDotsMenu');
  }

  async function init(): Promise<void> {
    mojoApi.resetForTest();

    clearBody();
    internetPage = document.createElement('settings-internet-page');
    document.body.appendChild(internetPage);

    setDoesDisconnectProhibitedAlwaysOnVpnPrefs(
        /*arcVpnAlwaysOnPackageNamePrefValue=*/ '',
        /*vpnConfigAllowedPrefValue=*/ false);
    await flushTasks();
    await Promise.all([
      mojoApi.whenCalled('getNetworkStateList'),
      mojoApi.whenCalled('getDeviceStateList'),
    ]);
  }

  setup(() => {
    loadTimeData.overrideValues({
      bypassConnectivityCheck: false,
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

    mojoApi = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
        mojoApi);
    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(eSimManagerRemote);
    passpointService = new FakePasspointService();
    MojoConnectivityProvider.getInstance().setPasspointServiceForTest(
        passpointService);
    hotspotConfig = new FakeHotspotConfig();
    setHotspotConfigForTesting(hotspotConfig);
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
  });

  suite('MainPage', () => {
    test('Ethernet', async () => {
      await init();
      const networkSummary = getNetworkSummaryElement();

      // Default fake device state is Ethernet enabled only.
      const ethernet =
          networkSummary.shadowRoot!.querySelector<NetworkSummaryItemElement>(
              '#Ethernet');
      assertTrue(!!ethernet);
      assertEquals(1, ethernet.networkStateList.length);

      assertNull(networkSummary.shadowRoot!.querySelector('#Cellular'));
      assertNull(networkSummary.shadowRoot!.querySelector('#VPN'));
      assertNull(networkSummary.shadowRoot!.querySelector('#WiFi'));
    });

    test('WiFi', async () => {
      await init();
      setNetworksForTest([
        OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi1'),
        OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi2'),
      ]);
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      await flushTasks();

      const wifi =
          getNetworkSummaryElement()
              .shadowRoot!.querySelector<NetworkSummaryItemElement>('#WiFi');
      assertTrue(!!wifi);
      assertEquals(2, wifi.networkStateList.length);
    });

    test('WiFiToggle', async () => {
      await init();
      // Make WiFi an available but disabled technology.
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, false);
      await flushTasks();

      const networkSummary = getNetworkSummaryElement();
      const wifi = networkSummary.shadowRoot!.querySelector('#WiFi');
      assertTrue(!!wifi);

      // Ensure that the initial state is disabled and the toggle is
      // enabled but unchecked.
      let wifiDevice = mojoApi.getDeviceStateForTest(NetworkType.kWiFi);
      assertTrue(!!wifiDevice);
      assertEquals(DeviceStateType.kDisabled, wifiDevice.deviceState);
      const toggle = wifi.shadowRoot!.querySelector<CrToggleElement>(
          '#deviceEnabledButton');
      assertTrue(!!toggle);
      assertFalse(toggle.disabled);
      assertFalse(toggle.checked);

      // Tap the enable toggle button and ensure the state becomes enabling.
      toggle.click();
      await flushTasks();

      assertTrue(toggle.checked);
      wifiDevice = mojoApi.getDeviceStateForTest(NetworkType.kWiFi);
      assertTrue(!!wifiDevice);
      assertEquals(DeviceStateType.kEnabling, wifiDevice.deviceState);
    });

    test('Deep link to WiFi toggle', async () => {
      await init();
      // Make WiFi an available but disabled technology.
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, false);

      const settingId = settingMojom.Setting.kWifiOnOff;
      const params = new URLSearchParams();
      params.append('settingId', settingId.toString());
      Router.getInstance().navigateTo(routes.INTERNET, params);
      await flushTasks();

      const deepLinkElement =
          getNetworkSummaryElement()
              .shadowRoot!.querySelector('#WiFi')!.shadowRoot!
              .querySelector<HTMLElement>('#deviceEnabledButton');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          `Toggle WiFi should be focused for settingId=${settingId}.`);
    });

    test('Deep link to APN menu button', async () => {
      await init();

      const cellularNetwork = OncMojo.getDefaultManagedProperties(
          NetworkType.kCellular, 'cellular1', 'name1');
      cellularNetwork.typeProperties.cellular!.eid = 'eid';
      mojoApi.setManagedPropertiesForTest(cellularNetwork);

      const settingId = settingMojom.Setting.kCellularAddApn;
      const params = new URLSearchParams();
      params.append('settingId', settingId.toString());
      params.append('guid', cellularNetwork.guid);
      Router.getInstance().navigateTo(routes.APN, params);
      await flushTasks();

      const deepLinkElement = queryApnActionMenuButton();
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          `APN menu button be focused for settingId=${settingId}.`);
    });

    suite('VPN', () => {
      test('VpnProviders', async () => {
        await init();
        mojoApi.setVpnProvidersForTest([
          {
            type: VpnType.kExtension,
            providerId: 'extension_id1',
            providerName: 'MyExtensionVPN1',
            appId: 'extension_id1',
            lastLaunchTime: {internalValue: BigInt(0)},
          },
          {
            type: VpnType.kArc,
            providerId: 'vpn.app.package1',
            providerName: 'MyArcVPN1',
            appId: 'arcid1',
            lastLaunchTime: {internalValue: BigInt(1)},
          },
          {
            type: VpnType.kArc,
            providerId: 'vpn.app.package2',
            providerName: 'MyArcVPN2',
            appId: 'arcid2',
            lastLaunchTime: {internalValue: BigInt(2)},
          },
        ]);
        await flushTasks();

        const vpnProviders = internetPage.get('vpnProviders_');
        assertEquals(3, vpnProviders.length);
        // Ensure providers are sorted by type and lastLaunchTime.
        assertEquals('extension_id1', vpnProviders[0].providerId);
        assertEquals('vpn.app.package2', vpnProviders[1].providerId);
        assertEquals('vpn.app.package1', vpnProviders[2].providerId);
      });

      function expandAddConnections(): void {
        const button =
            internetPage.shadowRoot!.querySelector<CrExpandButtonElement>(
                '#expandAddConnections');
        assertTrue(!!button);
        button.expanded = true;
      }

      function addVpnNetworkAndSetDeviceState(vpnProhibited: boolean): void {
        setNetworksForTest([
          OncMojo.getDefaultNetworkState(NetworkType.kVPN, 'vpn'),
        ]);
        mojoApi.setDeviceStateForTest({
          type: NetworkType.kVPN,
          deviceState: vpnProhibited ? DeviceStateType.kProhibited :
                                       DeviceStateType.kEnabled,
        } as DeviceStateProperties);
      }

      function queryAddVpnLabel() {
        return internetPage.shadowRoot!.querySelector('#add-vpn-label');
      }

      function queryAddVpnButton() {
        return internetPage.shadowRoot!.querySelector<CrIconButtonElement>(
            '#add-vpn-button');
      }

      test(
          'should show disabled add VPN button when allow only policy ' +
              'WiFi networks to connect is enabled and VPN is prohibited',
          async () => {
            await init();
            mojoApi.setGlobalPolicy({
              allowOnlyPolicyCellularNetworks: true,
            } as GlobalPolicy);
            expandAddConnections();
            addVpnNetworkAndSetDeviceState(/* vpnProhibited= */ true);
            await flushTasks();

            assertTrue(isVisible(queryAddVpnLabel()));
            assertTrue(queryAddVpnButton()!.disabled);
          });

      test(
          'should show enabled add VPN button when allow only policy ' +
              'WiFi networks to connect is enabled and VPN is allowed',
          async () => {
            await init();
            internetPage.set('globalPolicy_', {
              allowOnlyPolicyWifiNetworksToConnect: true,
            });
            expandAddConnections();
            addVpnNetworkAndSetDeviceState(/* vpnProhibited= */ false);
            await flushTasks();

            assertTrue(isVisible(queryAddVpnLabel()));
            assertFalse(queryAddVpnButton()!.disabled);
          });

      [{
        vpnProhibited: true,
        arcVpnAlwaysOnPackageNamePrefValue: '',
        manualDisconnectionAllowed: false,
      },
       {
         vpnProhibited: true,
         arcVpnAlwaysOnPackageNamePrefValue: '',
         manualDisconnectionAllowed: true,
       },
       {
         vpnProhibited: true,
         arcVpnAlwaysOnPackageNamePrefValue: 'PackageName',
         manualDisconnectionAllowed: false,
       },
       {
         vpnProhibited: true,
         arcVpnAlwaysOnPackageNamePrefValue: 'PackageName',
         manualDisconnectionAllowed: true,
       },
       {
         vpnProhibited: false,
         arcVpnAlwaysOnPackageNamePrefValue: '',
         manualDisconnectionAllowed: false,
       },
       {
         vpnProhibited: false,
         arcVpnAlwaysOnPackageNamePrefValue: '',
         manualDisconnectionAllowed: true,
       },
       {
         vpnProhibited: false,
         arcVpnAlwaysOnPackageNamePrefValue: 'PackageName',
         manualDisconnectionAllowed: false,
       },
       {
         vpnProhibited: false,
         arcVpnAlwaysOnPackageNamePrefValue: 'PackageName',
         manualDisconnectionAllowed: true,
       },
      ].forEach(({
                  vpnProhibited,
                  arcVpnAlwaysOnPackageNamePrefValue,
                  manualDisconnectionAllowed,
                }) => {
        test(
            `VPNs prohibited by policy: ${
                vpnProhibited}, always on VPN set by policy: ${
                  !!arcVpnAlwaysOnPackageNamePrefValue}, ` +
                `setDoesDisconnectProhibitedAlwaysOnVpnPrefs allowed ` +
                `by policy: ${manualDisconnectionAllowed}`,
            async () => {
          await init();

          expandAddConnections();
          addVpnNetworkAndSetDeviceState(vpnProhibited);
          setDoesDisconnectProhibitedAlwaysOnVpnPrefs(
              /* arcVpnAlwaysOnPackageNamePrefValue= */
              arcVpnAlwaysOnPackageNamePrefValue,
              /* vpnConfigAllowedPrefValue= */ manualDisconnectionAllowed);
          await flushTasks();

          const vpnPolicyIndicator =
              internetPage.shadowRoot!.querySelector('#vpnPolicyIndicator');

          if (vpnProhibited ||
              (!!arcVpnAlwaysOnPackageNamePrefValue &&
               !manualDisconnectionAllowed)) {
            assertTrue(isVisible(vpnPolicyIndicator));
          } else {
            assertFalse(isVisible(vpnPolicyIndicator));
          }

          const policyIndicator =
              getNetworkSummaryElement()
                  .shadowRoot!.querySelector('#VPN')!.shadowRoot!.querySelector(
                      '#policyIndicator');
          if (vpnProhibited) {
            assertTrue(isVisible(policyIndicator));
          } else {
            // Note: Users are still allowed to configure existing VPNs
            // they set previously if |arcVpnAlwaysOnPackageNamePrefValue|
            // is not set and |manualDisconnectionAllowed| is false.
            // TODO(http://b/302390893): Consider renaming
            // |vpn_config_allowed| pref to something like
            // "manual_disconnection_allowed", or discuss with DPanel team
            // about rewording
            //  https://screenshot.googleplex.com/4KLUdLUtPsvNMDT
            assertFalse(isVisible(policyIndicator));
          }
            });
      });
    });

    test('Deep link to mobile on/off toggle', async () => {
      await init();
      // Make WiFi an available but disabled technology.
      mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, false);

      const setting = settingMojom.Setting.kMobileOnOff;
      const params = new URLSearchParams();
      params.append('settingId', setting.toString());
      Router.getInstance().navigateTo(routes.INTERNET, params);
      await flushTasks();

      const deepLinkElement =
          getNetworkSummaryElement()
              .shadowRoot!.querySelector('#Cellular')!.shadowRoot!
              .querySelector<HTMLElement>('#deviceEnabledButton');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          `Toggle mobile on/off should be focused for settingId=${setting}.`);
    });

    test('Show rename esim profile dialog', async () => {
      await init();
      eSimManagerRemote.addEuiccForTest(1);
      await flushTasks();

      function queryEsimRenameDialog(): EsimRenameDialogElement|null {
        return internetPage.shadowRoot!.querySelector('esim-rename-dialog');
      }

      let renameDialog = queryEsimRenameDialog();
      assertNull(renameDialog);

      const event = new CustomEvent(
          'show-esim-profile-rename-dialog', {detail: {iccid: '1'}});
      internetPage.dispatchEvent(event);
      await flushTasks();

      renameDialog = queryEsimRenameDialog();
      assertTrue(!!renameDialog);
      await assertWarningMessageVisibility(renameDialog.$.warningMessage);
    });

    test('Show remove esim profile dialog', async () => {
      await init();
      eSimManagerRemote.addEuiccForTest(1);
      await flushTasks();

      function queryEsimRemoveProfileDialog(): EsimRemoveProfileDialogElement|
          null {
        return internetPage.shadowRoot!.querySelector(
            'esim-remove-profile-dialog');
      }

      let removeDialog = queryEsimRemoveProfileDialog();
      assertNull(removeDialog);

      const event = new CustomEvent(
          'show-esim-remove-profile-dialog', {detail: {iccid: '1'}});
      internetPage.dispatchEvent(event);
      await flushTasks();

      removeDialog = queryEsimRemoveProfileDialog();
      assertTrue(!!removeDialog);
      await assertWarningMessageVisibility(removeDialog.$.warningMessage);
    });
  });

  test(
      'Show pSIM flow cellular setup dialog if route params' +
          'contain showCellularSetup and showPsimFlow',
      async () => {
        await init();

        assertNull(queryCellularSetupDialog());

        await navigateToCellularSetupDialog(
            /*showPSimFlow=*/ true, /*isCellularEnabled=*/ true);

        const cellularSetupDialog = queryCellularSetupDialog();
        assertTrue(!!cellularSetupDialog);
        const psimFlow =
            cellularSetupDialog.shadowRoot!.querySelector('cellular-setup')!
                .shadowRoot!.querySelector('#psim-flow-ui');
        assertTrue(!!psimFlow);
      });

  test(
      'Show eSIM flow cellular setup dialog if route params' +
          'contains showCellularSetup, does not contain showPsimFlow,' +
          'connected to a non-cellular network, and cellular enabled',
      async () => {
        await init();
        eSimManagerRemote.addEuiccForTest(1);

        const wifiNetwork =
            OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi');
        wifiNetwork.connectionState = ConnectionStateType.kOnline;
        mojoApi.addNetworksForTest([wifiNetwork]);
        await flushTasks();

        assertNull(queryCellularSetupDialog());

        await navigateToCellularSetupDialog(
            /*showPSimFlow=*/ false, /*isCellularEnabled=*/ true);

        const cellularSetupDialog = queryCellularSetupDialog();
        assertTrue(!!cellularSetupDialog);
        const esimFlow =
            cellularSetupDialog.shadowRoot!.querySelector('cellular-setup')!
                .shadowRoot!.querySelector('#esim-flow-ui');
        assertTrue(!!esimFlow);
      });

  test(
      'Show no connection toast if route params' +
          'contain showCellularSetup, does not contain showPsimFlow,' +
          'cellular is enabled, but not connected to a non-cellular network',
      async () => {
        await init();
        eSimManagerRemote.addEuiccForTest(1);

        assertNull(queryCellularSetupDialog());

        await navigateToCellularSetupDialog(
            /*showPSimFlow=*/ false, /*isCellularEnabled=*/ true);

        assertTrue(internetPage.$.errorToast.open);
        assertEquals(
            internetPage.i18n('eSimNoConnectionErrorToast'),
            internetPage.$.errorToastMessage.innerHTML);
        assertNull(queryCellularSetupDialog());
      });

  test(
      'Show eSIM flow cellular setup dialog if route params ' +
          'contain showCellularSetup, does not contain showPsimFlow, ' +
          'cellular is enabled, not connected to a non-cellular network ' +
          'but cellular bypass esim installation connectivity flag is enabled',
      async () => {
        loadTimeData.overrideValues({
          bypassConnectivityCheck: true,
        });
        await init();
        eSimManagerRemote.addEuiccForTest(1);

        assertNull(queryCellularSetupDialog());

        await navigateToCellularSetupDialog(
            /*showPSimFlow=*/ false, /*isCellularEnabled=*/ true);

        const cellularSetupDialog = queryCellularSetupDialog();
        assertTrue(!!cellularSetupDialog);
        const esimFlow =
            cellularSetupDialog.shadowRoot!.querySelector('cellular-setup')!
                .shadowRoot!.querySelector('#esim-flow-ui');
        assertTrue(!!esimFlow);
      });

  test(
      'Show mobile data not enabled toast if route params' +
          'contains showCellularSetup, does not contain showPsimFlow,' +
          'connected to a non-cellular network, but cellular not enabled',
      async () => {
        await init();
        eSimManagerRemote.addEuiccForTest(1);

        const wifiNetwork =
            OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi');
        wifiNetwork.connectionState = ConnectionStateType.kOnline;
        mojoApi.addNetworksForTest([wifiNetwork]);
        await flushTasks();

        assertNull(queryCellularSetupDialog());

        await navigateToCellularSetupDialog(
            /*showPSimFlow=*/ false, /*isCellularEnabled=*/ false);

        assertTrue(internetPage.$.errorToast.open);
        assertEquals(
            internetPage.i18n('eSimMobileDataNotEnabledErrorToast'),
            internetPage.$.errorToastMessage.innerHTML);
        assertNull(queryCellularSetupDialog());
      });

  test(
      'Show profile limit reached toast if route params' +
          'contains showCellularSetup, does not contain showPsimFlow,' +
          'connected to a non-cellular network, cellular enabled,' +
          'but profile limit is reached',
      async () => {
        await init();
        eSimManagerRemote.addEuiccForTest(/*numProfiles=*/ 5);

        const wifiNetwork =
            OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi');
        wifiNetwork.connectionState = ConnectionStateType.kOnline;
        mojoApi.addNetworksForTest([wifiNetwork]);
        await flushTasks();

        assertNull(queryCellularSetupDialog());

        await navigateToCellularSetupDialog(
            /*showPSimFlow=*/ false, /*isCellularEnabled=*/ true);

        assertTrue(internetPage.$.errorToast.open);
        assertEquals(
            internetPage.i18n('eSimProfileLimitReachedErrorToast', 5),
            internetPage.$.errorToastMessage.innerHTML);
        assertNull(queryCellularSetupDialog());
      });

  test('Show sim lock dialog through URL parameters', async () => {
    await init();

    const params = new URLSearchParams();
    params.append('type', OncMojo.getNetworkTypeString(NetworkType.kCellular));
    params.append('showSimLockDialog', 'true');

    // Pretend that we initially started on the INTERNET_NETWORKS route with the
    // params.
    Router.getInstance().navigateTo(routes.INTERNET_NETWORKS, params);
    internetPage.currentRouteChanged(routes.INTERNET_NETWORKS, undefined);

    // Update the device state here to trigger an onDeviceStatesChanged_() call.
    mojoApi.setDeviceStateForTest({
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason: InhibitReason.kNotInhibited,
      simLockStatus: {
        lockEnabled: true,
      },
    } as DeviceStateProperties);
    await flushTasks();

    const simLockDialogs =
        internetPage.shadowRoot!.querySelector('sim-lock-dialogs');
    assertTrue(!!simLockDialogs);
    assertTrue(simLockDialogs.isDialogOpen);
  });

  test('Show carrier lock sub header when locked', async () => {
    await init();

    const params = new URLSearchParams();
    params.append('type', OncMojo.getNetworkTypeString(NetworkType.kCellular));

    // Pretend that we initially started on the INTERNET_NETWORKS route with the
    // params.
    Router.getInstance().navigateTo(routes.INTERNET_NETWORKS, params);
    internetPage.currentRouteChanged(routes.INTERNET_NETWORKS, undefined);

    // Update the device state here to trigger an onDeviceStatesChanged_() call.
    mojoApi.setDeviceStateForTest({
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason: InhibitReason.kNotInhibited,
      isCarrierLocked: true,
    } as DeviceStateProperties);
    await flushTasks();

    const cellularSubtitle =
        internetPage.shadowRoot!.querySelector<HTMLElement>(
            '#cellularSubtitle');
    assertTrue(!!cellularSubtitle);
  });

  test(
      'Verify carrier lock sub header not displayed when unlocked',
      async () => {
        await init();

        const params = new URLSearchParams();
        params.append(
            'type', OncMojo.getNetworkTypeString(NetworkType.kCellular));

        // Pretend that we initially started on the INTERNET_NETWORKS route with
        // the params.
        Router.getInstance().navigateTo(routes.INTERNET_NETWORKS, params);
        internetPage.currentRouteChanged(routes.INTERNET_NETWORKS, undefined);

        // Update the device state here to trigger an onDeviceStatesChanged_()
        // call.
        mojoApi.setDeviceStateForTest({
          type: NetworkType.kCellular,
          deviceState: DeviceStateType.kEnabled,
          inhibitReason: InhibitReason.kNotInhibited,
          isCarrierLocked: false,
        } as DeviceStateProperties);
        await flushTasks();

        const cellularSubtitle =
            internetPage.shadowRoot!.querySelector<HTMLElement>(
                '#cellularSubtitle');
        assertNull(cellularSubtitle);
      });

  test('Show modem flashing sub header when flashing', async () => {
    await init();

    const params = new URLSearchParams();
    params.append('type', OncMojo.getNetworkTypeString(NetworkType.kCellular));

    // Pretend that we initially started on the INTERNET_NETWORKS route with the
    // params.
    Router.getInstance().navigateTo(routes.INTERNET_NETWORKS, params);
    internetPage.currentRouteChanged(routes.INTERNET_NETWORKS, undefined);

    // Update the device state here to trigger an onDeviceStatesChanged_() call.
    mojoApi.setDeviceStateForTest({
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason: InhibitReason.kNotInhibited,
      isFlashing: true,
    } as DeviceStateProperties);
    await flushTasks();

    const flashingSubtitle =
        internetPage.shadowRoot!.querySelector<HTMLElement>(
            '#flashingSubtitle');
    assertTrue(!!flashingSubtitle);
  });

  test('Not showing modem flashing sub header when flashing', async () => {
    await init();

    const params = new URLSearchParams();
    params.append('type', OncMojo.getNetworkTypeString(NetworkType.kCellular));

    // Pretend that we initially started on the INTERNET_NETWORKS route with the
    // params.
    Router.getInstance().navigateTo(routes.INTERNET_NETWORKS, params);
    internetPage.currentRouteChanged(routes.INTERNET_NETWORKS, undefined);

    // Update the device state here to trigger an onDeviceStatesChanged_() call.
    mojoApi.setDeviceStateForTest({
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason: InhibitReason.kNotInhibited,
      isFlashing: false,
    } as DeviceStateProperties);
    await flushTasks();

    const flashingSubtitle =
        internetPage.shadowRoot!.querySelector<HTMLElement>(
            '#flashingSubtitle');
    assertNull(flashingSubtitle);
  });

  test(
      'Show no connection toast if receive show-cellular-setup' +
          'event and not connected to non-cellular network',
      async () => {
        await init();
        eSimManagerRemote.addEuiccForTest(/*numProfiles=*/ 1);
        mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);
        await flushTasks();

        assertFalse(internetPage.$.errorToast.open);

        // Send event, toast should show, dialog hidden.
        const event = new CustomEvent(
            'show-cellular-setup',
            {detail: {pageName: CellularSetupPageName.ESIM_FLOW_UI}});
        internetPage.dispatchEvent(event);
        await flushTasks();

        assertTrue(internetPage.$.errorToast.open);
        assertEquals(
            internetPage.i18n('eSimNoConnectionErrorToast'),
            internetPage.$.errorToastMessage.innerHTML);

        assertNull(queryCellularSetupDialog());

        // Hide the toast
        internetPage.$.errorToast.hide();
        assertFalse(internetPage.$.errorToast.open);

        // Connect to non-cellular network.
        const wifiNetwork =
            OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi');
        wifiNetwork.connectionState = ConnectionStateType.kOnline;
        mojoApi.addNetworksForTest([wifiNetwork]);
        await flushTasks();

        // Send event, toast should be hidden, dialog open.
        internetPage.dispatchEvent(event);
        await flushTasks();
        assertFalse(internetPage.$.errorToast.open);
        assertTrue(!!queryCellularSetupDialog());
      });

  test('Show toast on show-error-toast event', async () => {
    await init();
    assertFalse(internetPage.$.errorToast.open);

    const message = 'Toast message';
    const event = new CustomEvent('show-error-toast', {detail: message});
    internetPage.dispatchEvent(event);
    await flushTasks();
    assertTrue(internetPage.$.errorToast.open);
    assertEquals(message, internetPage.$.errorToastMessage.innerHTML);
  });

  test('Internet detail menu renders', async () => {
    await navigateToCellularDetailPage();

    const internetDetailMenu =
        internetPage.shadowRoot!.querySelector('settings-internet-detail-menu');
    assertTrue(!!internetDetailMenu);
  });

  test('Update global policy when triggering OnPoliciesApplied()', async () => {
    await navigateToCellularDetailPage();

    const detailPage = internetPage.shadowRoot!.querySelector(
        'settings-internet-detail-subpage');
    assertTrue(!!detailPage);
    assertTrue(!!detailPage.globalPolicy);
    assertFalse(!!detailPage.globalPolicy.allowOnlyPolicyCellularNetworks);

    // Set global policy should also update the global policy
    mojoApi.setGlobalPolicy({
      allowOnlyPolicyCellularNetworks: true,
    } as GlobalPolicy);
    await flushTasks();

    assertTrue(!!detailPage);
    assertTrue(!!detailPage.globalPolicy);
    assertTrue(detailPage.globalPolicy.allowOnlyPolicyCellularNetworks);
  });

  test(
      'Navigating to Known Networks without network-type parameters ' +
          'defaults to Wi-Fi',
      async () => {
        await init();

        const params = new URLSearchParams();
        params.append('type', '');

        // Navigate straight to Known Networks while passing in parameters
        // with an empty type.
        Router.getInstance().navigateTo(routes.KNOWN_NETWORKS, params);
        internetPage.currentRouteChanged(routes.KNOWN_NETWORKS, undefined);

        const knownNetworksPage = internetPage.shadowRoot!.querySelector(
            'settings-internet-known-networks-subpage');
        assertTrue(!!knownNetworksPage);

        // Confirm that the knownNetworkType_ was set to kWiFi.
        assertEquals(NetworkType.kWiFi, knownNetworksPage.networkType);
      });

  test('Navigate to/from APN subpage', async () => {
    loadTimeData.overrideValues({isApnRevampEnabled: true});
    await navigateToApnSubpage();
    assertEquals(Router.getInstance().currentRoute, routes.APN);
    assertTrue(!!internetPage.shadowRoot!.querySelector('apn-subpage'));

    const windowPopstatePromise = eventToPromise('popstate', window);
    Router.getInstance().navigateToPreviousRoute();
    await windowPopstatePromise;
    await waitBeforeNextRender(internetPage);

    const detailPage = internetPage.shadowRoot!.querySelector(
        'settings-internet-detail-subpage');
    assertTrue(!!detailPage);
    await flushTasks();

    assertEquals(
        detailPage.shadowRoot!.querySelector('#apnSubpageButton'),
        detailPage.shadowRoot!.activeElement,
        'Apn subpage row should be focused');
  });

  test(
      'Create apn button opens dialogs and clicking cancel button removes it',
      async () => {
        loadTimeData.overrideValues({isApnRevampEnabled: true});
        await navigateToApnSubpage();
        const subpage = internetPage.shadowRoot!.querySelector('apn-subpage');
        assertTrue(!!subpage);
        const apnList = subpage.shadowRoot!.querySelector('apn-list');
        assertTrue(!!apnList);
        const getApnDetailDialog = () =>
            apnList.shadowRoot!.querySelector('apn-detail-dialog');

        assertNull(getApnDetailDialog());
        const apnMenuButton = queryApnActionMenuButton();
        assertTrue(!!apnMenuButton);
        apnMenuButton.click();
        await flushTasks();

        const apnDotsMenu = queryApnDotsMenu();
        assertTrue(!!apnDotsMenu);
        assertTrue(apnDotsMenu.open);
        const createCustomApnButton = queryCreateCustomApnButton();
        assertTrue(!!createCustomApnButton);
        createCustomApnButton.click();
        await flushTasks();

        assertFalse(apnDotsMenu.open);
        const apnDetailDialog = getApnDetailDialog();
        assertTrue(!!apnDetailDialog);
        const onCloseEventPromise = eventToPromise('close', apnList);
        const cancelButton =
            apnDetailDialog.shadowRoot!.querySelector<HTMLElement>(
                '#apnDetailCancelBtn');
        assertTrue(!!cancelButton);
        cancelButton.click();
        await onCloseEventPromise;

        assertNull(getApnDetailDialog());
      });

  test(
      'Discover more apns button opens dialog and clicking cancel button ' +
          'removes it',
      async () => {
        loadTimeData.overrideValues({isApnRevampEnabled: true});
        await navigateToApnSubpage();
        const subpage = internetPage.shadowRoot!.querySelector('apn-subpage');
        assertTrue(!!subpage);
        const apnList = subpage.shadowRoot!.querySelector('apn-list');
        assertTrue(!!apnList);

        function getApnSelectionDialog() {
          return apnList!.shadowRoot!.querySelector('apn-selection-dialog');
        }

        assertNull(getApnSelectionDialog());
        const apnMenuButton = queryApnActionMenuButton();
        assertTrue(!!apnMenuButton);
        assertEquals(internetPage.i18n('moreActions'), apnMenuButton.title);
        apnMenuButton.click();
        await flushTasks();

        const apnDotsMenu = queryApnDotsMenu();
        assertTrue(!!apnDotsMenu);
        assertTrue(apnDotsMenu.open);
        const discoverMoreApnsButton = queryDiscoverMoreApnsButton();
        assertTrue(!!discoverMoreApnsButton);
        discoverMoreApnsButton.click();
        await flushTasks();

        assertFalse(apnDotsMenu.open);
        const apnSelectionDialog = getApnSelectionDialog();
        assertTrue(!!apnSelectionDialog);
        const onCloseEventPromise = eventToPromise('close', apnList);
        const cancelButton =
            apnSelectionDialog.shadowRoot!.querySelector<HTMLElement>(
                '.cancel-button');
        assertTrue(!!cancelButton);
        cancelButton.click();
        await onCloseEventPromise;

        assertNull(getApnSelectionDialog());
      });

  test('Navigate to APN subpage and remove cellular properties.', async () => {
    loadTimeData.overrideValues({isApnRevampEnabled: true});
    await navigateToApnSubpage();
    assertEquals(routes.APN, Router.getInstance().currentRoute);
    assertTrue(!!internetPage.shadowRoot!.querySelector('apn-subpage'));
    // We use the same guid as in navigateToCellularDetailPage so that
    // we trigger onNetworkStateChanged
    const network = OncMojo.getDefaultManagedProperties(
        NetworkType.kWiFi, 'cellular1', 'name1');
    const windowPopstatePromise = eventToPromise('popstate', window);
    mojoApi.setManagedPropertiesForTest(network);
    await windowPopstatePromise;
    await waitBeforeNextRender(internetPage);
    // Because there were no cellular properties we call apn_subpage close
    // which navigates to the previous page.
    assertEquals(routes.NETWORK_DETAIL, Router.getInstance().currentRoute);
  });

  test(
      'Navigate to APN subpage without providing guid as parameter',
      async () => {
        loadTimeData.overrideValues({isApnRevampEnabled: true});
        await navigateToCellularDetailPage();
        const windowPopstatePromise = eventToPromise('popstate', window);
        Router.getInstance().navigateTo(routes.APN);
        await windowPopstatePromise;
        await waitBeforeNextRender(internetPage);
        assertNotEquals(routes.APN, Router.getInstance().currentRoute);
      });

  ['ltr', 'rtl'].forEach(languageDirection => {
    test(
        'Disable and show tooltip for APN buttons when custom APNs limit is' +
            'reached',
        async () => {
          document.body.style.direction = languageDirection;
          loadTimeData.overrideValues({isApnRevampEnabled: true});
          await navigateToApnSubpage();
          const getCreateCustomApnTooltip = () =>
              internetPage.shadowRoot!.querySelector<PaperTooltipElement>(
                  '#createCustomApnTooltip');
          const getDiscoverMoreApnsTooltip = () =>
              internetPage.shadowRoot!.querySelector<PaperTooltipElement>(
                  '#discoverMoreApnsTooltip');

          const createCustomApnButton = queryCreateCustomApnButton();
          assertTrue(!!createCustomApnButton);
          assertFalse(createCustomApnButton.disabled);
          const discoverMoreApnsButton = queryDiscoverMoreApnsButton();
          assertTrue(!!discoverMoreApnsButton);
          assertFalse(discoverMoreApnsButton.disabled);
          assertNull(getCreateCustomApnTooltip());
          assertNull(getDiscoverMoreApnsTooltip());

          let properties = OncMojo.getDefaultManagedProperties(
              NetworkType.kCellular, 'cellular1', 'cellular');

          // We're setting the list of APNs to the max number
          const customApn = {accessPointName: 'apn'} as ApnProperties;
          properties.typeProperties.cellular!.customApnList =
              Array(MAX_NUM_CUSTOM_APNS).fill({...customApn});
          mojoApi.setManagedPropertiesForTest(properties);
          await flushTasks();

          assertTrue(createCustomApnButton.disabled);
          assertTrue(discoverMoreApnsButton.disabled);
          const createCustomApnTooltip = getCreateCustomApnTooltip();
          assertTrue(!!createCustomApnTooltip);
          assertTrue(createCustomApnTooltip.innerHTML.includes(
              internetPage.i18n('customApnLimitReached')));
          const toolTipPosition =
              languageDirection === 'ltr' ? 'left' : 'right';
          assertEquals(toolTipPosition, createCustomApnTooltip.position);
          const discoverMoreApnsTooltip = getDiscoverMoreApnsTooltip();
          assertTrue(!!discoverMoreApnsTooltip);
          assertTrue(discoverMoreApnsTooltip.innerHTML.includes(
              internetPage.i18n('customApnLimitReached')));
          assertEquals(toolTipPosition, discoverMoreApnsTooltip.position);

          properties = OncMojo.getDefaultManagedProperties(
              NetworkType.kCellular, 'cellular1', 'cellular');
          properties.typeProperties.cellular!.customApnList = [];
          mojoApi.setManagedPropertiesForTest(properties);
          await flushTasks();

          assertFalse(createCustomApnButton.disabled);
          assertFalse(discoverMoreApnsButton.disabled);
          assertNull(getCreateCustomApnTooltip());
          assertNull(getDiscoverMoreApnsTooltip());
        });
  });

  [true, false].forEach(isApnRevampAndAllowApnModificationPolicyEnabled => {
    test(
        `Managed APN UI states when ` +
            `isApnRevampAndAllowApnModificationPolicyEnabled is ${
                isApnRevampAndAllowApnModificationPolicyEnabled}`,
        async () => {
          loadTimeData.overrideValues({
            isApnRevampEnabled: true,
            isApnRevampAndAllowApnModificationPolicyEnabled:
                isApnRevampAndAllowApnModificationPolicyEnabled,
          });
          await navigateToApnSubpage();

          mojoApi.setGlobalPolicy(undefined);
          await flushTasks();

          // Check for APN policies managed icon.
          const getApnManagedIcon = () =>
              internetPage.shadowRoot!.querySelector('#apnManagedIcon');
          const apnActionMenuButton =
              internetPage.shadowRoot!.querySelector<HTMLButtonElement>(
                  '#apnActionMenuButton');
          const apnSubpage =
              internetPage.shadowRoot!.querySelector<ApnSubpageElement>(
                  'apn-subpage');
          assertFalse(!!getApnManagedIcon());
          assert(apnActionMenuButton);
          assertFalse(apnActionMenuButton.disabled);
          assert(apnSubpage);
          assertFalse(apnSubpage.shouldDisallowApnModification);

          let globalPolicy = {
            allowApnModification: true,
          } as GlobalPolicy;
          mojoApi.setGlobalPolicy(globalPolicy);
          await flushTasks();
          assertFalse(!!getApnManagedIcon());
          assertFalse(apnActionMenuButton.disabled);
          assertFalse(apnSubpage.shouldDisallowApnModification);

          globalPolicy = {
            allowApnModification: false,
          } as GlobalPolicy;
          mojoApi.setGlobalPolicy(globalPolicy);
          await flushTasks();
          assertEquals(
              isApnRevampAndAllowApnModificationPolicyEnabled,
              !!getApnManagedIcon());
          assertEquals(
              isApnRevampAndAllowApnModificationPolicyEnabled,
              apnActionMenuButton.disabled);
          assertEquals(
              isApnRevampAndAllowApnModificationPolicyEnabled,
              apnSubpage.shouldDisallowApnModification);
        });
  });

  test('Navigate to Passpoint detail page', async () => {
    const subId = 'a_passpoint_id';
    const sub = {
      id: subId,
      domains: ['passpoint.example.com'],
      friendlyName: 'Passpoint Example Ltd.',
      provisioningSource: 'app.passpoint.example.com',
      trustedCa: '',
      expirationEpochMs: 0n,
    };
    passpointService.addSubscription(sub);
    await init();

    const params = new URLSearchParams();
    params.append('id', subId);

    // Navigate straight to Passpoint detail subpage.
    Router.getInstance().navigateTo(routes.PASSPOINT_DETAIL, params);
    internetPage.currentRouteChanged(routes.PASSPOINT_DETAIL, undefined);

    const passpointDetailPage =
        internetPage.shadowRoot!.querySelector('settings-passpoint-subpage');
    assertTrue(!!passpointDetailPage);
  });

  test('Show spinner on hotspot subpage when enabling', async () => {
    const hotspotInfo = {
      state: HotspotState.kDisabled,
      allowStatus: HotspotAllowStatus.kAllowed,
      clientCount: 0,
      config: {
        ssid: 'test_ssid',
        passphrase: 'test_passphrase',
        autoDisable: true,
        security: WiFiSecurityMode.kWpa2,
        band: WiFiBand.kAutoChoose,
        bssidRandomization: true,
      },
      allowedWifiSecurityModes: [],
    } as HotspotInfo;
    hotspotConfig.setFakeHotspotInfo(hotspotInfo);
    await init();

    Router.getInstance().navigateTo(routes.HOTSPOT_DETAIL);
    await flushTasks();

    const hotspotDetailPage =
        internetPage.shadowRoot!.querySelector('settings-hotspot-subpage');
    assertTrue(!!hotspotDetailPage);

    const hotspotSubpage =
        internetPage.shadowRoot!.querySelector<OsSettingsSubpageElement>(
            'os-settings-subpage#hotspotSubpage');
    assertTrue(!!hotspotSubpage);
    assertFalse(hotspotSubpage.showSpinner);

    hotspotConfig.setFakeHotspotState(HotspotState.kEnabling);
    await flushTasks();
    assertTrue(hotspotSubpage.showSpinner);

    hotspotConfig.setFakeHotspotState(HotspotState.kDisabling);
    await flushTasks();
    assertTrue(hotspotSubpage.showSpinner);
  });

  // TODO(stevenjb): Figure out a way to reliably test navigation. Currently
  // such tests are flaky.
});
