// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {MultiDeviceFeature, MultiDeviceFeatureState, PhoneHubNotificationAccessStatus} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

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
  /** @type {!settings.MultiDeviceFeature} */
  let featureToTest;
  /** @type {string} */
  let pageContentDataKey;

  /**
   * Sets the state of the feature shown in the toggle. Note that in order to
   * trigger featureToggle's bindings to update, we set its pageContentData to a
   * new object as the actual UI does.
   * @param {?settings.MultiDeviceFeatureState} newFeatureState
   */
  function setFeatureState(newFeatureState) {
    featureToggle.pageContentData = Object.assign(
        {}, featureToggle.pageContentData,
        {[pageContentDataKey]: newFeatureState});
    Polymer.dom.flush();
  }

  /**
   * @param {!settings.PhoneHubNotificationAccessStatus} accessStatus
   */
  function setNotificationAccessStatus(accessStatus) {
    featureToggle.pageContentData = Object.assign(
        {}, featureToggle.pageContentData,
        {notificationAccessStatus: accessStatus});
    Polymer.dom.flush();
  }

  /**
   * Sets the state of the feature suite. Note that in order to trigger
   * featureToggle's bindings to update, we set its pageContentData to a new
   * object as the actual UI does.
   * @param {?settings.MultiDeviceFeatureState} newSuiteState. New value for
   * featureToggle.pageContentData.betterTogetherState.
   */
  function setSuiteState(newSuiteState) {
    featureToggle.pageContentData = Object.assign(
        {}, featureToggle.pageContentData,
        {betterTogetherState: newSuiteState});
    Polymer.dom.flush();
  }

  /**
   * @param {!settings.MultiDeviceFeature} feature
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
      betterTogetherState: settings.MultiDeviceFeatureState.ENABLED_BY_USER,
      [pageContentDataKey]: settings.MultiDeviceFeatureState.DISABLED_BY_USER,
    };
    document.body.appendChild(featureToggle);
    Polymer.dom.flush();

    crToggle = featureToggle.$.toggle;

    assertFalse(featureToggle.checked_);
    assertFalse(crToggle.checked);
    assertFalse(crToggle.disabled);
  }

  setup(() => {
    PolymerTest.clearBody();
  });

  test('checked property can be set by feature state', () => {
    init(settings.MultiDeviceFeature.MESSAGES, 'messagesState');

    setFeatureState(settings.MultiDeviceFeatureState.ENABLED_BY_USER);
    assertTrue(featureToggle.checked_);
    assertTrue(crToggle.checked);

    setFeatureState(settings.MultiDeviceFeatureState.DISABLED_BY_USER);
    assertFalse(featureToggle.checked_);
    assertFalse(crToggle.checked);
  });

  test('disabled property can be set by feature state', () => {
    init(settings.MultiDeviceFeature.MESSAGES, 'messagesState');

    setFeatureState(settings.MultiDeviceFeatureState.PROHIBITED_BY_POLICY);
    assertTrue(crToggle.disabled);

    setFeatureState(settings.MultiDeviceFeatureState.DISABLED_BY_USER);
    assertFalse(crToggle.disabled);
  });

  test('disabled and checked properties update simultaneously', () => {
    init(settings.MultiDeviceFeature.MESSAGES, 'messagesState');

    setFeatureState(settings.MultiDeviceFeatureState.ENABLED_BY_USER);
    assertTrue(featureToggle.checked_);
    assertTrue(crToggle.checked);
    assertFalse(crToggle.disabled);

    setFeatureState(settings.MultiDeviceFeatureState.PROHIBITED_BY_POLICY);
    assertFalse(featureToggle.checked_);
    assertFalse(crToggle.checked);
    assertTrue(crToggle.disabled);

    setFeatureState(settings.MultiDeviceFeatureState.DISABLED_BY_USER);
    assertFalse(featureToggle.checked_);
    assertFalse(crToggle.checked);
    assertFalse(crToggle.disabled);
  });

  test('disabled property can be set by suite pref', () => {
    init(settings.MultiDeviceFeature.MESSAGES, 'messagesState');

    setSuiteState(settings.MultiDeviceFeatureState.DISABLED_BY_USER);
    Polymer.dom.flush();
    assertTrue(crToggle.disabled);

    setSuiteState(settings.MultiDeviceFeatureState.ENABLED_BY_USER);
    Polymer.dom.flush();
    assertFalse(crToggle.disabled);
  });

  test('checked property is unaffected by suite pref', () => {
    init(settings.MultiDeviceFeature.MESSAGES, 'messagesState');

    setFeatureState(settings.MultiDeviceFeatureState.ENABLED_BY_USER);
    assertTrue(featureToggle.checked_);
    assertTrue(crToggle.checked);
    assertFalse(crToggle.disabled);

    setSuiteState(settings.MultiDeviceFeatureState.DISABLED_BY_USER);
    Polymer.dom.flush();
    assertTrue(featureToggle.checked_);
    assertTrue(crToggle.checked);
    assertTrue(crToggle.disabled);
  });

  test('clicking toggle does not change checked property', () => {
    init(settings.MultiDeviceFeature.MESSAGES, 'messagesState');

    const preClickCrToggleChecked = crToggle.checked;
    crToggle.click();
    Polymer.dom.flush();
    assertEquals(crToggle.checked, preClickCrToggleChecked);
  });

  test('notification access is prohibited', () => {
    init(
        settings.MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS,
        'phoneHubNotificationsState');

    setFeatureState(settings.MultiDeviceFeatureState.ENABLED_BY_USER);
    assertTrue(featureToggle.checked_);
    assertTrue(crToggle.checked);
    assertFalse(crToggle.disabled);

    setNotificationAccessStatus(
        settings.PhoneHubNotificationAccessStatus.PROHIBITED);
    assertFalse(featureToggle.checked_);
    assertFalse(crToggle.checked);
    assertTrue(crToggle.disabled);
  });
});
