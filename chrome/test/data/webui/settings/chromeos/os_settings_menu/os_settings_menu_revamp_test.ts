// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Runs tests for the left menu of CrOS Settings, assuming the
 * kOsSettingsRevampWayfinding feature flag is enabled.
 */

import 'chrome://os-settings/os_settings.js';

import {Account, AccountManagerBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {createPageAvailabilityForTesting, FakeInputDeviceSettingsProvider, fakeKeyboards, fakeMice, fakePointingSticks, fakeTouchpads, MultiDeviceBrowserProxyImpl, MultiDevicePageContentData, MultiDeviceSettingsMode, OsSettingsMenuElement, OsSettingsMenuItemElement, routesMojom, setInputDeviceSettingsProviderForTesting} from 'chrome://os-settings/os_settings.js';
import {setBluetoothConfigForTesting} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {AudioOutputCapability, DeviceConnectionState, DeviceType} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {InhibitReason} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, DeviceStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertStringContains, assertStringExcludes, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {createDefaultBluetoothDevice, FakeBluetoothConfig} from 'chrome://webui-test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {createFakePageContentData, TestMultideviceBrowserProxy} from '../multidevice_page/test_multidevice_browser_proxy.js';
import {TestAccountManagerBrowserProxy} from '../os_people_page/test_account_manager_browser_proxy.js';

const {Section} = routesMojom;
type SectionName = keyof typeof Section;

interface MenuItemData {
  sectionName: SectionName;
  path: string;
}

suite('<os-settings-menu>', () => {
  let settingsMenu: OsSettingsMenuElement;
  let browserProxy: TestAccountManagerBrowserProxy;

  async function createMenu(): Promise<void> {
    settingsMenu = document.createElement('os-settings-menu');
    settingsMenu.pageAvailability = createPageAvailabilityForTesting();
    settingsMenu.advancedOpened = true;
    document.body.appendChild(settingsMenu);

    await browserProxy.whenCalled('getAccounts');
    await flushTasks();
  }

  function queryMenuItemByPath(path: string): OsSettingsMenuItemElement|null {
    return settingsMenu.shadowRoot!.querySelector<OsSettingsMenuItemElement>(
        `os-settings-menu-item[path="${path}"]`);
  }

  setup(() => {
    browserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(browserProxy);
  });

  teardown(() => {
    settingsMenu.remove();
  });

  suite('Menu item visibility', () => {
    setup(async () => {
      await createMenu();
    });

    test('Advanced toggle and collapsible menu are not visible', () => {
      const advancedButton =
          settingsMenu.shadowRoot!.querySelector('#advancedButton');
      assertFalse(isVisible(advancedButton));

      const advancedCollapse =
          settingsMenu.shadowRoot!.querySelector('#advancedCollapse');
      assertFalse(isVisible(advancedCollapse));

      const advancedSubmenu =
          settingsMenu.shadowRoot!.querySelector('#advancedSubmenu');
      assertFalse(isVisible(advancedSubmenu));
    });

    test('About page menu item should always be visible', () => {
      const path = `/${routesMojom.ABOUT_CHROME_OS_SECTION_PATH}`;
      const menuItem = queryMenuItemByPath(path);
      assertTrue(isVisible(menuItem));
    });

    const menuItemData: MenuItemData[] = [
      // Basic pages
      {
        sectionName: 'kNetwork',
        path: `/${routesMojom.NETWORK_SECTION_PATH}`,
      },
      {
        sectionName: 'kBluetooth',
        path: `/${routesMojom.BLUETOOTH_SECTION_PATH}`,
      },
      {
        sectionName: 'kMultiDevice',
        path: `/${routesMojom.MULTI_DEVICE_SECTION_PATH}`,
      },
      {
        sectionName: 'kPeople',
        path: `/${routesMojom.PEOPLE_SECTION_PATH}`,
      },
      {
        sectionName: 'kKerberos',
        path: `/${routesMojom.KERBEROS_SECTION_PATH}`,
      },
      {
        sectionName: 'kDevice',
        path: `/${routesMojom.DEVICE_SECTION_PATH}`,
      },
      {
        sectionName: 'kPersonalization',
        path: `/${routesMojom.PERSONALIZATION_SECTION_PATH}`,
      },
      {
        sectionName: 'kPrivacyAndSecurity',
        path: `/${routesMojom.PRIVACY_AND_SECURITY_SECTION_PATH}`,
      },
      {
        sectionName: 'kApps',
        path: `/${routesMojom.APPS_SECTION_PATH}`,
      },
      {
        sectionName: 'kAccessibility',
        path: `/${routesMojom.ACCESSIBILITY_SECTION_PATH}`,
      },
      {
        sectionName: 'kSystemPreferences',
        path: `/${routesMojom.SYSTEM_PREFERENCES_SECTION_PATH}`,
      },
    ];

    for (const {sectionName, path} of menuItemData) {
      test(`${sectionName} menu item is visible if page is available`, () => {
        // Make page available
        settingsMenu.pageAvailability = {
          ...settingsMenu.pageAvailability,
          [Section[sectionName]]: true,
        };
        flush();

        const menuItem = queryMenuItemByPath(path);
        assertTrue(isVisible(menuItem));
      });

      test(
          `${sectionName} menu item is not visible if page is unavailable`,
          () => {
            // Make page unavailable
            settingsMenu.pageAvailability = {
              ...settingsMenu.pageAvailability,
              [Section[sectionName]]: false,
            };
            flush();

            const menuItem = queryMenuItemByPath(path);
            assertFalse(isVisible(menuItem));
          });
    }
  });

  suite('About ChromeOS menu item', () => {
    test('Description text', async () => {
      await createMenu();

      const aboutMenuItem =
          queryMenuItemByPath(`/${routesMojom.ABOUT_CHROME_OS_SECTION_PATH}`);
      assertTrue(!!aboutMenuItem);

      assertEquals('Updates, help, developer options', aboutMenuItem.sublabel);
    });
  });

  suite('Accounts menu item', () => {
    const fakeAccounts: Account[] = [
      {
        id: '123',
        accountType: 1,
        isDeviceAccount: true,
        isSignedIn: true,
        unmigrated: false,
        isManaged: false,
        fullName: 'Jon Snow',
        pic: 'data:image/png;base64,primaryAccountPicData',
        email: 'jon-snow-test@example.com',
        isAvailableInArc: true,
        organization: 'Stark',
      },
      {
        id: '456',
        accountType: 1,
        isDeviceAccount: false,
        isSignedIn: true,
        unmigrated: false,
        isManaged: false,
        fullName: 'Daenerys Targaryen',
        pic: 'data:image/png;base64,primaryAccountPicData',
        email: 'daenerys-targaryen-test@example.com',
        isAvailableInArc: true,
        organization: 'Targaryen',
      },
    ];

    function getAccountsMenuItem(): OsSettingsMenuItemElement {
      const accountsMenuItem =
          queryMenuItemByPath(`/${routesMojom.PEOPLE_SECTION_PATH}`);
      assertTrue(!!accountsMenuItem);
      return accountsMenuItem;
    }

    suite('When there is only one account', () => {
      setup(() => {
        browserProxy.setAccountsForTesting(fakeAccounts.slice(0, 1));
      });

      test('Description should show account email', async () => {
        await createMenu();

        const accountsMenuItem = getAccountsMenuItem();
        assertEquals(fakeAccounts[0]!.email, accountsMenuItem.sublabel);
      });

      test('Description should update when an account is added', async () => {
        await createMenu();

        const accountsMenuItem = getAccountsMenuItem();
        assertEquals(fakeAccounts[0]!.email, accountsMenuItem.sublabel);

        // Update accounts to have 2 accounts
        browserProxy.setAccountsForTesting(fakeAccounts);
        webUIListenerCallback('accounts-changed');
        await flushTasks();

        assertEquals('2 accounts', accountsMenuItem.sublabel);
      });
    });

    suite('When there is more than one account', () => {
      setup(() => {
        browserProxy.setAccountsForTesting(fakeAccounts);
      });

      test('Description should show number of accounts', async () => {
        await createMenu();

        const accountsMenuItem = getAccountsMenuItem();
        assertEquals('2 accounts', accountsMenuItem.sublabel);
      });

      test('Description should update when an account is removed', async () => {
        await createMenu();

        const accountsMenuItem = getAccountsMenuItem();
        assertEquals('2 accounts', accountsMenuItem.sublabel);

        // Remove an account to leave only 1 account
        browserProxy.setAccountsForTesting(fakeAccounts.slice(0, 1));
        webUIListenerCallback('accounts-changed');
        await flushTasks();

        assertEquals(fakeAccounts[0]!.email, accountsMenuItem.sublabel);
      });
    });
  });

  suite('Apps menu item', () => {
    test('Description text', async () => {
      await createMenu();

      const appsMenuItem =
          queryMenuItemByPath(`/${routesMojom.APPS_SECTION_PATH}`);
      assertTrue(!!appsMenuItem);

      assertEquals('Notifications, Google Play', appsMenuItem.sublabel);
    });
  });

  suite('A11y menu item', () => {
    test('Description text', async () => {
      await createMenu();

      const a11yMenuItem =
          queryMenuItemByPath(`/${routesMojom.ACCESSIBILITY_SECTION_PATH}`);
      assertTrue(!!a11yMenuItem);

      assertEquals('Screen reader, magnification', a11yMenuItem.sublabel);
    });
  });

  suite('Bluetooth menu item', () => {
    let bluetoothConfig: FakeBluetoothConfig;
    const bluetoothMouse = createDefaultBluetoothDevice(
        '111111',
        /*publicName=*/ 'Bluetooth Mouse',
        /*connectionState=*/ DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'My Bluetooth Mouse',
        /*opt_audioCapability=*/ AudioOutputCapability.kNotCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);
    const bluetoothHeadphones = createDefaultBluetoothDevice(
        '222222',
        /*publicName=*/ 'Bluetooth Headphones',
        /*connectionState=*/ DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'My Beats',
        /*opt_audioCapability=*/ AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kHeadset);

    function getBluetoothMenuItem(): OsSettingsMenuItemElement {
      const bluetoothMenuItem =
          queryMenuItemByPath(`/${routesMojom.BLUETOOTH_SECTION_PATH}`);
      assertTrue(!!bluetoothMenuItem);
      return bluetoothMenuItem;
    }

    setup(() => {
      bluetoothConfig = new FakeBluetoothConfig();
      bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);
      setBluetoothConfigForTesting(bluetoothConfig);
    });

    test('Description shows "Off" when bluetooth is off', async () => {
      bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ false);
      await createMenu();

      const bluetoothMenuItem = getBluetoothMenuItem();
      assertEquals('Off', bluetoothMenuItem.sublabel);
    });

    test(
        'Description shows "On" when bluetooth is on and no devices are connected',
        async () => {
          await createMenu();

          const bluetoothMenuItem = getBluetoothMenuItem();
          assertEquals('On', bluetoothMenuItem.sublabel);
        });

    test(
        'Description shows device name for a single connected device',
        async () => {
          bluetoothConfig.appendToPairedDeviceList([bluetoothMouse]);
          await createMenu();

          const bluetoothMenuItem = getBluetoothMenuItem();
          assertEquals(bluetoothMouse.nickname, bluetoothMenuItem.sublabel);
        });

    test('Description shows that multiple devices are connected', async () => {
      bluetoothConfig.appendToPairedDeviceList(
          [bluetoothMouse, bluetoothHeadphones]);
      await createMenu();

      const bluetoothMenuItem = getBluetoothMenuItem();
      assertEquals('2 devices connected', bluetoothMenuItem.sublabel);
    });

    test('Description updates when connected devices change', async () => {
      await createMenu();

      const bluetoothMenuItem = getBluetoothMenuItem();
      assertEquals('On', bluetoothMenuItem.sublabel);

      // Connect a bluetooth mouse.
      bluetoothConfig.appendToPairedDeviceList([bluetoothMouse]);
      await flushTasks();
      assertEquals(bluetoothMouse.nickname, bluetoothMenuItem.sublabel);

      // Connect bluetooth headphones.
      bluetoothConfig.appendToPairedDeviceList([bluetoothHeadphones]);
      await flushTasks();
      assertEquals('2 devices connected', bluetoothMenuItem.sublabel);

      // Disconnect the bluetooth mouse.
      bluetoothConfig.removePairedDevice(bluetoothMouse);
      await flushTasks();
      assertEquals(bluetoothHeadphones.nickname, bluetoothMenuItem.sublabel);

      // Disconnect the bluetooth headphones.
      bluetoothConfig.removePairedDevice(bluetoothHeadphones);
      await flushTasks();
      assertEquals('On', bluetoothMenuItem.sublabel);

      bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ false);
      await flushTasks();
      assertEquals('Off', bluetoothMenuItem.sublabel);
    });
  });

  suite('Device menu item', () => {
    let provider: FakeInputDeviceSettingsProvider;

    function getDeviceMenuItem(): OsSettingsMenuItemElement {
      const deviceMenuItem =
          queryMenuItemByPath(`/${routesMojom.DEVICE_SECTION_PATH}`);
      assertTrue(!!deviceMenuItem);
      return deviceMenuItem;
    }

    setup(() => {
      provider = new FakeInputDeviceSettingsProvider();
      provider.setFakeKeyboards([]);
      provider.setFakeMice([]);
      provider.setFakePointingSticks([]);
      provider.setFakeTouchpads([]);
      setInputDeviceSettingsProviderForTesting(provider);
    });

    test('Description includes "keyboard" when connected', async () => {
      await createMenu();

      // No keyboard connected.
      const deviceMenuItem = getDeviceMenuItem();
      assertStringExcludes(deviceMenuItem.sublabel.toLowerCase(), 'keyboard');

      // Connect a keyboard.
      provider.setFakeKeyboards(fakeKeyboards);
      flush();
      assertStringContains(deviceMenuItem.sublabel.toLowerCase(), 'keyboard');

      // Disconnect the keyboard.
      provider.setFakeKeyboards([]);
      flush();
      assertStringExcludes(deviceMenuItem.sublabel.toLowerCase(), 'keyboard');
    });

    test('Description includes "mouse" when connected', async () => {
      await createMenu();

      // No mouse connected.
      const deviceMenuItem = getDeviceMenuItem();
      assertStringExcludes(deviceMenuItem.sublabel.toLowerCase(), 'mouse');

      // Connect a mouse.
      provider.setFakeMice(fakeMice);
      flush();
      assertStringContains(deviceMenuItem.sublabel.toLowerCase(), 'mouse');

      // Disconnect the mouse.
      provider.setFakeMice([]);
      flush();
      assertStringExcludes(deviceMenuItem.sublabel.toLowerCase(), 'mouse');
    });

    test(
        'Description includes "mouse" when pointing stick is connected',
        async () => {
          await createMenu();

          // No pointing stick connected.
          const deviceMenuItem = getDeviceMenuItem();
          assertStringExcludes(deviceMenuItem.sublabel.toLowerCase(), 'mouse');

          // Connect a pointing stick.
          provider.setFakePointingSticks(fakePointingSticks);
          flush();
          assertStringContains(deviceMenuItem.sublabel.toLowerCase(), 'mouse');

          // Disconnect the pointing stick.
          provider.setFakePointingSticks([]);
          flush();
          assertStringExcludes(deviceMenuItem.sublabel.toLowerCase(), 'mouse');
        });

    test('Description includes "touchpad" when connected', async () => {
      await createMenu();

      // No touchpad connected.
      const deviceMenuItem = getDeviceMenuItem();
      assertStringExcludes(deviceMenuItem.sublabel.toLowerCase(), 'touchpad');

      // Connect a touchpad.
      provider.setFakeTouchpads(fakeTouchpads);
      flush();
      assertStringContains(deviceMenuItem.sublabel.toLowerCase(), 'touchpad');

      // Disconnect the touchpad.
      provider.setFakeTouchpads([]);
      flush();
      assertStringExcludes(deviceMenuItem.sublabel.toLowerCase(), 'touchpad');
    });

    test('Description prioritizes "mouse" over "touchpad"', async () => {
      await createMenu();

      // No mouse or touchpad connected.
      const deviceMenuItem = getDeviceMenuItem();
      assertStringExcludes(deviceMenuItem.sublabel.toLowerCase(), 'mouse');
      assertStringExcludes(deviceMenuItem.sublabel.toLowerCase(), 'touchpad');

      // Connect a touchpad.
      provider.setFakeTouchpads(fakeTouchpads);
      flush();
      assertStringExcludes(deviceMenuItem.sublabel.toLowerCase(), 'mouse');
      assertStringContains(deviceMenuItem.sublabel.toLowerCase(), 'touchpad');

      // Connect a mouse.
      provider.setFakeMice(fakeMice);
      flush();
      assertStringContains(deviceMenuItem.sublabel.toLowerCase(), 'mouse');
      assertStringExcludes(deviceMenuItem.sublabel.toLowerCase(), 'touchpad');
    });

    test('Description includes print and display by default', async () => {
      await createMenu();
      const deviceMenuItem = getDeviceMenuItem();
      assertEquals('Print, display', deviceMenuItem.sublabel);
    });

    test(
        'Description shows at most 3 words when devices are connected',
        async () => {
          // 4 devices connected.
          provider.setFakeKeyboards(fakeKeyboards);
          provider.setFakeMice(fakeMice);
          provider.setFakePointingSticks(fakePointingSticks);
          provider.setFakeTouchpads(fakeTouchpads);

          await createMenu();
          const deviceMenuItem = getDeviceMenuItem();
          assertEquals('Keyboard, mouse, print', deviceMenuItem.sublabel);
        });

    test('Description shows "Keyboard, print, display', async () => {
      provider.setFakeKeyboards(fakeKeyboards);
      provider.setFakeMice([]);
      provider.setFakePointingSticks([]);
      provider.setFakeTouchpads([]);

      await createMenu();
      const deviceMenuItem = getDeviceMenuItem();
      assertEquals('Keyboard, print, display', deviceMenuItem.sublabel);
    });

    test('Description shows "Mouse, print, display"', async () => {
      provider.setFakeKeyboards([]);
      provider.setFakeMice(fakeMice);
      provider.setFakePointingSticks([]);
      provider.setFakeTouchpads(fakeTouchpads);

      await createMenu();
      const deviceMenuItem = getDeviceMenuItem();
      assertEquals('Mouse, print, display', deviceMenuItem.sublabel);

      // Still show "mouse" if pointing stick is connected.
      provider.setFakeKeyboards([]);
      provider.setFakeMice([]);
      provider.setFakePointingSticks(fakePointingSticks);
      provider.setFakeTouchpads(fakeTouchpads);
      flush();

      assertEquals('Mouse, print, display', deviceMenuItem.sublabel);
    });

    test('Description shows "Touchpad, print, display', async () => {
      provider.setFakeKeyboards([]);
      provider.setFakeMice([]);
      provider.setFakePointingSticks([]);
      provider.setFakeTouchpads(fakeTouchpads);

      await createMenu();
      const deviceMenuItem = getDeviceMenuItem();
      assertEquals('Touchpad, print, display', deviceMenuItem.sublabel);
    });
  });

  suite('Internet menu item', () => {
    let networkConfigRemote: FakeNetworkConfig;
    const ethernetNetwork =
        OncMojo.getDefaultNetworkState(NetworkType.kEthernet, 'ethernet');
    const wifiNetwork =
        OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi');
    const tetherNetwork =
        OncMojo.getDefaultNetworkState(NetworkType.kTether, 'tether');
    const vpnNetwork = OncMojo.getDefaultNetworkState(NetworkType.kVPN, 'vpn');

    function getInternetMenuItem(): OsSettingsMenuItemElement {
      const internetMenuItem =
          queryMenuItemByPath(`/${routesMojom.NETWORK_SECTION_PATH}`);
      assertTrue(!!internetMenuItem);
      return internetMenuItem;
    }

    async function addConnectedNetworks(networkTypes: NetworkType[]):
        Promise<void> {
      const networkStates: OncMojo.NetworkStateProperties[] = [];
      for (const type of networkTypes) {
        switch (type) {
          case NetworkType.kEthernet:
            networkStates.push({
              ...ethernetNetwork,
              connectionState: ConnectionStateType.kConnected,
            });
            break;
          case NetworkType.kWiFi:
            networkStates.push({
              ...wifiNetwork,
              connectionState: ConnectionStateType.kConnected,
            });
            break;
          case NetworkType.kTether:
            networkStates.push({
              ...tetherNetwork,
              connectionState: ConnectionStateType.kConnected,
            });
            break;
          case NetworkType.kVPN:
            networkStates.push({
              ...vpnNetwork,
              connectionState: ConnectionStateType.kConnected,
            });
            break;
          default:
            break;
        }
      }
      networkConfigRemote.addNetworksForTest(networkStates);
      await flushTasks();
    }

    function getCellularDeviceStateProps(): OncMojo.DeviceStateProperties {
      return {
        ipv4Address: undefined,
        ipv6Address: undefined,
        imei: undefined,
        macAddress: undefined,
        scanning: false,
        simLockStatus: undefined,
        simInfos: undefined,
        inhibitReason: InhibitReason.kNotInhibited,
        simAbsent: false,
        deviceState: DeviceStateType.kDisabled,
        type: NetworkType.kCellular,
        managedNetworkAvailable: false,
        serial: undefined,
        isCarrierLocked: false,
      };
    }

    setup(() => {
      networkConfigRemote = new FakeNetworkConfig();
      MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
          networkConfigRemote);
    });

    test('Ethernet takes priority when it is connected', async () => {
      await createMenu();

      await addConnectedNetworks([
        NetworkType.kEthernet,
        NetworkType.kWiFi,
        NetworkType.kTether,
        NetworkType.kVPN,
      ]);

      const internetMenuItem = getInternetMenuItem();
      assertEquals(ethernetNetwork.name, internetMenuItem.sublabel);
    });

    test(
        'Wifi takes priority when ethernet network is not connected',
        async () => {
          await createMenu();

          await addConnectedNetworks(
              [NetworkType.kWiFi, NetworkType.kTether, NetworkType.kVPN]);

          const internetMenuItem = getInternetMenuItem();
          assertEquals(wifiNetwork.name, internetMenuItem.sublabel);
        });

    test(
        'Tether takes priority when neither ethernet nor wifi network is connected',
        async () => {
          await createMenu();

          await addConnectedNetworks([NetworkType.kTether, NetworkType.kVPN]);

          const internetMenuItem = getInternetMenuItem();
          assertEquals(tetherNetwork.name, internetMenuItem.sublabel);
        });

    test('VPN shows when it is the only network connected', async () => {
      await createMenu();

      await addConnectedNetworks([NetworkType.kVPN]);

      const internetMenuItem = getInternetMenuItem();
      assertEquals(vpnNetwork.name, internetMenuItem.sublabel);
    });

    test(
        'Wifi internet description shows when no network connected and the device does not support mobile data',
        async () => {
          await createMenu();

          const internetMenuItem = getInternetMenuItem();
          assertEquals('Wi-Fi', internetMenuItem.sublabel);
        });

    test(
        'Wifi and mobile data internet description shows when no network connected but the device supports mobile data',
        async () => {
          networkConfigRemote.setDeviceStateForTest({
            ...getCellularDeviceStateProps(),
          });

          await createMenu();

          const internetMenuItem = getInternetMenuItem();
          assertEquals('Wi-Fi, mobile data', internetMenuItem.sublabel);
        });

    test(
        'Internet description updates dynamically on networks connection updates',
        async () => {
          networkConfigRemote.setDeviceStateForTest({
            ...getCellularDeviceStateProps(),
          });

          await createMenu();
          const internetMenuItem = getInternetMenuItem();

          await addConnectedNetworks([NetworkType.kTether]);

          // Tether network is the only connected network, internet description
          // is the name of the Tether network.
          assertEquals(tetherNetwork.name, internetMenuItem.sublabel);

          // Connect the Wi-Fi network, now Wi-Fi network takes priority.
          await addConnectedNetworks([NetworkType.kWiFi]);
          assertEquals(wifiNetwork.name, internetMenuItem.sublabel);

          // Connect the Ethernet network and remove Wi-Fi network. The Ethernet
          // network should take priority.
          await addConnectedNetworks([NetworkType.kEthernet]);
          networkConfigRemote.removeNetworkForTest(wifiNetwork);
          await flushTasks();
          assertEquals(ethernetNetwork.name, internetMenuItem.sublabel);

          // Remove the Ethernet network and connect the VPN network. The Tether
          // network should take priority now.
          await addConnectedNetworks([NetworkType.kVPN]);
          networkConfigRemote.removeNetworkForTest(ethernetNetwork);
          await flushTasks();
          assertEquals(tetherNetwork.name, internetMenuItem.sublabel);

          // Remove the Tether network. VPN network is the only connected
          // network now.
          networkConfigRemote.removeNetworkForTest(tetherNetwork);
          await flushTasks();
          assertEquals(vpnNetwork.name, internetMenuItem.sublabel);

          // Remove the VPN network, the default description "Wi-Fi, mobile
          // data" should be shown.
          networkConfigRemote.removeNetworkForTest(vpnNetwork);
          await flushTasks();
          assertEquals('Wi-Fi, mobile data', internetMenuItem.sublabel);
        });
  });

  suite('Multidevice menu item', () => {
    let multideviceBrowserProxy: TestMultideviceBrowserProxy;

    function getMultideviceMenuItem(): OsSettingsMenuItemElement {
      const multideviceMenuItem =
          queryMenuItemByPath(`/${routesMojom.MULTI_DEVICE_SECTION_PATH}`);
      assertTrue(!!multideviceMenuItem);
      return multideviceMenuItem;
    }

    /**
     * Sets pageContentData via WebUI Listener and flushes.
     */
    function setPageContentData(newPageContentData: MultiDevicePageContentData):
        void {
      webUIListenerCallback(
          'settings.updateMultidevicePageContentData', newPageContentData);
      flush();
    }

    setup(() => {
      multideviceBrowserProxy = new TestMultideviceBrowserProxy();
      MultiDeviceBrowserProxyImpl.setInstanceForTesting(
          multideviceBrowserProxy);
    });

    test('Default description shows for NO_ELIGIBLE_HOSTS status', async () => {
      await createMenu();

      const multideviceMenuItem = getMultideviceMenuItem();

      setPageContentData(
          createFakePageContentData(MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS));
      assertEquals('Phone Hub, Nearby Share', multideviceMenuItem.sublabel);
    });


    test('Default description shows for NO_HOST_SET status', async () => {
      await createMenu();

      const multideviceMenuItem = getMultideviceMenuItem();

      setPageContentData(
          createFakePageContentData(MultiDeviceSettingsMode.NO_HOST_SET));
      assertEquals('Phone Hub, Nearby Share', multideviceMenuItem.sublabel);
    });

    test(
        'Default description shows for HOST_SET_WAITING_FOR_SERVER status',
        async () => {
          await createMenu();

          const multideviceMenuItem = getMultideviceMenuItem();

          setPageContentData(createFakePageContentData(
              MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER));
          assertEquals('Phone Hub, Nearby Share', multideviceMenuItem.sublabel);
        });

    test(
        'Default description shows for HOST_SET_WAITING_FOR_VERIFICATION status',
        async () => {
          await createMenu();

          const multideviceMenuItem = getMultideviceMenuItem();

          setPageContentData(createFakePageContentData(
              MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION));
          assertEquals('Phone Hub, Nearby Share', multideviceMenuItem.sublabel);
        });

    test(
        'Phone connected description shows for HOST_SET_VERIFIED status',
        async () => {
          await createMenu();

          const multideviceMenuItem = getMultideviceMenuItem();

          const deviceName = 'Google pixel phone';
          setPageContentData(createFakePageContentData(
              MultiDeviceSettingsMode.HOST_SET_VERIFIED, deviceName));
          assertEquals(
              `Connected to ${deviceName}`, multideviceMenuItem.sublabel);
        });

    test(
        'Android phone connected description shows when the device name is missing',
        async () => {
          await createMenu();

          const multideviceMenuItem = getMultideviceMenuItem();

          const pageContentData = createFakePageContentData(
              MultiDeviceSettingsMode.HOST_SET_VERIFIED);
          pageContentData.hostDeviceName = '';
          setPageContentData(pageContentData);
          assertEquals(
              `Connected to Android phone`, multideviceMenuItem.sublabel);
        });

    test(
        'Multidevice menu item description updates on device connection changes',
        async () => {
          await createMenu();

          const multideviceMenuItem = getMultideviceMenuItem();

          // No eligible device found, show the default description "Phone Hub,
          // Nearby Share".
          setPageContentData(createFakePageContentData(
              MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS));
          assertEquals('Phone Hub, Nearby Share', multideviceMenuItem.sublabel);

          // No device connected, show the default description "Phone Hub,
          // Nearby Share".
          setPageContentData(
              createFakePageContentData(MultiDeviceSettingsMode.NO_HOST_SET));
          assertEquals('Phone Hub, Nearby Share', multideviceMenuItem.sublabel);

          // Device connection is waiting for server, show the default
          // description "Phone Hub, Nearby Share".
          setPageContentData(createFakePageContentData(
              MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER));
          assertEquals('Phone Hub, Nearby Share', multideviceMenuItem.sublabel);

          // Device connection is waiting for verification, show the default
          // description "Phone Hub, Nearby Share".
          setPageContentData(createFakePageContentData(
              MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION));
          assertEquals('Phone Hub, Nearby Share', multideviceMenuItem.sublabel);

          // Device is connected, show the phone connected description
          // "Connected to <phone name>".
          const deviceName = 'Google pixel phone';
          setPageContentData(createFakePageContentData(
              MultiDeviceSettingsMode.HOST_SET_VERIFIED, deviceName));
          assertEquals(
              `Connected to ${deviceName}`, multideviceMenuItem.sublabel);

          // Disconnect the Device, the description should be updated to the
          // default "Phone Hub, Nearby Share".
          setPageContentData(
              createFakePageContentData(MultiDeviceSettingsMode.NO_HOST_SET));
          assertEquals('Phone Hub, Nearby Share', multideviceMenuItem.sublabel);
        });
  });

  suite('Privacy menu item', () => {
    test('Privacy menu item description', async () => {
      await createMenu();

      const privacyMenuItem = queryMenuItemByPath(
          `/${routesMojom.PRIVACY_AND_SECURITY_SECTION_PATH}`);
      assertTrue(!!privacyMenuItem);

      assertEquals('Lock screen, controls', privacyMenuItem.sublabel);
    });
  });

  suite('System preferences menu item', () => {
    test('Description text', async () => {
      await createMenu();

      const systemPreferencesMenuItem = queryMenuItemByPath(
          `/${routesMojom.SYSTEM_PREFERENCES_SECTION_PATH}`);
      assertTrue(!!systemPreferencesMenuItem);

      assertEquals(
          'Storage, power, language', systemPreferencesMenuItem.sublabel);
    });
  });
});
