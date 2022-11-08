// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrSettingsPrefs, osPageVisibility, Router, routes, setContactManagerForTesting, setNearbyShareSettingsForTesting} from 'chrome://os-settings/chromeos/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeContactManager} from 'chrome://webui-test/nearby_share/shared/fake_nearby_contact_manager.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/nearby_share/shared/fake_nearby_share_settings.js';

let settingsPrefs = null;
/** @type {!FakeContactManager} */
let fakeContactManager = null;
/** @type {!FakeNearbyShareSettings} */
let fakeSettings = null;

suiteSetup(function() {
  settingsPrefs = document.createElement('settings-prefs');
  return CrSettingsPrefs.initialized;
});

suite('MainPageTests', function() {
  /** @type {?SettingsMainElement} */
  let settingsMain = null;

  setup(function() {
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
    settingsMain.pageVisibility = osPageVisibility;
    document.body.appendChild(settingsMain);
  });

  teardown(function() {
    settingsMain.remove();
  });

  function showManagedHeader() {
    return settingsMain.showManagedHeader_(
        settingsMain.inSearchMode_, settingsMain.showingSubpage_,
        settingsMain.showPages_.about);
  }

  test('managed header hides when showing subpage', function() {
    flush();

    assertTrue(showManagedHeader());

    const page = settingsMain.shadowRoot.querySelector('os-settings-page');

    const subpageExpandEvent =
        new CustomEvent('subpage-expand', {'bubbles': true, composed: true});
    page.dispatchEvent(subpageExpandEvent);

    assertFalse(showManagedHeader());
  });

  test('managed header hides when showing about page', function() {
    flush();

    assertTrue(showManagedHeader());
    Router.getInstance().navigateTo(routes.ABOUT);

    assertFalse(showManagedHeader());
  });

  /** @return {!HTMLElement} */
  function getToggleContainer() {
    const page = settingsMain.shadowRoot.querySelector('os-settings-page');
    assertTrue(!!page);
    const toggleContainer = page.shadowRoot.querySelector('#toggleContainer');
    assertTrue(!!toggleContainer);
    return toggleContainer;
  }

  /**
   * Asserts that the Advanced toggle container exists in the combined
   * settings page and asserts whether it should be visible.
   * @param {boolean} expectedVisible
   */
  function assertToggleContainerVisible(expectedVisible) {
    const toggleContainer = getToggleContainer();
    if (expectedVisible) {
      assertNotEquals('none', toggleContainer.style.display);
    } else {
      assertEquals('none', toggleContainer.style.display);
    }
  }

  /**
   * Asserts the visibility of the basic and advanced pages.
   * @param {string} Expected 'display' value for the basic page.
   * @param {string} Expected 'display' value for the advanced page.
   */
  async function assertPageVisibility(expectedBasic, expectedAdvanced) {
    flush();
    const page = settingsMain.shadowRoot.querySelector('os-settings-page');
    assertEquals(
        expectedBasic,
        getComputedStyle(page.shadowRoot.querySelector('#basicPage')).display);

    const advancedPage =
        await page.shadowRoot.querySelector('#advancedPageTemplate').get();
    assertEquals(expectedAdvanced, getComputedStyle(advancedPage).display);
  }

  test('navigating to a basic page does not collapse advanced', async () => {
    Router.getInstance().navigateTo(routes.DATETIME);
    flush();

    assertToggleContainerVisible(true);

    Router.getInstance().navigateTo(routes.DEVICE);
    flush();

    await assertPageVisibility('block', 'block');
  });

  test('updates the title based on current route', function() {
    Router.getInstance().navigateTo(routes.BASIC);
    assertEquals(document.title, loadTimeData.getString('settings'));

    Router.getInstance().navigateTo(routes.ABOUT);
    assertEquals(
        document.title,
        loadTimeData.getStringF(
            'settingsAltPageTitle', loadTimeData.getString('aboutPageTitle')));
  });
});
