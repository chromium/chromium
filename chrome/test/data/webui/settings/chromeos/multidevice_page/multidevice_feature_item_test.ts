// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsMultideviceFeatureItemElement, SettingsMultideviceFeatureToggleElement} from 'chrome://os-settings/lazy_load.js';
import {CrToggleElement, LocalizedLinkElement, MultiDeviceFeature, MultiDeviceFeatureState, MultiDeviceSettingsMode, PhoneHubFeatureAccessProhibitedReason, PhoneHubFeatureAccessStatus, Route, Router, routes} from 'chrome://os-settings/os_settings.js';
import {IronIconElement} from 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('<settings-multidevice-feature-item>', () => {
  let featureItem: SettingsMultideviceFeatureItemElement;
  let featureToggle: SettingsMultideviceFeatureToggleElement;
  let crToggle: CrToggleElement;
  let featureState: MultiDeviceFeatureState;
  let initialRoute: Route;

  // Fake MultiDeviceFeature enum value
  const FAKE_SUMMARY_HTML = 'Gives you candy <a href="#">Learn more.</a>';

  /** Resets both the suite and the (fake) feature to on state. */
  function resetFeatureData() {
    featureState = MultiDeviceFeatureState.ENABLED_BY_USER;
    featureItem.pageContentData = {
      betterTogetherState: MultiDeviceFeatureState.ENABLED_BY_USER,
      mode: MultiDeviceSettingsMode.NO_HOST_SET,
      hostDeviceName: undefined,
      instantTetheringState: MultiDeviceFeatureState.DISABLED_BY_USER,
      smartLockState: MultiDeviceFeatureState.ENABLED_BY_USER,
      phoneHubState: MultiDeviceFeatureState.DISABLED_BY_USER,
      phoneHubCameraRollState: MultiDeviceFeatureState.DISABLED_BY_USER,
      phoneHubNotificationsState: MultiDeviceFeatureState.DISABLED_BY_USER,
      phoneHubTaskContinuationState: MultiDeviceFeatureState.DISABLED_BY_USER,
      phoneHubAppsState: MultiDeviceFeatureState.DISABLED_BY_USER,
      wifiSyncState: MultiDeviceFeatureState.DISABLED_BY_USER,
      cameraRollAccessStatus:
          PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
      notificationAccessStatus: PhoneHubFeatureAccessStatus.ACCESS_GRANTED,
      appsAccessStatus: PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
      notificationAccessProhibitedReason:
          PhoneHubFeatureAccessProhibitedReason.UNKNOWN,
      isNearbyShareDisallowedByPolicy: false,
      isPhoneHubPermissionsDialogSupported: false,
      isCameraRollFilePermissionGranted: false,
      isPhoneHubFeatureCombinedSetupSupported: false,
      isChromeOSSyncedSessionSharingEnabled: false,
      isLacrosTabSyncEnabled: false,
    };
    flush();
    assertFalse(crToggle.disabled);
    assertTrue(crToggle.checked);
  }

  /**
   * Clicks an element, asserts whether the click navigated the page away to a
   * new route, then navigates back to initialRoute.
   * @param element. Target of click.
   * @param shouldRouteAway. Whether the page is expected to navigate
   * to a new route.
   */
  function checkWhetherClickRoutesAway(
      element: HTMLElement, shouldRouteAway: boolean) {
    element.click();
    flush();
    assertEquals(
        shouldRouteAway, initialRoute !== Router.getInstance().currentRoute);
    Router.getInstance().navigateTo(initialRoute);
    assertEquals(initialRoute, Router.getInstance().currentRoute);
  }

  setup(() => {
    featureItem = document.createElement('settings-multidevice-feature-item');
    featureItem.getFeatureSummaryHtml = () => FAKE_SUMMARY_HTML;
    featureItem.feature = MultiDeviceFeature.BETTER_TOGETHER_SUITE;
    document.body.appendChild(featureItem);
    flush();

    const toggle = featureItem.shadowRoot!.querySelector(
        'settings-multidevice-feature-toggle');
    assertTrue(!!toggle);
    featureToggle = toggle;
    featureToggle.getFeatureState = () => featureState;

    crToggle = featureToggle.$.toggle;

    initialRoute = routes.MULTIDEVICE_FEATURES;
    const dummyRoute = new Route('/freeCandy');
    featureItem.subpageRoute = dummyRoute;

    resetFeatureData();
    Router.getInstance().navigateTo(initialRoute);
    flush();
  });

  teardown(() => {
    featureItem.remove();
  });

  test('generic click navigates to subpage', () => {
    const itemTextContainer =
        featureItem.shadowRoot!.querySelector<HTMLElement>(
            '#item-text-container');
    assertTrue(!!itemTextContainer);
    checkWhetherClickRoutesAway(itemTextContainer, true);
    const ironIcon =
        featureItem.shadowRoot!.querySelector<IronIconElement>('iron-icon');
    assertTrue(!!ironIcon);
    checkWhetherClickRoutesAway(ironIcon, true);
    const featureSecondary =
        featureItem.shadowRoot!.querySelector<LocalizedLinkElement>(
            '#featureSecondary');
    assertTrue(!!featureSecondary);
    checkWhetherClickRoutesAway(featureSecondary, true);
  });

  test('link click does not navigate to subpage', () => {
    const featureSecondary =
        featureItem.shadowRoot!.querySelector<LocalizedLinkElement>(
            '#featureSecondary');
    assertTrue(!!featureSecondary);
    const link = featureSecondary.$.container.querySelector('a');
    assertTrue(!!link);
    checkWhetherClickRoutesAway(link, false);
  });

  test('row is clickable', async () => {
    featureItem.feature = MultiDeviceFeature.BETTER_TOGETHER_SUITE;
    featureState = MultiDeviceFeatureState.ENABLED_BY_USER;
    featureItem.subpageRoute = undefined;
    flush();

    const expectedEvent =
        eventToPromise('feature-toggle-clicked', featureToggle);
    const linkWrapper =
        featureItem.shadowRoot!.querySelector<HTMLAnchorElement>(
            '#linkWrapper');
    assertTrue(!!linkWrapper);
    linkWrapper.click();
    await expectedEvent;
  });

  test('toggle click does not navigate to subpage in any state', () => {
    checkWhetherClickRoutesAway(featureToggle, false);

    crToggle.checked = true;
    assertFalse(crToggle.disabled);
    checkWhetherClickRoutesAway(crToggle, false);

    crToggle.checked = false;
    assertFalse(crToggle.disabled);
    checkWhetherClickRoutesAway(crToggle, false);
  });
});
