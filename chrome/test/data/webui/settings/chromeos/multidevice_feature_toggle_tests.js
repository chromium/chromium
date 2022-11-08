// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MultiDeviceFeature, MultiDeviceFeatureState, PhoneHubFeatureAccessStatus} from 'chrome://os-settings/chromeos/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

/**
 * @fileoverview
 * Suite of tests for settings-multidevice-feature-toggle element. For
 * simplicity, we provide the toggle with a real feature (i.e. messages) as a
 * stand-in for a generic feature.
 */
suite('Multidevice', () => {
  /** @type {?SettingsMultideviceFeatureToggleElement} */
  let featureToggle = null;
  /** @type {?CrToggleElement} */
  let crToggle = null;
  /** @type {!MultiDeviceFeature} */
  let featureToTest;
  /** @type {string} */
  let pageContentDataKey;

  /**
   * Sets the state of the feature shown in the toggle. Note that in order to
   * trigger featureToggle's bindings to update, we set its pageContentData to a
   * new object as the actual UI does.
   * @param {?MultiDeviceFeatureState} newFeatureState
   */
  function setFeatureState(newFeatureState) {
    featureToggle.pageContentData = Object.assign(
        {}, featureToggle.pageContentData,
        {[pageContentDataKey]: newFeatureState});
    flush();
  }

  /**
   * @param {!PhoneHubFeatureAccessStatus} accessStatus
   */
  function setNotificationAccessStatus(accessStatus) {
    featureToggle.pageContentData = Object.assign(
        {}, featureToggle.pageContentData,
        {notificationAccessStatus: accessStatus});
    flush();
  }

  /**
   * Sets the state of the feature suite. Note that in order to trigger
   * featureToggle's bindings to update, we set its pageContentData to a new
   * object as the actual UI does.
   * @param {?MultiDeviceFeatureState} newSuiteState. New value for
   * featureToggle.pageContentData.betterTogetherState.
   */
  function setSuiteState(newSuiteState) {
    featureToggle.pageContentData = Object.assign(
        {}, featureToggle.pageContentData,
        {betterTogetherState: newSuiteState});
    flush();
  }

  /**
   * Sets the state of the top-level Phone Hub feature. Note that in order to
   * trigger featureToggle's bindings to update, we set its pageContentData to a
   * new object as the actual UI does.
   * @param {?MultiDeviceFeatureState} newPhoneHubState. New value for
   * featureToggle.pageContentData.phoneHubState.
   */
  function setPhoneHubState(newPhoneHubState) {
    featureToggle.pageContentData = Object.assign(
        {}, featureToggle.pageContentData, {phoneHubState: newPhoneHubState});
    flush();
  }

  /**
   * @param {!MultiDeviceFeature} feature
   * @param {string} key
   */
  function init(feature, key) {
    featureToTest = feature;
    pageContentDataKey = key;

    featureToggle =
        document.createElement('settings-multidevice-feature-toggle');
    featureToggle.feature = feature;

    // Initially toggle will be unchecked but not disabled. Note that the word
    // "disabled" is ambiguous for feature toggles because it can refer to the
    // feature or the cr-toggle property/attribute. DISABLED_BY_USER refers to
    // the former so it unchecks but does not functionally disable the toggle.
    featureToggle.pageContentData = {
      betterTogetherState: MultiDeviceFeatureState.ENABLED_BY_USER,
      [pageContentDataKey]: MultiDeviceFeatureState.DISABLED_BY_USER,
    };
    document.body.appendChild(featureToggle);
    flush();

    crToggle = featureToggle.$.toggle;

    assertFalse(featureToggle.checked_);
    assertFalse(crToggle.checked);
  }

  setup(() => {
    PolymerTest.clearBody();
  });

  test('checked property can be set by feature state', () => {
    init(MultiDeviceFeature.MESSAGES, 'messagesState');

    setFeatureState(MultiDeviceFeatureState.ENABLED_BY_USER);
    assertTrue(featureToggle.checked_);
    assertTrue(crToggle.checked);

    setFeatureState(MultiDeviceFeatureState.DISABLED_BY_USER);
    assertFalse(featureToggle.checked_);
    assertFalse(crToggle.checked);
  });

  test('disabled property can be set by feature state', () => {
    init(MultiDeviceFeature.MESSAGES, 'messagesState');

    setFeatureState(MultiDeviceFeatureState.PROHIBITED_BY_POLICY);
    assertTrue(crToggle.disabled);

    setFeatureState(MultiDeviceFeatureState.DISABLED_BY_USER);
    assertFalse(crToggle.disabled);
  });

  test('disabled and checked properties update simultaneously', () => {
    init(MultiDeviceFeature.MESSAGES, 'messagesState');

    setFeatureState(MultiDeviceFeatureState.ENABLED_BY_USER);
    assertTrue(featureToggle.checked_);
    assertTrue(crToggle.checked);
    assertFalse(crToggle.disabled);

    setFeatureState(MultiDeviceFeatureState.PROHIBITED_BY_POLICY);
    assertFalse(featureToggle.checked_);
    assertFalse(crToggle.checked);
    assertTrue(crToggle.disabled);

    setFeatureState(MultiDeviceFeatureState.DISABLED_BY_USER);
    assertFalse(featureToggle.checked_);
    assertFalse(crToggle.checked);
    assertFalse(crToggle.disabled);
  });

  test('disabled property can be set by suite pref', () => {
    init(MultiDeviceFeature.MESSAGES, 'messagesState');

    setSuiteState(MultiDeviceFeatureState.DISABLED_BY_USER);
    flush();
    assertTrue(crToggle.disabled);

    setSuiteState(MultiDeviceFeatureState.ENABLED_BY_USER);
    flush();
    assertFalse(crToggle.disabled);
  });

  test('checked property is unaffected by suite pref', () => {
    init(MultiDeviceFeature.MESSAGES, 'messagesState');

    setFeatureState(MultiDeviceFeatureState.ENABLED_BY_USER);
    assertTrue(featureToggle.checked_);
    assertTrue(crToggle.checked);
    assertFalse(crToggle.disabled);

    setSuiteState(MultiDeviceFeatureState.DISABLED_BY_USER);
    flush();
    assertTrue(featureToggle.checked_);
    assertTrue(crToggle.checked);
    assertTrue(crToggle.disabled);
  });

  test('clicking toggle does not change checked property', () => {
    init(MultiDeviceFeature.MESSAGES, 'messagesState');

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
    assertTrue(featureToggle.checked_);
    assertTrue(crToggle.checked);
    assertFalse(crToggle.disabled);

    setNotificationAccessStatus(PhoneHubFeatureAccessStatus.PROHIBITED);
    assertFalse(featureToggle.checked_);
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
