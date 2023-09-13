// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsMultideviceFeatureToggleElement} from 'chrome://os-settings/lazy_load.js';
import {CrToggleElement, MultiDeviceFeature, MultiDeviceFeatureState, MultiDeviceSettingsMode, PhoneHubFeatureAccessProhibitedReason, PhoneHubFeatureAccessStatus} from 'chrome://os-settings/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

/**
 * @fileoverview
 * Suite of tests for settings-multidevice-feature-toggle element. For
 * simplicity, we provide the toggle with a real feature (i.e. Phone Hub) as a
 * stand-in for a generic feature.
 */
suite('<settings-multidevice-feature-toggle>', () => {
  let featureToggle: SettingsMultideviceFeatureToggleElement;
  let crToggle: CrToggleElement;
  let pageContentDataKey: string;

  /**
   * Sets the state of the feature shown in the toggle. Note that in order to
   * trigger featureToggle's bindings to update, we set its pageContentData to a
   * new object as the actual UI does.
   */
  function setFeatureState(newFeatureState: MultiDeviceFeatureState) {
    featureToggle.pageContentData = Object.assign(
        {}, featureToggle.pageContentData,
        {[pageContentDataKey]: newFeatureState});
    flush();
  }

  function setNotificationAccessStatus(
      accessStatus: PhoneHubFeatureAccessStatus) {
    featureToggle.pageContentData = Object.assign(
        {}, featureToggle.pageContentData,
        {notificationAccessStatus: accessStatus});
    flush();
  }

  /**
   * Sets the state of the feature suite. Note that in order to trigger
   * featureToggle's bindings to update, we set its pageContentData to a new
   * object as the actual UI does.
   * @param newSuiteState. New value for
   * featureToggle.pageContentData.betterTogetherState.
   */
  function setSuiteState(newSuiteState: MultiDeviceFeatureState) {
    featureToggle.pageContentData = Object.assign(
        {}, featureToggle.pageContentData,
        {betterTogetherState: newSuiteState});
    flush();
  }

  /**
   * Sets the state of the top-level Phone Hub feature. Note that in order to
   * trigger featureToggle's bindings to update, we set its pageContentData to a
   * new object as the actual UI does.
   * @param newPhoneHubState. New value for
   * featureToggle.pageContentData.phoneHubState.
   */
  function setPhoneHubState(newPhoneHubState: MultiDeviceFeatureState) {
    featureToggle.pageContentData = Object.assign(
        {}, featureToggle.pageContentData, {phoneHubState: newPhoneHubState});
    flush();
  }

  function init(feature: MultiDeviceFeature, key: string) {
    pageContentDataKey = key;
    featureToggle =
        document.createElement('settings-multidevice-feature-toggle');
    featureToggle.feature = feature;

    // Initially toggle will be unchecked but not disabled. Note that the word
    // "disabled" is ambiguous for feature toggles because it can refer to the
    // feature or the cr-toggle property/attribute. DISABLED_BY_USER refers to
    // the former so it unchecks but does not functionally disable the toggle.
    featureToggle.pageContentData = {
      mode: MultiDeviceSettingsMode.NO_ELIGIBLE_HOSTS,
      hostDeviceName: undefined,
      instantTetheringState: MultiDeviceFeatureState.DISABLED_BY_USER,
      smartLockState: MultiDeviceFeatureState.DISABLED_BY_USER,
      phoneHubState: MultiDeviceFeatureState.DISABLED_BY_USER,
      phoneHubCameraRollState: MultiDeviceFeatureState.DISABLED_BY_USER,
      phoneHubNotificationsState: MultiDeviceFeatureState.DISABLED_BY_USER,
      phoneHubTaskContinuationState: MultiDeviceFeatureState.DISABLED_BY_USER,
      phoneHubAppsState: MultiDeviceFeatureState.DISABLED_BY_USER,
      wifiSyncState: MultiDeviceFeatureState.DISABLED_BY_USER,
      cameraRollAccessStatus:
          PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
      notificationAccessStatus:
          PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
      appsAccessStatus: PhoneHubFeatureAccessStatus.AVAILABLE_BUT_NOT_GRANTED,
      notificationAccessProhibitedReason:
          PhoneHubFeatureAccessProhibitedReason.UNKNOWN,
      isNearbyShareDisallowedByPolicy: false,
      isPhoneHubPermissionsDialogSupported: false,
      isCameraRollFilePermissionGranted: false,
      isPhoneHubFeatureCombinedSetupSupported: false,
      isChromeOSSyncedSessionSharingEnabled: false,
      isLacrosTabSyncEnabled: false,
      betterTogetherState: MultiDeviceFeatureState.ENABLED_BY_USER,
      [pageContentDataKey]: MultiDeviceFeatureState.DISABLED_BY_USER,
    };
    document.body.appendChild(featureToggle);
    flush();

    crToggle = featureToggle.$.toggle;
    assertFalse(featureToggle.get('checked_'));
    assertFalse(crToggle.checked);
  }

  test('checked property can be set by feature state', () => {
    init(MultiDeviceFeature.PHONE_HUB, 'phoneHubState');

    setFeatureState(MultiDeviceFeatureState.ENABLED_BY_USER);
    assertTrue(featureToggle.get('checked_'));
    assertTrue(crToggle.checked);

    setFeatureState(MultiDeviceFeatureState.DISABLED_BY_USER);
    assertFalse(featureToggle.get('checked_'));
    assertFalse(crToggle.checked);
  });

  test('disabled property can be set by feature state', () => {
    init(MultiDeviceFeature.PHONE_HUB, 'phoneHubState');

    setFeatureState(MultiDeviceFeatureState.PROHIBITED_BY_POLICY);
    assertTrue(crToggle.disabled);

    setFeatureState(MultiDeviceFeatureState.DISABLED_BY_USER);
    assertFalse(crToggle.disabled);
  });

  test('disabled and checked properties update simultaneously', () => {
    init(MultiDeviceFeature.PHONE_HUB, 'phoneHubState');

    setFeatureState(MultiDeviceFeatureState.ENABLED_BY_USER);
    assertTrue(featureToggle.get('checked_'));
    assertTrue(crToggle.checked);
    assertFalse(crToggle.disabled);

    setFeatureState(MultiDeviceFeatureState.PROHIBITED_BY_POLICY);
    assertFalse(featureToggle.get('checked_'));
    assertFalse(crToggle.checked);
    assertTrue(crToggle.disabled);

    setFeatureState(MultiDeviceFeatureState.DISABLED_BY_USER);
    assertFalse(featureToggle.get('checked_'));
    assertFalse(crToggle.checked);
    assertFalse(crToggle.disabled);
  });

  test('disabled property can be set by suite pref', () => {
    init(MultiDeviceFeature.PHONE_HUB, 'phoneHubState');

    setSuiteState(MultiDeviceFeatureState.DISABLED_BY_USER);
    flush();
    assertTrue(crToggle.disabled);

    setSuiteState(MultiDeviceFeatureState.ENABLED_BY_USER);
    flush();
    assertFalse(crToggle.disabled);
  });

  test('checked property is unaffected by suite pref', () => {
    init(MultiDeviceFeature.PHONE_HUB, 'phoneHubState');

    setFeatureState(MultiDeviceFeatureState.ENABLED_BY_USER);
    assertTrue(featureToggle.get('checked_'));
    assertTrue(crToggle.checked);
    assertFalse(crToggle.disabled);

    setSuiteState(MultiDeviceFeatureState.DISABLED_BY_USER);
    flush();
    assertTrue(featureToggle.get('checked_'));
    assertTrue(crToggle.checked);
    assertTrue(crToggle.disabled);
  });

  test('clicking toggle does not change checked property', () => {
    init(MultiDeviceFeature.PHONE_HUB, 'phoneHubState');

    const preClickCrToggleChecked = crToggle.checked;
    crToggle.click();
    flush();
    assertEquals(crToggle.checked, preClickCrToggleChecked);
  });

  test('notification access is prohibited', () => {
    init(
        MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
        'phoneHubNotificationsState');

    setPhoneHubState(MultiDeviceFeatureState.ENABLED_BY_USER);

    setFeatureState(MultiDeviceFeatureState.ENABLED_BY_USER);
    assertTrue(featureToggle.get('checked_'));
    assertTrue(crToggle.checked);
    assertFalse(crToggle.disabled);

    setNotificationAccessStatus(PhoneHubFeatureAccessStatus.PROHIBITED);
    assertFalse(featureToggle.get('checked_'));
    assertFalse(crToggle.checked);
    assertTrue(crToggle.disabled);
  });

  test(
      'Phone Hub notifications toggle unclickable if Phone Hub disabled',
      () => {
        init(
            MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
            'phoneHubNotificationsState');

        setPhoneHubState(MultiDeviceFeatureState.ENABLED_BY_USER);
        setNotificationAccessStatus(PhoneHubFeatureAccessStatus.ACCESS_GRANTED);
        assertFalse(crToggle.disabled);

        setPhoneHubState(MultiDeviceFeatureState.DISABLED_BY_USER);
        assertTrue(crToggle.disabled);
      });

  test(
      'Phone Hub task-continuation toggle unclickable if Phone Hub disabled',
      () => {
        init(
            MultiDeviceFeature.PHONE_HUB_TASK_CONTINUATION,
            'phoneHubTaskContinuationState');

        setPhoneHubState(MultiDeviceFeatureState.ENABLED_BY_USER);
        assertFalse(crToggle.disabled);

        setPhoneHubState(MultiDeviceFeatureState.DISABLED_BY_USER);
        assertTrue(crToggle.disabled);
      });
});
