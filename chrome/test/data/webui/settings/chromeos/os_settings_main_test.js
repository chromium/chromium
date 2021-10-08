// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {CrSettingsPrefs, osPageVisibility, Router, routes, setNearbyShareSettingsForTesting, setContactManagerForTesting} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {flush} from'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {TestBrowserProxy} from '../../test_browser_proxy.js';
// #import {FakeNearbyShareSettings} from '../../nearby_share/shared/fake_nearby_share_settings.m.js';
// #import {FakeContactManager} from '../../nearby_share/shared/fake_nearby_contact_manager.m.js';
// clang-format on

cr.define('settings_main_page', function() {
  let settingsPrefs = null;
  /** @type {!nearby_share.FakeContactManager} */
  let fakeContactManager = null;
  /** @type {!nearby_share.FakeNearbyShareSettings} */
  let fakeSettings = null;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  suite('MainPageTests', function() {
    /** @type {?SettingsMainElement} */
    let settingsMain = null;

    setup(function() {
      fakeContactManager = new nearby_share.FakeContactManager();
      nearby_share.setContactManagerForTesting(fakeContactManager);
      fakeContactManager.setupContactRecords();

      fakeSettings = new nearby_share.FakeNearbyShareSettings();
      nearby_share.setNearbyShareSettingsForTesting(fakeSettings);

      settings.Router.getInstance().navigateTo(settings.routes.BASIC);
      PolymerTest.clearBody();
      settingsMain = document.createElement('os-settings-main');
      settingsMain.prefs = settingsPrefs.prefs;
      settingsMain.toolbarSpinnerActive = false;
      settingsMain.pageVisibility = settings.osPageVisibility;
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
      Polymer.dom.flush();

      assertTrue(showManagedHeader());

      const page = settingsMain.$$('os-settings-page');
      page.fire('subpage-expand', {});

      assertFalse(showManagedHeader());
    });

    test('managed header hides when showing about page', function() {
      Polymer.dom.flush();

      assertTrue(showManagedHeader());
      settings.Router.getInstance().navigateTo(settings.routes.ABOUT);

      assertFalse(showManagedHeader());
    });

    /** @return {!HTMLElement} */
    function getToggleContainer() {
      const page = settingsMain.$$('os-settings-page');
      assertTrue(!!page);
      const toggleContainer = page.$$('#toggleContainer');
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
      Polymer.dom.flush();
      const page = settingsMain.$$('os-settings-page');
      assertEquals(
          expectedBasic, getComputedStyle(page.$$('#basicPage')).display);

      const advancedPage = await page.$$('#advancedPageTemplate').get();
      assertEquals(expectedAdvanced, getComputedStyle(advancedPage).display);
    }

    test('navigating to a basic page does not collapse advanced', async () => {
      settings.Router.getInstance().navigateTo(settings.routes.DATETIME);
      Polymer.dom.flush();

      assertToggleContainerVisible(true);

      settings.Router.getInstance().navigateTo(settings.routes.DEVICE);
      Polymer.dom.flush();

      await assertPageVisibility('block', 'block');
    });

    test('updates the title based on current route', function() {
      settings.Router.getInstance().navigateTo(settings.routes.BASIC);
      assertEquals(document.title, loadTimeData.getString('settings'));

      settings.Router.getInstance().navigateTo(settings.routes.ABOUT);
      assertEquals(
          document.title,
          loadTimeData.getStringF(
              'settingsAltPageTitle',
              loadTimeData.getString('aboutPageTitle')));
    });
  });
  // #cr_define_end
});
