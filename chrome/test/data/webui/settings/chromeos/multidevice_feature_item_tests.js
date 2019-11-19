// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('Multidevice', function() {
  /** @type {?SettingsMultideviceFeatureItemElement} */
  let featureItem = null;
  /** @type {?SettingsMultideviceFeatureToggleElement} */
  let featureToggle = null;
  /** @type {?CrToggleElement} */
  let crToggle = null;
  /** @type {?settings.MultiDeviceFeatureState} */
  let featureState = null;

  /** @type {!settings.Route} */
  let initialRoute;

  // Fake MultiDeviceFeature enum value
  const FAKE_MULTIDEVICE_FEATURE = -1;
  const FAKE_SUMMARY_HTML = 'Gives you candy <a href="link">Learn more.</a>';

  /** Resets both the suite and the (fake) feature to on state. */
  function resetFeatureData() {
    featureState = settings.MultiDeviceFeatureState.ENABLED_BY_USER;
    featureItem.pageContentData = {
      betterTogetherState: settings.MultiDeviceFeatureState.ENABLED_BY_USER,
    };
    Polymer.dom.flush();
    assertFalse(crToggle.disabled);
    assertTrue(crToggle.checked);
  }

  /**
   * Override the cr-toggle's state. Because cr-toggle handles clicks
   * differently depending on its state (e.g., turns off PointerEvents when it's
   * disabled), we use this to check that the toggle can't enter a state where
   * clicking on it causes the page to nagivate away.
   * @param {boolean} checked
   * @param {boolean} disabled
   */
  function setCrToggle(checked, disabled) {
    crToggle.checked = checked;
    crToggle.disabled = disabled;
  }

  /**
   * Clicks an element, asserts whether the click navigated the page away to a
   * new route, then navigates back to initialRoute.
   * @param {HTMLElement} element. Target of click.
   * @param {boolean} shouldRouteAway. Whether the page is expected to navigate
   * to a new route.
   */
  function checkWhetherClickRoutesAway(element, shouldRouteAway) {
    element.click();
    Polymer.dom.flush();
    assertEquals(shouldRouteAway, initialRoute !== settings.getCurrentRoute());
    settings.navigateTo(initialRoute);
    assertEquals(initialRoute, settings.getCurrentRoute());
  }

  setup(function() {
    PolymerTest.clearBody();

    featureItem = document.createElement('settings-multidevice-feature-item');
    featureItem.getFeatureSummaryHtml = () => FAKE_SUMMARY_HTML;
    featureItem.feature = FAKE_MULTIDEVICE_FEATURE;
    featureItem.pageContentData = {};
    document.body.appendChild(featureItem);
    Polymer.dom.flush();

    featureToggle = featureItem.$$('settings-multidevice-feature-toggle');
    featureToggle.getFeatureState = () => featureState;

    crToggle = featureToggle.$.toggle;

    initialRoute = settings.routes.MULTIDEVICE_FEATURES;
    settings.routes.FREE_CANDY =
        settings.routes.BASIC.createSection('/freeCandy');
    featureItem.subpageRoute = settings.routes.FREE_CANDY;

    resetFeatureData();
    settings.navigateTo(initialRoute);
    Polymer.dom.flush();
  });

  teardown(function() {
    featureItem.remove();
  });

  test('generic click navigates to subpage', function() {
    checkWhetherClickRoutesAway(featureItem.$$('#item-text-container'), true);
    checkWhetherClickRoutesAway(featureItem.$$('iron-icon'), true);
    checkWhetherClickRoutesAway(featureItem.$$('#featureSecondary'), true);
  });

  test('link click does not navigate to subpage', function() {
    const link = featureItem.$$('#featureSecondary > a');
    assertTrue(!!link);
    checkWhetherClickRoutesAway(link, false);
  });

  test('toggle click does not navigate to subpage in any state', function() {
    checkWhetherClickRoutesAway(featureToggle, false);

    // Checked and enabled
    setCrToggle(true, true);
    checkWhetherClickRoutesAway(crToggle, false);

    // Checked and disabled
    setCrToggle(true, false);
    checkWhetherClickRoutesAway(crToggle, false);

    // Unchecked and enabled
    setCrToggle(false, true);
    checkWhetherClickRoutesAway(crToggle, false);

    // Unchecked and disabled
    setCrToggle(false, false);
    checkWhetherClickRoutesAway(crToggle, false);
  });
});
