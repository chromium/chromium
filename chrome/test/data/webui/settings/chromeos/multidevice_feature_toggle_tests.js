// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  /**
   * Sets the state of the feature shown in the toggle (i.e. Messages). Note
   * that in order to trigger featureToggle's bindings to update, we set its
   * pageContentData to a new object as the actual UI does.
   * @param {?settings.MultiDeviceFeatureState} newMessagesState. New value for
   * featureToggle.pageContentData.messagesState.
   */
  function setMessagesState(newMessagesState) {
    featureToggle.pageContentData = Object.assign(
        {}, featureToggle.pageContentData, {messagesState: newMessagesState});
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

  setup(() => {
    PolymerTest.clearBody();

    featureToggle =
        document.createElement('settings-multidevice-feature-toggle');
    featureToggle.feature = settings.MultiDeviceFeature.MESSAGES;
    // Initially toggle will be unchecked but not disabled. Note that the word
    // "disabled" is ambiguous for feature toggles because it can refer to the
    // feature or the cr-toggle property/attribute. DISABLED_BY_USER refers to
    // the former so it unchecks but does not functionally disable the toggle.
    featureToggle.pageContentData = {
      betterTogetherState: settings.MultiDeviceFeatureState.ENABLED_BY_USER,
      messagesState: settings.MultiDeviceFeatureState.DISABLED_BY_USER,
    };
    document.body.appendChild(featureToggle);
    Polymer.dom.flush();

    crToggle = featureToggle.$.toggle;

    assertFalse(featureToggle.checked_);
    assertFalse(crToggle.checked);
    assertFalse(crToggle.disabled);
  });

  test('checked property can be set by feature state', () => {
    setMessagesState(settings.MultiDeviceFeatureState.ENABLED_BY_USER);
    assertTrue(featureToggle.checked_);
    assertTrue(crToggle.checked);

    setMessagesState(settings.MultiDeviceFeatureState.DISABLED_BY_USER);
    assertFalse(featureToggle.checked_);
    assertFalse(crToggle.checked);
  });

  test('disabled property can be set by feature state', () => {
    setMessagesState(settings.MultiDeviceFeatureState.PROHIBITED_BY_POLICY);
    assertTrue(crToggle.disabled);

    setMessagesState(settings.MultiDeviceFeatureState.DISABLED_BY_USER);
    assertFalse(crToggle.disabled);
  });

  test('disabled and checked properties update simultaneously', () => {
    setMessagesState(settings.MultiDeviceFeatureState.ENABLED_BY_USER);
    assertTrue(featureToggle.checked_);
    assertTrue(crToggle.checked);
    assertFalse(crToggle.disabled);

    setMessagesState(settings.MultiDeviceFeatureState.PROHIBITED_BY_POLICY);
    assertFalse(featureToggle.checked_);
    assertFalse(crToggle.checked);
    assertTrue(crToggle.disabled);

    setMessagesState(settings.MultiDeviceFeatureState.DISABLED_BY_USER);
    assertFalse(featureToggle.checked_);
    assertFalse(crToggle.checked);
    assertFalse(crToggle.disabled);
  });

  test('disabled property can be set by suite pref', () => {
    setSuiteState(settings.MultiDeviceFeatureState.DISABLED_BY_USER);
    Polymer.dom.flush();
    assertTrue(crToggle.disabled);

    setSuiteState(settings.MultiDeviceFeatureState.ENABLED_BY_USER);
    Polymer.dom.flush();
    assertFalse(crToggle.disabled);
  });

  test('checked property is unaffected by suite pref', () => {
    setMessagesState(settings.MultiDeviceFeatureState.ENABLED_BY_USER);
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
    const preClickCrToggleChecked = crToggle.checked;
    crToggle.click();
    Polymer.dom.flush();
    assertEquals(crToggle.checked, preClickCrToggleChecked);
  });
});
