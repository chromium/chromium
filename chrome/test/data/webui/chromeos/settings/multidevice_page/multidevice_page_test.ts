// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';
import 'chrome://os-settings/os_settings.js';

import {SettingsMultideviceSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {MultiDeviceBrowserProxyImpl, MultiDeviceFeature, MultiDeviceFeatureState, MultiDevicePageContentData, MultiDeviceSettingsMode, PhoneHubFeatureAccessStatus, Router, routes, setContactManagerForTesting, setNearbyShareSettingsForTesting, settingMojom, SettingsMultidevicePageElement} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {Visibility} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeContactManager} from 'chrome://webui-test/chromeos/nearby_share/shared/fake_nearby_contact_manager.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/chromeos/nearby_share/shared/fake_nearby_share_settings.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {createFakePageContentData, HOST_DEVICE, TestMultideviceBrowserProxy} from './test_multidevice_browser_proxy.js';

suite('<settings-multidevice-page>', () => {
  const isRevampWayfindingEnabled =
      loadTimeData.getBoolean('isRevampWayfindingEnabled');
  let multidevicePage: SettingsMultidevicePageElement;
  let browserProxy: TestMultideviceBrowserProxy;
  let ALL_MODES: MultiDeviceSettingsMode[];
  let fakeContactManager: FakeContactManager;
  let fakeSettings: FakeNearbyShareSettings;

  /**
   * Sets pageContentData via WebUI Listener and flushes.
   */
  function setPageContentData(newPageContentData: MultiDevicePageContentData):
      void {
    webUIListenerCallback(
        'settings.updateMultidevicePageContentData', newPageContentData);
    flush();
  }

  /**
   * Sets screen lock status via WebUI Listener and flushes.
   */
  function setScreenLockStatus(
      chromeStatus: boolean, phoneStatus: boolean): void {
    webUIListenerCallback('settings.OnEnableScreenLockChanged', chromeStatus);
    webUIListenerCallback('settings.OnScreenLockStatusChanged', phoneStatus);
    flush();
  }

  /**
   * Sets pageContentData to the specified mode. If it is a mode corresponding
   * to a set host, it will set the hostDeviceName to the provided name or else
   * default to HOST_DEVICE.
   * @param newHostDeviceName Overrides default if |newMode|
   *     corresponds to a set host.
   */
  function setHostData(
      newMode: MultiDeviceSettingsMode, newHostDeviceName?: string): void {
    setPageContentData(createFakePageContentData(newMode, newHostDeviceName));
  }

  function setSuiteState(newState: MultiDeviceFeatureState): void {
    setPageContentData({
      ...multidevicePage.pageContentData,
      betterTogetherState: newState,
    });
  }

  function setSmartLockState(newState: MultiDeviceFeatureState): void {
    setPageContentData({
      ...multidevicePage.pageContentData,
      smartLockState: newState,
    });
  }

  function setPhoneHubNotificationsState(newState: MultiDeviceFeatureState):
      void {
    setPageContentData({
      ...multidevicePage.pageContentData,
      phoneHubNotificationsState: newState,
    });
  }

  function setPhoneHubNotificationAccessGranted(accessGranted: boolean): void {
    const accessState = accessGranted ?
        PhoneHubFeatureAccessStatus.ACCESS_GRANTED :
        PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED;
    setPageContentData({
      ...multidevicePage.pageContentData,
      notificationAccessStatus: accessState,
    });
  }

  function setNearbyShareIsOnboardingComplete(isOnboardingComplete: boolean):
      void {
    multidevicePage.setPrefValue(
        'nearby_sharing.onboarding_complete', isOnboardingComplete);
    flush();
  }

  function setNearbyShareEnabled(enabled: boolean): void {
    multidevicePage.setPrefValue('nearby_sharing.enabled', enabled);
    flush();
  }

  function setNearbyShareDisallowedByPolicy(isDisallowedByPolicy: boolean):
      void {
    setPageContentData({
      ...multidevicePage.pageContentData,
      isNearbyShareDisallowedByPolicy: isDisallowedByPolicy,
    });
  }

  function setPhoneHubPermissionsDialogSupported(enabled: boolean): void {
    setPageContentData({
      ...multidevicePage.pageContentData,
      isPhoneHubPermissionsDialogSupported: enabled,
    });
  }

  function getNearbyShareDisabledDescription(): string {
    // If the page is first with NS enabled, then there will be two
    // elements with id nearbyShareSecondary: in this case, the second will have
    // the off description. There should always be at least one element present
    // with that id.
    const nearbyShareSecondaryDisabledList =
        multidevicePage.shadowRoot!.querySelectorAll('#nearbyShareSecondary');
    const nearbyShareSecondaryDisabled =
        nearbyShareSecondaryDisabledList[nearbyShareSecondaryDisabledList.length - 1];
    assertTrue(!!nearbyShareSecondaryDisabled);

    const disabledLocalizedLink =
        nearbyShareSecondaryDisabled.querySelector('localized-link');
    assertTrue(!!disabledLocalizedLink);

    const disabledDescription =
        disabledLocalizedLink.shadowRoot!.querySelector<HTMLElement>(
            '#container');
    assertTrue(!!disabledDescription);
    return disabledDescription.innerText.trim();
  }

  /**
   * @param feature The feature to change.
   * @param enabled Whether to enable or disable the feature.
   * @param authRequired Whether authentication is required for the
   *     change.
   * @return Promise which resolves when the state change has been
   *     verified.
   */
  async function simulateFeatureStateChangeRequest(
      feature: MultiDeviceFeature, enabled: boolean,
      authRequired: boolean): Promise<void> {
    // When the user requests a feature state change, an event with the relevant
    // details is handled.
    multidevicePage.dispatchEvent(new CustomEvent('feature-toggle-clicked', {
      bubbles: true,
      composed: true,
      detail: {feature, enabled},
    }));
    flush();

    if (authRequired) {
      assertTrue(multidevicePage.get('showPasswordPromptDialog_'));
      const prompt = multidevicePage.shadowRoot!.querySelector(
          '#multidevicePasswordPrompt');
      assertTrue(!!prompt);
      // Simulate the user entering a valid password, then closing the dialog.
      prompt.dispatchEvent(new CustomEvent('token-obtained', {
        bubbles: true,
        composed: true,
        detail: {token: 'validAuthToken', lifetimeSeconds: 300},
      }));
      // Simulate closing the password prompt dialog
      prompt.dispatchEvent(
          new CustomEvent('close', {bubbles: true, composed: true}));
      flush();
    }

    if (enabled && feature === MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS) {
      const accessDialog =
          multidevicePage.pageContentData.isPhoneHubPermissionsDialogSupported ?
          multidevicePage.shadowRoot!.querySelector(
              'settings-multidevice-permissions-setup-dialog') :
          multidevicePage.shadowRoot!.querySelector(
              'settings-multidevice-notification-access-setup-dialog');
      assertEquals(
          !!accessDialog,
          multidevicePage.pageContentData.notificationAccessStatus ===
              PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED);
      return;
    }

    assertFalse(multidevicePage.get('showPasswordPromptDialog_'));
    const params = await browserProxy.whenCalled('setFeatureEnabledState');
    assertEquals(feature, params[0]);
    assertEquals(enabled, params[1]);
    // Reset the resolver so that setFeatureEnabledState() can be called
    // multiple times in a test.
    browserProxy.resetResolver('setFeatureEnabledState');
  }

  /**
   * Sets up Quick Share v2 tests which require the QuickShareV2 flag to be
   * enabled on page load.
   */
  async function setupQuickShareV2() {
    multidevicePage.remove();
    loadTimeData.overrideValues({'isQuickShareV2Enabled': true});
    await init();
  }

  suiteSetup(() => {
    ALL_MODES = Object.values(MultiDeviceSettingsMode)
                    .filter((item) => typeof item === 'number') as
        MultiDeviceSettingsMode[];

    fakeContactManager = new FakeContactManager();
    setContactManagerForTesting(fakeContactManager);
    fakeContactManager.setupContactRecords();

    fakeSettings = new FakeNearbyShareSettings();
    fakeSettings.setEnabled(true);
    setNearbyShareSettingsForTesting(fakeSettings);

    browserProxy = new TestMultideviceBrowserProxy();
    MultiDeviceBrowserProxyImpl.setInstanceForTesting(browserProxy);
  });

  async function init(): Promise<void> {
    multidevicePage = document.createElement('settings-multidevice-page');
    multidevicePage.prefs = {
      'nearby_sharing': {
        'onboarding_complete': {
          value: false,
        },
        'enabled': {
          value: false,
        },
      },
    };

    document.body.appendChild(multidevicePage);
    flush();
    await browserProxy.whenCalled('getPageContentData');
  }

  setup(async () => {
    loadTimeData.overrideValues({
      isNearbyShareSupported: true,
      isChromeosScreenLockEnabled: false,
      isPhoneScreenLockEnabled: false,
      // TODO(b/350547931): Permanently enable QSv2, remove flag and need to
      // override it.
      isQuickShareV2Enabled: false,
    });
    await init();
  });

  teardown(() => {
    multidevicePage.remove();
    browserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  function getLabel(): string {
    const element = multidevicePage.shadowRoot!.querySelector<HTMLElement>(
        '#multideviceLabel');
    assertTrue(!!element);
    return element.innerText.trim();
  }

  function getSublabel(): string {
    const element =
        multidevicePage.shadowRoot!.querySelector('#multideviceSubLabel')!
            .shadowRoot!.querySelector<HTMLElement>('#container');
    assertTrue(!!element);
    return element.innerText.trim();
  }

  function getSubpage(): SettingsMultideviceSubpageElement|null {
    return multidevicePage.shadowRoot!.querySelector(
        'settings-multidevice-subpage');
  }

  function getSubpageWrapper(): HTMLElement|null {
    return multidevicePage.shadowRoot!.querySelector<HTMLElement>(
        '#settingsMultideviceSubpageWrapper');
  }

  function getNearbyShareSecondary(): HTMLElement {
    const nearbyShareSecondary =
        multidevicePage.shadowRoot!.querySelector<HTMLElement>(
            '#nearbyShareSecondary');
    assertTrue(!!nearbyShareSecondary);
    return nearbyShareSecondary;
  }

  suite('nearby share description updates with isRevampWayfindingEnabled enabled', () => {
    setup(async () => {
      loadTimeData.overrideValues(
          {isNearbyShareSupported: true, isRevampWayfindingEnabled: true});

      await init();

      setNearbyShareDisallowedByPolicy(false);
      setNearbyShareIsOnboardingComplete(true);
      await flushTasks();
    });

    test(
        'nearby share off description shows when nearby share is disabled',
        () => {
          setNearbyShareEnabled(false);
          flush();

          const disabledDescription = getNearbyShareDisabledDescription();
          assertEquals(
              'Share files and more with nearby devices. Learn more',
              disabledDescription);
        });

    test(
        'nearby share visible to all description shows when visible to all contacts is selected',
        async () => {
          setNearbyShareEnabled(true);
          fakeSettings.setVisibility(Visibility.kAllContacts);
          await flushTasks();

          const nearbyShareSecondary = getNearbyShareSecondary();
          assertEquals(
              'Visible to all contacts', nearbyShareSecondary.innerText.trim());
        });

    test(
        'nearby share visible to all description shows when it is only visible to selected contacts',
        async () => {
          setNearbyShareEnabled(true);
          fakeSettings.setVisibility(Visibility.kSelectedContacts);
          await flushTasks();

          const nearbyShareSecondary = getNearbyShareSecondary();
          assertTrue(!!nearbyShareSecondary);
          assertEquals(
              'Visible to some contacts',
              nearbyShareSecondary.innerText.trim());
        });

    test(
        'nearby share visible to your devices shows when it is only visible to your devices',
        async () => {
          setNearbyShareEnabled(true);
          fakeSettings.setVisibility(Visibility.kYourDevices);
          await flushTasks();

          const nearbyShareSecondary = getNearbyShareSecondary();
          assertTrue(!!nearbyShareSecondary);
          assertEquals(
              'Visible to your devices', nearbyShareSecondary.innerText.trim());
        });

    test(
        'nearby share hidden description shows when no contact is selected',
        async () => {
          setNearbyShareEnabled(true);
          fakeSettings.setVisibility(Visibility.kNoOne);
          await flushTasks();

          const nearbyShareSecondary = getNearbyShareSecondary();
          assertTrue(!!nearbyShareSecondary);
          assertEquals('Hidden', nearbyShareSecondary.innerText.trim());
        });

    test(
        'nearby share description updates on visibility or enable states change',
        async () => {
          setNearbyShareEnabled(true);
          fakeSettings.setVisibility(Visibility.kNoOne);
          await flushTasks();

          const nearbyShareSecondaryEnabled = getNearbyShareSecondary();
          assertTrue(!!nearbyShareSecondaryEnabled);
          assertEquals('Hidden', nearbyShareSecondaryEnabled.innerText.trim());

          fakeSettings.setVisibility(Visibility.kAllContacts);
          await flushTasks();
          assertEquals(
              'Visible to all contacts',
              nearbyShareSecondaryEnabled.innerText.trim());

          fakeSettings.setVisibility(Visibility.kYourDevices);
          await flushTasks();
          assertEquals(
              'Visible to your devices',
              nearbyShareSecondaryEnabled.innerText.trim());

          setNearbyShareEnabled(false);
          flush();
          const disabledDescription = getNearbyShareDisabledDescription();
          assertEquals(
              'Share files and more with nearby devices. Learn more',
              disabledDescription);

          setNearbyShareEnabled(true);
          flush();
          fakeSettings.setVisibility(Visibility.kSelectedContacts);
          await flushTasks();
          assertEquals(
              'Visible to some contacts',
              nearbyShareSecondaryEnabled.innerText.trim());
        });
  });

  test('clicking setup shows multidevice setup dialog', async () => {
    setHostData(MultiDeviceSettingsMode.NO_HOST_SET);
    const button = multidevicePage.shadowRoot!.querySelector('cr-button');
    assertTrue(!!button);
    button.click();
    await browserProxy.whenCalled('showMultiDeviceSetupDialog');
  });

  test('Deep link to multidevice setup', async () => {
    setHostData(MultiDeviceSettingsMode.NO_HOST_SET);

    const SETTING_ID_200 = settingMojom.Setting.kSetUpMultiDevice.toString();
    const params = new URLSearchParams();
    params.append('settingId', SETTING_ID_200);
    Router.getInstance().navigateTo(routes.MULTIDEVICE, params);

    flush();

    const deepLinkElement =
        multidevicePage.shadowRoot!.querySelector('cr-button');
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        `Setup multidevice button should be focused for settingId=${
            SETTING_ID_200}.`);
  });

  test('Open notification access setup dialog route param', async () => {
    Router.getInstance().navigateTo(
        routes.MULTIDEVICE_FEATURES,
        new URLSearchParams('showPhonePermissionSetupDialog=true'));

    browserProxy.setNotificationAccessStatusForTesting(
        PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED);

    await init();

    const dialog = multidevicePage.shadowRoot!.querySelector(
        'settings-multidevice-notification-access-setup-dialog');
    assertTrue(!!dialog);

    // Close the dialog.
    dialog.$.dialog.close();
    await flushTasks();

    // Check the subpage is focused on dialog close.
    assertEquals(
        getSubpageWrapper(), getDeepActiveElement(),
        'subpage wrapper should be focused.');

    // A change in pageContentData will not cause the notification access
    // setup dialog to reappear
    setHostData(MultiDeviceSettingsMode.NO_HOST_SET);
    flush();

    assertNull(multidevicePage.shadowRoot!.querySelector(
        'settings-multidevice-notification-access-setup-dialog'));
  });

  test('Open multidevice permissions setup dialog route param', async () => {
    Router.getInstance().navigateTo(
        routes.MULTIDEVICE_FEATURES,
        new URLSearchParams('showPhonePermissionSetupDialog&mode=1'));

    browserProxy.setNotificationAccessStatusForTesting(
        PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED);
    browserProxy.setIsPhoneHubPermissionsDialogSupportedForTesting(true);

    await init();

    assertTrue(!!multidevicePage.shadowRoot!.querySelector(
        'settings-multidevice-permissions-setup-dialog'));

    // Close the dialog.
    multidevicePage.set('showPhonePermissionSetupDialog_', false);
    flush();

    // A change in pageContentData will not cause the multidevice permissions
    // setup dialog to reappear.
    setHostData(MultiDeviceSettingsMode.NO_HOST_SET);
    flush();

    assertNull(multidevicePage.shadowRoot!.querySelector(
        'settings-multidevice-permissions-setup-dialog'));
  });

  if (isRevampWayfindingEnabled) {
    test('Label always shows "Android phone" for all modes', () => {
      for (const mode of ALL_MODES) {
        setHostData(mode);
        assertEquals('Android phone', getLabel());
      }
    });
  } else {
    test('Label changes based on mode and host', () => {
      for (const mode of ALL_MODES) {
        setHostData(mode);
        assertEquals(multidevicePage.isHostSet(), getLabel() === HOST_DEVICE);
      }
    });
  }

  if (isRevampWayfindingEnabled) {
    test('Host device name displayed updates if the device is changed', () => {
      setHostData(MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals('Android phone', getLabel());
      assertEquals(HOST_DEVICE, getSublabel());

      const anotherHost = `Super Duper ${HOST_DEVICE}`;
      setHostData(MultiDeviceSettingsMode.HOST_SET_VERIFIED, anotherHost);
      assertEquals('Android phone', getLabel());
      assertEquals(anotherHost, getSublabel());
    });

    test('Labels for no eligible host device', () => {
      setHostData(MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS);
      assertEquals('Android phone', getLabel());
      assertEquals(
          'No available devices. Add your Google Account to your phone to ' +
              'connect it to this Chrome device. Learn more',
          getSublabel());
    });
  } else {
    test('changing host device changes label', () => {
      setHostData(MultiDeviceSettingsMode.HOST_SET_VERIFIED);
      assertEquals(HOST_DEVICE, getLabel());

      const anotherHost = `Super Duper ${HOST_DEVICE}`;
      setHostData(MultiDeviceSettingsMode.HOST_SET_VERIFIED, anotherHost);
      assertEquals(anotherHost, getLabel());
    });

    test('Labels for no eligible host device', () => {
      setHostData(MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS);
      assertEquals('Android phone', getLabel());
      assertEquals('No eligible devices. Learn more', getSublabel());
    });
  }

  test('item is actionable if and only if a host is set', () => {
    for (const mode of ALL_MODES) {
      setHostData(mode);
      const suiteLinkWrapper =
          multidevicePage.shadowRoot!.querySelector('#suiteLinkWrapper');
      assertTrue(!!suiteLinkWrapper);
      assertEquals(
          multidevicePage.isHostSet(),
          suiteLinkWrapper.hasAttribute('actionable'));
    }
  });

  test('clicking item with verified host opens subpage with features', () => {
    setHostData(MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    assertNull(getSubpage());
    const suiteLinkWrapper =
        multidevicePage.shadowRoot!.querySelector<HTMLElement>(
            '#suiteLinkWrapper');
    assertTrue(!!suiteLinkWrapper);
    suiteLinkWrapper.click();
    assertTrue(!!getSubpage());
    assertTrue(!!getSubpage()!.shadowRoot!.querySelector(
        'settings-multidevice-feature-item'));
  });

  test(
      'clicking item with unverified set host opens subpage without features',
      () => {
        setHostData(
            MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
            HOST_DEVICE);
        assertNull(getSubpage());
        const suiteLinkWrapper =
            multidevicePage.shadowRoot!.querySelector<HTMLElement>(
                '#suiteLinkWrapper');
        assertTrue(!!suiteLinkWrapper);
        suiteLinkWrapper.click();
        assertTrue(!!getSubpage());
        assertNull(getSubpage()!.shadowRoot!.querySelector(
            'settings-multidevice-feature-item'));
      });

  test(
      'Multidevice subpage trigger should be focused after returning from ' +
          'subpage',
      async () => {
        Router.getInstance().navigateTo(routes.MULTIDEVICE);
        setHostData(MultiDeviceSettingsMode.HOST_SET_VERIFIED);

        // Sub-page trigger navigates to Multidevice Features subpage
        const triggerSelector = '#multideviceItem .subpage-arrow';
        const subpageTrigger =
            multidevicePage.shadowRoot!.querySelector<HTMLButtonElement>(
                triggerSelector);
        assertTrue(!!subpageTrigger);
        subpageTrigger.click();
        assertEquals(
            routes.MULTIDEVICE_FEATURES, Router.getInstance().currentRoute);

        // Navigate back
        const popStateEventPromise = eventToPromise('popstate', window);
        Router.getInstance().navigateToPreviousRoute();
        await popStateEventPromise;
        await waitAfterNextRender(multidevicePage);

        assertEquals(
            subpageTrigger, multidevicePage.shadowRoot!.activeElement,
            `${triggerSelector} should be focused.`);
      });

  test('policy prohibited suite shows policy indicator', () => {
    setHostData(MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS);
    assertNull(
        multidevicePage.shadowRoot!.querySelector('#suitePolicyIndicator'));
    // Prohibit suite by policy.
    setSuiteState(MultiDeviceFeatureState.PROHIBITED_BY_POLICY);
    assertTrue(
        !!multidevicePage.shadowRoot!.querySelector('#suitePolicyIndicator'));
    // Reallow suite.
    setSuiteState(MultiDeviceFeatureState.DISABLED_BY_USER);
    assertNull(
        multidevicePage.shadowRoot!.querySelector('#suitePolicyIndicator'));
  });

  test('Multidevice permissions setup dialog', () => {
    setPhoneHubNotificationsState(MultiDeviceFeatureState.DISABLED_BY_USER);
    assertNull(multidevicePage.shadowRoot!.querySelector(
        'settings-multidevice-notification-access-setup-dialog'));

    setPhoneHubNotificationAccessGranted(false);
    simulateFeatureStateChangeRequest(
        MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
        /*enabled=*/ true, /*authRequired=*/ false);

    // Close the dialog.
    multidevicePage.set('showPhonePermissionSetupDialog_', false);

    setPhoneHubNotificationAccessGranted(false);
    simulateFeatureStateChangeRequest(
        MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
        /*enabled=*/ false, /*authRequired=*/ false);

    setPhoneHubNotificationAccessGranted(true);
    simulateFeatureStateChangeRequest(
        MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
        /*enabled=*/ true, /*authRequired=*/ false);

    multidevicePage.pageContentData.notificationAccessStatus =
        PhoneHubFeatureAccessStatus.ACCESS_GRANTED;
    simulateFeatureStateChangeRequest(
        MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
        /*enabled=*/ false, /*authRequired=*/ false);
  });

  test('New multidevice permissions setup dialog', () => {
    setPhoneHubPermissionsDialogSupported(true);
    setPhoneHubNotificationsState(MultiDeviceFeatureState.DISABLED_BY_USER);
    assertNull(multidevicePage.shadowRoot!.querySelector(
        'settings-multidevice-permissions-setup-dialog'));

    setPhoneHubNotificationAccessGranted(false);
    simulateFeatureStateChangeRequest(
        MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
        /*enabled=*/ true, /*authRequired=*/ false);

    // Close the dialog.
    multidevicePage.set('showPhonePermissionSetupDialog_', false);

    setPhoneHubNotificationAccessGranted(false);
    simulateFeatureStateChangeRequest(
        MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
        /*enabled=*/ false, /*authRequired=*/ false);

    setPhoneHubNotificationAccessGranted(true);
    simulateFeatureStateChangeRequest(
        MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
        /*enabled=*/ true, /*authRequired=*/ false);

    multidevicePage.pageContentData.notificationAccessStatus =
        PhoneHubFeatureAccessStatus.ACCESS_GRANTED;
    simulateFeatureStateChangeRequest(
        MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
        /*enabled=*/ false, /*authRequired=*/ false);
  });

  test('Disabling features never requires authentication', async () => {
    const Feature = MultiDeviceFeature;

    const disableFeatureFn = (feature: MultiDeviceFeature) =>
        simulateFeatureStateChangeRequest(
            feature, false /* enabled */, false /* authRequired */);

    await disableFeatureFn(Feature.BETTER_TOGETHER_SUITE);
    await disableFeatureFn(Feature.INSTANT_TETHERING);
    await disableFeatureFn(Feature.SMART_LOCK);
  });

  test(
      'Enabling some features requires authentication; others do not',
      async () => {
        const Feature = MultiDeviceFeature;
        const FeatureState = MultiDeviceFeatureState;

        const enableFeatureWithoutAuthFn = (feature: MultiDeviceFeature) =>
            simulateFeatureStateChangeRequest(
                feature, true /* enabled */, false /* authRequired */);

        const enableFeatureWithAuthFn = (feature: MultiDeviceFeature) =>
            simulateFeatureStateChangeRequest(
                feature, true /* enabled */, true /* authRequired */);

        // Start out with SmartLock being disabled by the user. This means that
        // the first attempt to enable BETTER_TOGETHER_SUITE below will not
        // require authentication.
        setSmartLockState(FeatureState.DISABLED_BY_USER);

        // INSTANT_TETHERING requires no authentication.
        await enableFeatureWithoutAuthFn(Feature.INSTANT_TETHERING);
        // BETTER_TOGETHER_SUITE requires no authentication normally.
        await enableFeatureWithoutAuthFn(Feature.BETTER_TOGETHER_SUITE);
        // BETTER_TOGETHER_SUITE requires authentication when SmartLock's
        // state is UNAVAILABLE_SUITE_DISABLED.
        setSmartLockState(FeatureState.UNAVAILABLE_SUITE_DISABLED);
        await enableFeatureWithAuthFn(Feature.BETTER_TOGETHER_SUITE);
        // BETTER_TOGETHER_SUITE requires authentication when SmartLock's
        // state is UNAVAILABLE_INSUFFICIENT_SECURITY.
        setSmartLockState(FeatureState.UNAVAILABLE_INSUFFICIENT_SECURITY);
        await enableFeatureWithAuthFn(Feature.BETTER_TOGETHER_SUITE);
        // SMART_LOCK always requires authentication.
        await enableFeatureWithAuthFn(Feature.SMART_LOCK);
      });

  test('Nearby setup button shown before onboarding is complete', () => {
    setNearbyShareDisallowedByPolicy(false);
    assertTrue(
        isVisible(multidevicePage.shadowRoot!.querySelector('#nearbySetUp')));
    assertFalse(isVisible(multidevicePage.shadowRoot!.querySelector(
        '#nearbySharingToggleButton')));

    setNearbyShareIsOnboardingComplete(true);
    assertFalse(
        isVisible(multidevicePage.shadowRoot!.querySelector('#nearbySetUp')));

    const nearbySharingToggleButton =
        multidevicePage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#nearbySharingToggleButton');
    assertTrue(!!nearbySharingToggleButton);
    assertFalse(nearbySharingToggleButton.disabled);
  });

  test('Nearby disabled toggle shown if disallowed by policy', () => {
    setNearbyShareDisallowedByPolicy(false);
    assertTrue(
        isVisible(multidevicePage.shadowRoot!.querySelector('#nearbySetUp')));
    assertFalse(isVisible(multidevicePage.shadowRoot!.querySelector(
        '#nearbySharingToggleButton')));

    setNearbyShareDisallowedByPolicy(true);
    assertFalse(
        isVisible(multidevicePage.shadowRoot!.querySelector('#nearbySetUp')));

    const nearbySharingToggleButton =
        multidevicePage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#nearbySharingToggleButton');
    assertTrue(!!nearbySharingToggleButton);
    assertTrue(nearbySharingToggleButton.disabled);
  });

  test('Nearby description shown before onboarding is completed', () => {
    setNearbyShareDisallowedByPolicy(false);
    assertTrue(isVisible(
        multidevicePage.shadowRoot!.querySelector('#setupDescription')));

    setNearbyShareIsOnboardingComplete(true);
    assertFalse(isVisible(
        multidevicePage.shadowRoot!.querySelector('#setupDescription')));

    const disabledDescription = getNearbyShareDisabledDescription();
    const expectedText = 'Share files and more with nearby devices. Learn more';
    assertEquals(expectedText, disabledDescription);
  });

  test('Nearby description shown if disallowed by policy', () => {
    setNearbyShareDisallowedByPolicy(false);
    setNearbyShareIsOnboardingComplete(true);
    assertFalse(isVisible(
        multidevicePage.shadowRoot!.querySelector('#setupDescription')));

    const disabledDescription = getNearbyShareDisabledDescription();

    const expectedText = 'Share files and more with nearby devices. Learn more';
    assertEquals(expectedText, disabledDescription);

    setNearbyShareDisallowedByPolicy(true);
    assertTrue(isVisible(
        multidevicePage.shadowRoot!.querySelector('#setupDescription')));
  });

  test('Nearby policy indicator shown when disallowed by policy', () => {
    setNearbyShareDisallowedByPolicy(false);
    assertFalse(isVisible(
        multidevicePage.shadowRoot!.querySelector('#nearbyPolicyIndicator')));

    setNearbyShareDisallowedByPolicy(true);
    assertTrue(isVisible(
        multidevicePage.shadowRoot!.querySelector('#nearbyPolicyIndicator')));

    setNearbyShareDisallowedByPolicy(false);
    assertFalse(isVisible(
        multidevicePage.shadowRoot!.querySelector('#nearbyPolicyIndicator')));
  });

  test('Nearby subpage not available when disallowed by policy', () => {
    setNearbyShareDisallowedByPolicy(true);
    let nearbyLinkWrapper =
        multidevicePage.shadowRoot!.querySelector('#nearbyLinkWrapper');
    assertTrue(!!nearbyLinkWrapper);
    assertFalse(nearbyLinkWrapper.hasAttribute('actionable'));

    setNearbyShareDisallowedByPolicy(false);
    nearbyLinkWrapper =
        multidevicePage.shadowRoot!.querySelector('#nearbyLinkWrapper');
    assertTrue(!!nearbyLinkWrapper);
    assertTrue(nearbyLinkWrapper.hasAttribute('actionable'));
  });

  test('Better Together Suite icon visible when there is no host set', () => {
    setHostData(MultiDeviceSettingsMode.NO_HOST_SET);
    assertTrue(isVisible(
        multidevicePage.shadowRoot!.querySelector('#betterTogetherSuiteIcon')));
  });

  test('Better Together Suite icon visible when there is a host set', () => {
    setHostData(MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    assertTrue(isVisible(
        multidevicePage.shadowRoot!.querySelector('#betterTogetherSuiteIcon')));
  });

  test('Better Together Suite icon remains visible when host added', () => {
    setHostData(MultiDeviceSettingsMode.NO_HOST_SET);
    assertTrue(isVisible(
        multidevicePage.shadowRoot!.querySelector('#betterTogetherSuiteIcon')));

    setHostData(MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    assertTrue(isVisible(
        multidevicePage.shadowRoot!.querySelector('#betterTogetherSuiteIcon')));
  });

  test('Better Together Suite icon remains visible when host removed', () => {
    setHostData(MultiDeviceSettingsMode.HOST_SET_VERIFIED);
    assertTrue(isVisible(
        multidevicePage.shadowRoot!.querySelector('#betterTogetherSuiteIcon')));

    setHostData(MultiDeviceSettingsMode.NO_HOST_SET);
    assertTrue(isVisible(
        multidevicePage.shadowRoot!.querySelector('#betterTogetherSuiteIcon')));
  });

  test('Nearby share sub page arrow is not visible before onboarding', () => {
    setNearbyShareDisallowedByPolicy(false);
    assertTrue(
        isVisible(multidevicePage.shadowRoot!.querySelector('#nearbySetUp')));
    assertTrue(isVisible(
        multidevicePage.shadowRoot!.querySelector('#nearbyShareSubpageArrow')));

    setNearbyShareIsOnboardingComplete(true);
    setNearbyShareEnabled(true);
    flush();
    assertTrue(isVisible(
        multidevicePage.shadowRoot!.querySelector('#nearbyShareSubpageArrow')));
  });

  test(
      'Clicking nearby subpage before onboarding initiates onboarding',
      async () => {
        setNearbyShareDisallowedByPolicy(false);
        assertTrue(isVisible(
            multidevicePage.shadowRoot!.querySelector('#nearbySetUp')));
        assertTrue(isVisible(multidevicePage.shadowRoot!.querySelector(
            '#nearbyShareSubpageArrow')));

        const router = Router.getInstance();
        const nearbyLinkWrapper =
            multidevicePage.shadowRoot!.querySelector<HTMLElement>(
                '#nearbyLinkWrapper');
        assertTrue(!!nearbyLinkWrapper);
        nearbyLinkWrapper.click();
        await flushTasks();
        assertEquals(routes.NEARBY_SHARE, router.currentRoute);
        assertFalse(router.getQueryParameters().has('onboarding'));
      });

  test('Clicking nearby subpage after onboarding enters subpage', async () => {
    setNearbyShareDisallowedByPolicy(false);
    assertTrue(
        isVisible(multidevicePage.shadowRoot!.querySelector('#nearbySetUp')));
    assertTrue(isVisible(
        multidevicePage.shadowRoot!.querySelector('#nearbyShareSubpageArrow')));

    setNearbyShareIsOnboardingComplete(true);
    setNearbyShareEnabled(true);
    flush();

    assertTrue(isVisible(
        multidevicePage.shadowRoot!.querySelector('#nearbyShareSubpageArrow')));
    const router = Router.getInstance();
    const nearbyLinkWrapper =
        multidevicePage.shadowRoot!.querySelector<HTMLElement>(
            '#nearbyLinkWrapper');
    assertTrue(!!nearbyLinkWrapper);
    nearbyLinkWrapper.click();
    await flushTasks();
    assertEquals(routes.NEARBY_SHARE, router.currentRoute);
    assertFalse(router.getQueryParameters().has('onboarding'));
  });

  test('Settings mojo changes propagate to settings property', async () => {
    // Allow initial settings to be loaded.
    await flushTasks();

    const newName = 'NEW NAME';
    assertNotEquals(newName, multidevicePage.get('settings.deviceName'));

    await fakeSettings.setDeviceName(newName);
    await flushTasks();
    assertEquals(newName, multidevicePage.get('settings.deviceName'));

    const newEnabledState = !multidevicePage.get('settings.enabled');
    assertNotEquals(newEnabledState, multidevicePage.get('settings.enabled'));

    await fakeSettings.setEnabled(newEnabledState);
    await flushTasks();
    assertEquals(newEnabledState, multidevicePage.get('settings.enabled'));
  });

  test('Screen lock changes propagate to settings property', () => {
    setScreenLockStatus(/* chromeStatus= */ true, /* phoneStatus= */ true);

    assertTrue(multidevicePage.get('isChromeosScreenLockEnabled_'));
    assertTrue(multidevicePage.get('isPhoneScreenLockEnabled_'));
  });

  test('Nearby share sub page arrow is visible before onboarding', async () => {
    // Arrow only visible if background scanning feature flag is enabled
    // and hardware offloading is supported.
    await flushTasks();
    setNearbyShareDisallowedByPolicy(false);
    multidevicePage.set('settings.isFastInitiationHardwareSupported', true);

    setNearbyShareDisallowedByPolicy(false);
    assertTrue(
        isVisible(multidevicePage.shadowRoot!.querySelector('#nearbySetUp')));
    assertTrue(isVisible(
        multidevicePage.shadowRoot!.querySelector('#nearbyShareSubpageArrow')));

    setNearbyShareIsOnboardingComplete(true);
    setNearbyShareEnabled(true);
    await flushTasks();
    assertTrue(isVisible(
        multidevicePage.shadowRoot!.querySelector('#nearbyShareSubpageArrow')));
  });

  test('No Background Scanning hardware support', async () => {
    // Ensure initial nearby settings values are set before overriding.
    await flushTasks();
    setNearbyShareDisallowedByPolicy(false);
    multidevicePage.set('settings.isFastInitiationHardwareSupported', false);
    await flushTasks();

    assertTrue(
        isVisible(multidevicePage.shadowRoot!.querySelector('#nearbySetUp')));
    assertFalse(isVisible(
        multidevicePage.shadowRoot!.querySelector('#nearbyShareSubpageArrow')));

    // Clicking on Nearby Subpage row should initiate onboarding.
    const router = Router.getInstance();
    const nearbyLinkWrapper =
        multidevicePage.shadowRoot!.querySelector<HTMLElement>(
            '#nearbyLinkWrapper');
    assertTrue(!!nearbyLinkWrapper);
    nearbyLinkWrapper.click();
    await flushTasks();
    assertEquals(routes.NEARBY_SHARE, router.currentRoute);
    assertTrue(router.getQueryParameters().has('onboarding'));
  });

  test('Clicking nearby subpage before onboarding enters subpage', async () => {
    setNearbyShareDisallowedByPolicy(false);
    await flushTasks();

    const router = Router.getInstance();
    const nearbyLinkWrapper =
        multidevicePage.shadowRoot!.querySelector<HTMLElement>(
            '#nearbyLinkWrapper');
    assertTrue(!!nearbyLinkWrapper);
    nearbyLinkWrapper.click();

    await flushTasks();
    assertEquals(routes.NEARBY_SHARE, router.currentRoute);
    assertFalse(router.getQueryParameters().has('onboarding'));
  });

  test(
      'Subpage shows no Quick Share on/off toggle on QuickShareV2 enabled',
      async () => {
        setupQuickShareV2();
        const quickShareToggle = multidevicePage.shadowRoot!.querySelector(
            '#nearbySharingToggleButton');
        assertFalse(!!quickShareToggle);
      });
});
