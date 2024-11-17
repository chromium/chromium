// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://internet-detail-dialog/internet_detail_dialog.js';

import {InternetDetailDialogElement} from 'chrome://internet-detail-dialog/internet_detail_dialog.js';
import {InternetDetailDialogBrowserProxy, InternetDetailDialogBrowserProxyImpl} from 'chrome://internet-detail-dialog/internet_detail_dialog_browser_proxy.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrToastElement} from 'chrome://resources/ash/common/cr_elements/cr_toast/cr_toast.js';
import {ApnListElement} from 'chrome://resources/ash/common/network/apn_list.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {NetworkApnListElement} from 'chrome://resources/ash/common/network/network_apnlist.js';
import {NetworkChooseMobileElement} from 'chrome://resources/ash/common/network/network_choose_mobile.js';
import {NetworkIpConfigElement} from 'chrome://resources/ash/common/network/network_ip_config.js';
import {NetworkNameserversElement} from 'chrome://resources/ash/common/network/network_nameservers.js';
import {NetworkPropertyListMojoElement} from 'chrome://resources/ash/common/network/network_property_list_mojo.js';
import {NetworkProxyElement} from 'chrome://resources/ash/common/network/network_proxy.js';
import {NetworkSiminfoElement} from 'chrome://resources/ash/common/network/network_siminfo.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {ApnAuthenticationType, ApnIpType, ApnProperties, ApnSource, ApnState, ApnType, GlobalPolicy, InhibitReason, MAX_NUM_CUSTOM_APNS, SIMInfo} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, DeviceStateType, NetworkType, OncSource, PortalState} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {IronCollapseElement} from 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {FakeNetworkConfig} from './fake_network_config_mojom.js';

export class TestInternetDetailDialogBrowserProxy extends TestBrowserProxy
    implements InternetDetailDialogBrowserProxy {
  constructor() {
    super([
      'getDialogArguments',
      'closeDialog',
      'showPortalSignin',
    ]);
  }

  getDialogArguments() {
    return JSON.stringify({guid: 'guid'});
  }

  closeDialog() {}

  showPortalSignin() {}
}

