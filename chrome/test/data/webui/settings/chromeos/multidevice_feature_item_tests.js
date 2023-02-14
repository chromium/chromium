// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MultiDeviceFeature, MultiDeviceFeatureState, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('Multidevice', function() {
  /** @type {?SettingsMultideviceFeatureItemElement} */
  let featureItem = null;
  /** @type {?SettingsMultideviceFeatureToggleElement} */
  let featureToggle = null;
  /** @type {?CrToggleElement} */
  let crToggle = null;
  /** @type {?MultiDeviceFeatureState} */
  let featureState = null;

  /** @type {!Route} */
  let initialRoute;

  // Fake MultiDeviceFeature enum value
  const FAKE_MULTIDEVICE_FEATURE = -1;
  const FAKE_SUMMARY_HTML = 'Gives you candy <a href="link">Learn more.</a>';

  /** Resets both the suite and the (fake) feature to on state. */
  function resetFeatureData() {
    featureState = MultiDeviceFeatureState.ENABLED_BY_USER;
    featureItem.pageContentData = {
      betterTogetherState: MultiDeviceFeatureState.ENABLED_BY_USER,
    };
    flush();
    assertFalse(crToggle.disabled);
    assertTrue(crToggle.checked);
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
    flush();
    assertEquals(
        shouldRouteAway, initialRoute !== Router.getInstance().currentRoute);
    Router.getInstance().navigateTo(initialRoute);
    assertEquals(initialRoute, Router.getInstance().currentRoute);
  }

  setup(function() {
    PolymerTest.clearBody();

    featureItem = document.createElement('settings-multidevice-feature-item');
    featureItem.getFeatureSummaryHtml = () => FAKE_SUMMARY_HTML;
    featureItem.feature = FAKE_MULTIDEVICE_FEATURE;
    featureItem.pageContentData = {};
    document.body.appendChild(featureItem);
    flush();

    featureToggle = featureItem.shadowRoot.querySelector(
        'settings-multidevice-feature-toggle');
    featureToggle.getFeatureState = () => featureState;

    crToggle = featureToggle.$.toggle;

    initialRoute = routes.MULTIDEVICE_FEATURES;
    routes.FREE_CANDY = routes.BASIC.createSection('/freeCandy');
    featureItem.subpageRoute = routes.FREE_CANDY;

    resetFeatureData();
    Router.getInstance().navigateTo(initialRoute);
    flush();
  });

  teardown(function() {
    featureItem.remove();
  });

  test('generic click navigates to subpage', function() {
    checkWhetherClickRoutesAway(
        featureItem.shadowRoot.querySelector('#item-text-container'), true);
    checkWhetherClickRoutesAway(
        featureItem.shadowRoot.querySelector('iron-icon'), true);
    checkWhetherClickRoutesAway(
        featureItem.shadowRoot.querySelector('#featureSecondary'), true);
  });

  test('link click does not navigate to subpage', function() {
    const link = featureItem.shadowRoot.querySelector('#featureSecondary')
                     .$.container.querySelector('a');
    assertTrue(!!link);
    checkWhetherClickRoutesAway(link, false);
  });

  test('row is clickable', async () => {
    featureItem.feature = MultiDeviceFeature.BETTER_TOGETHER_SUITE;
    featureState = MultiDeviceFeatureState.ENABLED_BY_USER;
    featureItem.subpageRoute = null;
    flush();

    const expectedEvent =
        eventToPromise('feature-toggle-clicked', featureToggle);
    featureItem.shadowRoot.querySelector('#linkWrapper').click();
    await expectedEvent;
  });

  test('toggle click does not navigate to subpage in any state', function() {
    checkWhetherClickRoutesAway(featureToggle, false);

    crToggle.checked = true;
    assertFalse(crToggle.disabled);
    checkWhetherClickRoutesAway(crToggle, false);

    crToggle.checked = false;
    assertFalse(crToggle.disabled);
    checkWhetherClickRoutesAway(crToggle, false);
  });
});
