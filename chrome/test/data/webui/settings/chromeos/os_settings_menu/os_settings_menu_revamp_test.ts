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
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {AudioOutputCapability, DeviceConnectionState, DeviceType} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertStringContains, assertStringExcludes, assertTrue} from 'chrome://webui-test/chai_assert.js';
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
      setBluetoothConfigForTesting(bluetoothConfig);
    });

    test('Description is not shown when no devices are connected', async () => {
      await createMenu();

      const bluetoothMenuItem = getBluetoothMenuItem();
      assertEquals('', bluetoothMenuItem.sublabel);
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
      assertEquals('', bluetoothMenuItem.sublabel);

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
      assertEquals('', bluetoothMenuItem.sublabel);
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