suite('internet-detail-dialog', () => {
  const guid = 'guid';
  const testIccid = '11111111111111111';
  let internetDetailDialog: InternetDetailDialogElement;

  let mojoApi: FakeNetworkConfig;

  suiteSetup(function() {
    mojoApi = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
        mojoApi);
  });

  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  function getManagedProperties(
      type: NetworkType, name?: string, source?: OncSource) {
    const result =
        OncMojo.getDefaultManagedProperties(type, guid, name ? name : '');
    if (source) {
      result.source = source;
    }
    return result;
  }

  setup(async () => {
    assert(window.trustedTypes);
    document.body.innerHTML = window.trustedTypes.emptyHTML;
    InternetDetailDialogBrowserProxyImpl.setInstance(
        new TestInternetDetailDialogBrowserProxy());
    mojoApi.resetForTest();
  });

  teardown(function() {
    // If a previous test was run with Jelly, the css needs to be removed.
    const old_elements =
        document.querySelectorAll('link[href*=\'chrome://theme/colors.css\']');
    old_elements.forEach(function(node) {
      node.remove();
    });
    assertFalse(
        !!document.querySelector('link[href*=\'chrome://theme/colors.css\']'));

    document.body.classList.remove('jelly-enabled');
  });

  async function init() {
    internetDetailDialog = document.createElement('internet-detail-dialog');
    document.body.appendChild(internetDetailDialog);
    await flushAsync();
  }

  async function setupCellularNetwork(
      isPrimary: boolean, isInhibited: boolean, connectedApn?: ApnProperties,
      customApnList?: ApnProperties[], errorState?: string,
      portalState?: PortalState) {
    await mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);

    const cellularNetwork = getManagedProperties(
        NetworkType.kCellular, /*name=*/ undefined, OncSource.kDevice);
    if (cellularNetwork.typeProperties.cellular) {
      cellularNetwork.typeProperties.cellular.iccid = testIccid;
      // Required for networkChooseMobile to be rendered.
      cellularNetwork.typeProperties.cellular.supportNetworkScan = true;
      cellularNetwork.typeProperties.cellular.connectedApn = connectedApn;
      cellularNetwork.typeProperties.cellular.customApnList = customApnList;
    }
    // Required for connectDisconnectButton to be rendered.
    cellularNetwork.connectionState = isPrimary ?
        ConnectionStateType.kConnected :
        ConnectionStateType.kNotConnected;
    cellularNetwork.errorState = errorState;
    if (portalState) {
      cellularNetwork.portalState = portalState;
    }

    mojoApi.setManagedPropertiesForTest(cellularNetwork);
    setDeviceState(
        NetworkType.kCellular, DeviceStateType.kEnabled,
        (isInhibited ? InhibitReason.kInstallingProfile :
                       InhibitReason.kNotInhibited),
        [{
          iccid: testIccid,
          isPrimary: isPrimary,
          slotId: 1,
          eid: 'eid',
        }]);
  }

  function setDeviceState(
      type: NetworkType, deviceState: DeviceStateType,
      inhibitReason?: InhibitReason, simInfos?: SIMInfo[],
      macAddress?: string) {
    mojoApi.setDeviceStateForTest({
      type: type,
      deviceState: deviceState,
      inhibitReason: inhibitReason ? inhibitReason :
                                     InhibitReason.kNotInhibited,
      simInfos: simInfos ? simInfos : undefined,
      ipv4Address: undefined,
      ipv6Address: undefined,
      imei: undefined,
      macAddress: macAddress,
      scanning: false,
      simLockStatus: undefined,
      simAbsent: false,
      managedNetworkAvailable: false,
      serial: undefined,
      isCarrierLocked: false,
      isFlashing: false,
    });
  }

  function createApn(
      accessPointName: string, source: ApnSource, name?: string) {
    return {
      accessPointName: accessPointName,
      id: undefined,
      authentication: ApnAuthenticationType.kAutomatic,
      language: undefined,
      localizedName: undefined,
      name: name,
      password: undefined,
      username: undefined,
      attach: undefined,
      state: ApnState.kEnabled,
      ipType: ApnIpType.kAutomatic,
      apnTypes: [ApnType.kDefault],
      source: source,
    };
  }

  function getElement<T extends HTMLElement = HTMLElement>(selector: string):
      T {
    const element = internetDetailDialog.shadowRoot!.querySelector<T>(selector);
    assert(element);
    return element;
  }

  suite('captive portal ui updates', () => {
    function getButton(buttonId: string): CrButtonElement {
      const button =
          internetDetailDialog.shadowRoot!.querySelector<CrButtonElement>(
              `#${buttonId}`);
      assertTrue(!!button);
      return button;
    }

    test('WiFi in a portal portalState', function() {
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi_user');
      wifiNetwork.source = OncSource.kUser;
      wifiNetwork.connectable = true;
      wifiNetwork.connectionState = ConnectionStateType.kPortal;
      wifiNetwork.portalState = PortalState.kPortal;

      mojoApi.setManagedPropertiesForTest(wifiNetwork);
      init();
      return flushAsync().then(() => {
        const networkStateText = getElement('#networkState');
        assertTrue(networkStateText.hasAttribute('warning'));
        assert(networkStateText.textContent);
        assertEquals(
            networkStateText.textContent.trim(),
            internetDetailDialog.i18n('networkListItemSignIn'));
        const signinButton = getButton('signinButton');
        assertTrue(!!signinButton);
        assertFalse(signinButton.hasAttribute('hidden'));
        assertFalse(signinButton.disabled);
      });
    });

    test('WiFi in a no internet portalState', function() {
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi_user');
      wifiNetwork.source = OncSource.kUser;
      wifiNetwork.connectable = true;
      wifiNetwork.connectionState = ConnectionStateType.kPortal;
      wifiNetwork.portalState = PortalState.kNoInternet;

      mojoApi.setManagedPropertiesForTest(wifiNetwork);
      init();
      return flushAsync().then(() => {
        const networkStateText = getElement('#networkState');
        assertTrue(networkStateText.hasAttribute('warning'));
        assert(networkStateText.textContent);
        assertEquals(
            networkStateText.textContent.trim(),
            internetDetailDialog.i18n(
                'networkListItemConnectedNoConnectivity'));
        const signinButton = getButton('signinButton');
        assertTrue(!!signinButton);
        assertTrue(signinButton.hasAttribute('hidden'));
        assertTrue(signinButton.disabled);
      });
    });

    test('WiFi in a portal suspected portalState', function() {
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi_user');
      wifiNetwork.source = OncSource.kUser;
      wifiNetwork.connectable = true;
      wifiNetwork.connectionState = ConnectionStateType.kPortal;
      wifiNetwork.portalState = PortalState.kPortalSuspected;

      mojoApi.setManagedPropertiesForTest(wifiNetwork);
      init();
      return flushAsync().then(() => {
        const networkStateText = getElement('#networkState');
        assertTrue(networkStateText.hasAttribute('warning'));
        assert(networkStateText.textContent);
        assertEquals(
            networkStateText.textContent.trim(),
            internetDetailDialog.i18n('networkListItemSignIn'));
        const signinButton = getButton('signinButton');
        assertTrue(!!signinButton);
        assertFalse(signinButton.hasAttribute('hidden'));
        assertFalse(signinButton.disabled);
      });
    });
  });

  test('Network not on active sim, hide configurations', async () => {
    await setupCellularNetwork(/*isPrimary=*/ false, /*isInhibited=*/ false);

    await init();
    assertFalse(
        !!internetDetailDialog.shadowRoot!.querySelector<HTMLElement>('.hr'));

    assertFalse(!!internetDetailDialog.shadowRoot!.querySelector<HTMLElement>(
        'network-siminfo'));

    // The 'Forget' and 'ConnectDisconnect' buttons should still be showing.
    assertTrue(!!internetDetailDialog.shadowRoot!.querySelector<HTMLElement>(
        'cr-button'));
  });

  test('Network on active sim, show configurations', async () => {
    await setupCellularNetwork(/*isPrimary=*/ true, /*isInhibited=*/ false);

    await init();
    assertTrue(
        !!internetDetailDialog.shadowRoot!.querySelector<HTMLElement>('.hr'));

    assertTrue(!!internetDetailDialog.shadowRoot!.querySelector<HTMLElement>(
        'network-siminfo'));
  });

  // Syntactic sugar for running test twice with different values for the
  // apnRevamp feature flag.
  [true, false].forEach(isApnRevampEnabled => {
    test(
        `Dialog disabled when inhibited, ApnRevamp enabled is: ${
            isApnRevampEnabled}`,
        async () => {
          loadTimeData.overrideValues({
            apnRevamp: isApnRevampEnabled,
          });

          // Start uninhibited.
          await setupCellularNetwork(
              /*isPrimary=*/ true, /*isInhibited=*/ false);
          await init();

          const connectDisconnectButton =
              getElement<CrButtonElement>('#connectDisconnect');
          const networkSimInfo =
              getElement<NetworkSiminfoElement>('network-siminfo');
          const networkChooseMobile =
              getElement<NetworkChooseMobileElement>('network-choose-mobile');
          let apnList: NetworkApnListElement|null = null;
          if (isApnRevampEnabled) {
            // TODO(b/318561207): Get <apn-list> element.
          } else {
            apnList = getElement<NetworkApnListElement>('network-apnlist');
          }
          const networkProxy = getElement<NetworkProxyElement>('network-proxy');
          const networkIpConfig =
              getElement<NetworkIpConfigElement>('network-ip-config');
          const networkNameservers =
              getElement<NetworkNameserversElement>('network-nameservers');
          const infoFields = getElement<NetworkPropertyListMojoElement>(
              'network-property-list-mojo');

          assertFalse(connectDisconnectButton.disabled);
          assertFalse(networkSimInfo.disabled);
          assertFalse(networkChooseMobile.disabled);
          if (apnList) {
            assertFalse(apnList.disabled);
          }
          assertTrue(networkProxy.editable);
          assertFalse(networkIpConfig.disabled);
          assertFalse(networkNameservers.disabled);
          assertFalse(infoFields.disabled);

          // Mock device being inhibited.
          setDeviceState(
              NetworkType.kCellular,
              DeviceStateType.kEnabled,
              InhibitReason.kInstallingProfile,
              [{
                iccid: testIccid,
                isPrimary: true,
                slotId: 1,
                eid: 'eid',
              }],
          );
          await flushAsync();

          assertTrue(connectDisconnectButton.disabled);
          assertTrue(networkSimInfo.disabled);
          assertTrue(networkChooseMobile.disabled);
          if (apnList) {
            assertTrue(apnList.disabled);
          }
          assertFalse(networkProxy.editable);
          assertTrue(networkIpConfig.disabled);
          assertTrue(networkNameservers.disabled);
          assertTrue(infoFields.disabled);

          // Uninhibit.
          setDeviceState(
              NetworkType.kCellular,
              DeviceStateType.kEnabled,
              InhibitReason.kNotInhibited,
              [{
                iccid: testIccid,
                isPrimary: true,
                slotId: 1,
                eid: 'eid',
              }],
          );
          await flushAsync();

          assertFalse(connectDisconnectButton.disabled);
          assertFalse(networkSimInfo.disabled);
          assertFalse(networkChooseMobile.disabled);
          if (apnList) {
            assertFalse(apnList.disabled);
          }
          assertTrue(networkProxy.editable);
          assertFalse(networkIpConfig.disabled);
          assertFalse(networkNameservers.disabled);
          assertFalse(infoFields.disabled);
        });
  });

  // Syntactic sugar for running test twice with different values for the
  // apnRevamp feature flag.
  [true, false].forEach(isApnRevampEnabled => {
    test('Show/Hide APN row correspondingly to ApnRevamp flag', async () => {
      loadTimeData.overrideValues({
        apnRevamp: isApnRevampEnabled,
      });
      const errorState = 'invalid-apn';
      await setupCellularNetwork(
          /* isPrimary= */ true, /* isInhibited= */ false,
          /* connectedApn= */ undefined, /* customApnList= */ undefined,
          errorState, PortalState.kNoInternet);

      await init();
      const legacyApnElement =
          internetDetailDialog.shadowRoot!.querySelector('network-apnlist');
      const apnSection =
          internetDetailDialog.shadowRoot!.querySelector('cr-expand-button');

      if (isApnRevampEnabled) {
        assertFalse(!!legacyApnElement);
        assertTrue(!!apnSection);
        assertEquals(
            internetDetailDialog.i18n('internetApnPageTitle'),
            getElement('#apnRowTitle').textContent);
        const apnRowSublabel = getElement('#apnRowSublabel');
        const getApnSectionSublabel = () => {
          assert(apnRowSublabel.textContent);
          return apnRowSublabel.textContent.trim();
        };

        assertFalse(!!getApnSectionSublabel());
        const getApnList = () => getElement<ApnListElement>('apn-list');
        assertTrue(getApnList().shouldOmitLinks);
        assertEquals(errorState, getApnList().errorState);
        assertEquals(PortalState.kNoInternet, getApnList().portalState);
        const isApnListShowing = () =>
            getElement<IronCollapseElement>('iron-collapse').opened;

        assertFalse(isApnListShowing());

        // Add a connected APN.
        const accessPointName = 'access point name';
        await setupCellularNetwork(
            /* isPrimary= */ true, /* isInhibited= */ false,
            createApn(accessPointName, ApnSource.kModb));

        // Force a refresh.
        internetDetailDialog.onDeviceStateListChanged();
        await flushAsync();
        assertEquals(accessPointName, getApnSectionSublabel());
        assertFalse(apnRowSublabel.hasAttribute('warning'));
        assertFalse(isApnListShowing());

        // Update the APN's name property and add a restricted connectivity
        // state.
        const name = 'name';
        await setupCellularNetwork(
            /* isPrimary= */ true, /* isInhibited= */ false,
            createApn(accessPointName, ApnSource.kModb, name),
            /* customApnList= */ undefined, /* errorState= */ undefined,
            PortalState.kNoInternet);

        // Force a refresh.
        internetDetailDialog.onDeviceStateListChanged();
        await flushAsync();
        assertEquals(name, getApnSectionSublabel());
        assertTrue(apnRowSublabel.hasAttribute('warning'));
        assertFalse(isApnListShowing());

        // Expand the section, the sublabel should no longer show.
        apnSection.click();
        await flushAsync();
        assertFalse(!!getApnSectionSublabel());
        assertTrue(isApnListShowing());

        // Collapse the section, the sublabel should show.
        apnSection.click();
        await flushAsync();
        assertEquals(name, getApnSectionSublabel());
        assertFalse(isApnListShowing());
      } else {
        assertTrue(!!legacyApnElement);
        assertFalse(!!apnSection);
      }
    });
  });

  [true, false].forEach(isApnRevampAndAllowApnModificationPolicyEnabled => {
    test(
        `Managed APN UI states when ` +
            `isApnRevampAndAllowApnModificationPolicyEnabled is ${
                isApnRevampAndAllowApnModificationPolicyEnabled}`,
        async () => {
          loadTimeData.overrideValues({
            apnRevamp: true,
            isApnRevampAndAllowApnModificationPolicyEnabled:
                isApnRevampAndAllowApnModificationPolicyEnabled,
          });
          await setupCellularNetwork(
              /* isPrimary= */ true, /* isInhibited= */ false);

          await init();
          assertTrue(!!internetDetailDialog.shadowRoot!.querySelector(
              'cr-expand-button'));

          // Check for APN policies managed icon.
          const getApnManagedIcon = () =>
              internetDetailDialog.shadowRoot!.querySelector('#apnManagedIcon');
          assertFalse(!!getApnManagedIcon());
          const apnList =
              internetDetailDialog.shadowRoot!.querySelector<ApnListElement>(
                  '#apnList');
          assertTrue(!!apnList);
          assertFalse(apnList.shouldDisallowApnModification);
          const createCustomApnButton = () =>
              getElement<CrButtonElement>('#createCustomApnButton');
          const discoverMoreApnsButton = () =>
              getElement<CrButtonElement>('#discoverMoreApnsButton');
          assertTrue(!!createCustomApnButton());
          assertFalse(createCustomApnButton().disabled);
          assertTrue(!!discoverMoreApnsButton());
          assertFalse(discoverMoreApnsButton().disabled);

          let globalPolicy = {
            allowApnModification: true,
          } as GlobalPolicy;
          mojoApi.setGlobalPolicy(globalPolicy);
          await flushAsync();
          assertFalse(!!getApnManagedIcon());
          assertFalse(apnList.shouldDisallowApnModification);
          assertFalse(createCustomApnButton().disabled);
          assertFalse(discoverMoreApnsButton().disabled);

          globalPolicy = {
            allowApnModification: false,
          } as GlobalPolicy;
          mojoApi.setGlobalPolicy(globalPolicy);
          await flushAsync();
          assertEquals(
              isApnRevampAndAllowApnModificationPolicyEnabled,
              !!getApnManagedIcon());
          assertEquals(
              isApnRevampAndAllowApnModificationPolicyEnabled,
              apnList.shouldDisallowApnModification);
          assertEquals(
              isApnRevampAndAllowApnModificationPolicyEnabled,
              createCustomApnButton().disabled);
          assertEquals(
              isApnRevampAndAllowApnModificationPolicyEnabled,
              discoverMoreApnsButton().disabled);
        });
  });

  test(
      'Disable and show tooltip for New APN button when custom APNs limit is' +
          ' reached',
      async () => {
        loadTimeData.overrideValues({
          apnRevamp: true,
        });
        await setupCellularNetwork(
            /* isPrimary= */ true, /* isInhibited= */ false,
            createApn(
                /*accessPointName=*/ 'access point name', ApnSource.kModb),
            []);
        await init();
        getElement('cr-expand-button').click();

        const createCustomApnButton = () =>
            getElement<CrButtonElement>('#createCustomApnButton');

        const discoverMoreApnsButton = () =>
            getElement<CrButtonElement>('#discoverMoreApnsButton');

        const getApnTooltip = () =>
            internetDetailDialog.shadowRoot!.querySelector('#apnTooltip');

        assertTrue(!!createCustomApnButton());
        assertFalse(!!getApnTooltip());
        assertFalse(createCustomApnButton().disabled);
        assertTrue(!!discoverMoreApnsButton());
        assertFalse(discoverMoreApnsButton().disabled);

        // We're setting the list of APNs to the max number
        await setupCellularNetwork(
            /* isPrimary= */ true, /* isInhibited= */ false,
            createApn(
                /*accessPointName=*/ 'access point name', ApnSource.kModb),
            Array(MAX_NUM_CUSTOM_APNS)
                .fill(createApn(/*accessPointName=*/ 'apn', ApnSource.kUi)));
        internetDetailDialog.onDeviceStateListChanged();
        await flushAsync();

        assertTrue(createCustomApnButton().disabled);
        assertTrue(discoverMoreApnsButton().disabled);
        const apnTooltip = getApnTooltip();
        assert(apnTooltip);
        assertTrue(apnTooltip.innerHTML.includes(
            internetDetailDialog.i18n('customApnLimitReached')));

        await setupCellularNetwork(
            /* isPrimary= */ true, /* isInhibited= */ false,
            createApn(
                /*accessPointName=*/ 'access point name', ApnSource.kModb),
            []);
        internetDetailDialog.onDeviceStateListChanged();
        await flushAsync();

        assertFalse(!!getApnTooltip());
        assertFalse(createCustomApnButton().disabled);
        assertFalse(discoverMoreApnsButton().disabled);

        createCustomApnButton().click();
        await flushAsync();

        const apnDetailDialog =
            getElement('apn-list')
                .shadowRoot!.querySelector('apn-detail-dialog');
        assertTrue(!!apnDetailDialog);

        const apnDetailDialogCancelBtn =
            apnDetailDialog.shadowRoot!.querySelector<CrButtonElement>(
                '#apnDetailCancelBtn');
        assertTrue(!!apnDetailDialogCancelBtn);
        apnDetailDialogCancelBtn.click();

        discoverMoreApnsButton().click();
        await flushAsync();

        const apnSelectionDialog =
            getElement('apn-list')
                .shadowRoot!.querySelector('apn-selection-dialog');
        assertTrue(!!apnSelectionDialog);
      });

  test('Show toast on show-error-toast event', async function() {
    loadTimeData.overrideValues({
      apnRevamp: true,
    });
    await init();
    const getErrorToast = () => getElement<CrToastElement>('#errorToast');
    assertFalse(getErrorToast().open);

    const message = 'Toast message';
    const event = new CustomEvent('show-error-toast', {detail: message});
    internetDetailDialog.dispatchEvent(event);
    await flushAsync();
    assertTrue(getErrorToast().open);
    assertEquals(getElement('#errorToastMessage').innerHTML, message);
  });

  test(
      'Dont show toast on show-error-toast event when ApnRevamp false',
      async function() {
        loadTimeData.overrideValues({
          apnRevamp: false,
        });
        await init();
        const getErrorToast = () =>
            internetDetailDialog.shadowRoot!.querySelector('#errorToast');
        assertFalse(!!getErrorToast());

        const message = 'Toast message';
        const event = new CustomEvent('show-error-toast', {detail: message});
        internetDetailDialog.dispatchEvent(event);
        await flushAsync();
        assertFalse(!!getErrorToast());
      });

  test('MacAddress not shown when invalid', async function() {
    mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
    const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi_user');
    wifiNetwork.source = OncSource.kUser;
    wifiNetwork.connectable = true;
    wifiNetwork.connectionState = ConnectionStateType.kConnected;

    mojoApi.setManagedPropertiesForTest(wifiNetwork);
    setDeviceState(
        NetworkType.kWiFi, DeviceStateType.kEnabled,
        /*inhibitReason=*/ undefined, /*simInfos=*/ undefined,
        /*macAddress=*/ '01:10:10:10:10:10');
    await flushAsync();

    init();
    await flushAsync();
    let macAddress = getElement('#macAddress');

    assertTrue(!!macAddress);
    assertFalse(macAddress.hidden);

    setDeviceState(
        NetworkType.kWiFi, DeviceStateType.kEnabled,
        /*inhibitReason=*/ undefined, /*simInfos=*/ undefined,
        /*macAddress=*/ '00:00:00:00:00:00');
    await flushAsync();

    macAddress = getElement('#macAddress');
    assertTrue(macAddress.hidden);
  });
});
