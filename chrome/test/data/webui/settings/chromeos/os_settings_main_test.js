// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {createPageAvailabilityForTesting, CrSettingsPrefs, Router, routes, setContactManagerForTesting, setNearbyShareSettingsForTesting} from 'chrome://os-settings/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeContactManager} from 'chrome://webui-test/nearby_share/shared/fake_nearby_contact_manager.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/nearby_share/shared/fake_nearby_share_settings.js';

let settingsPrefs = null;
/** @type {!FakeContactManager} */
let fakeContactManager = null;
/** @type {!FakeNearbyShareSettings} */
let fakeSettings = null;

suiteSetup(() => {
  settingsPrefs = document.createElement('settings-prefs');
  return CrSettingsPrefs.initialized;
});

suite('MainPageTests', () => {
  /** @type {?SettingsMainElement} */
  let settingsMain = null;

  setup(() => {
    fakeContactManager = new FakeContactManager();
    setContactManagerForTesting(fakeContactManager);
    fakeContactManager.setupContactRecords();

    fakeSettings = new FakeNearbyShareSettings();
    setNearbyShareSettingsForTesting(fakeSettings);

    Router.getInstance().navigateTo(routes.BASIC);
    PolymerTest.clearBody();
    settingsMain = document.createElement('os-settings-main');
    settingsMain.prefs = settingsPrefs.prefs;
    settingsMain.toolbarSpinnerActive = false;
    settingsMain.pageAvailability = createPageAvailabilityForTesting();
    document.body.appendChild(settingsMain);
    flush();
  });

  teardown(() => {
    settingsMain.remove();
    Router.getInstance().resetRouteForTesting();
  });

  function isShowingManagedHeader() {
    return settingsMain.showManagedHeader_();
  }

  test('managed header hides when showing subpage', () => {
    assertTrue(isShowingManagedHeader());

    const mainPageContainer =
        settingsMain.shadowRoot.querySelector('main-page-container');

    const showingSubpageEvent =
        new CustomEvent('showing-subpage', {bubbles: true, composed: true});
    mainPageContainer.dispatchEvent(showingSubpageEvent);

    assertFalse(isShowingManagedHeader());
  });

  test('managed header hides when showing about page', () => {
    assertTrue(isShowingManagedHeader());
    Router.getInstance().navigateTo(routes.ABOUT);

    assertFalse(isShowingManagedHeader());
  });

  /** @return {!HTMLElement} */
  function getToggleContainer() {
    const mainPageContainer =
        settingsMain.shadowRoot.querySelector('main-page-container');
    assertTrue(!!mainPageContainer);
    const toggleContainer =
        mainPageContainer.shadowRoot.querySelector('#toggleContainer');
    assertTrue(!!toggleContainer);
    return toggleContainer;
  }

  test('Basic page has the default title', () => {
    Router.getInstance().navigateTo(routes.BASIC);
    assertEquals(document.title, loadTimeData.getString('settings'));
  });

  test('About page has a custom title', () => {
    Router.getInstance().navigateTo(routes.ABOUT);
    assertEquals(
        document.title,
        loadTimeData.getStringF(
            'settingsAltPageTitle', loadTimeData.getString('aboutPageTitle')));
  });
});
