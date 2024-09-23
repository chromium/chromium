// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {createPageAvailabilityForTesting, CrSettingsPrefs, OsSettingsMainElement, Router, routes, setContactManagerForTesting, setNearbyShareSettingsForTesting, SettingsPrefsElement} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeContactManager} from 'chrome://webui-test/chromeos/nearby_share/shared/fake_nearby_contact_manager.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/chromeos/nearby_share/shared/fake_nearby_share_settings.js';

let settingsPrefs: SettingsPrefsElement;
let fakeContactManager: FakeContactManager;
let fakeSettings: FakeNearbyShareSettings;

suiteSetup(async () => {
  settingsPrefs = document.createElement('settings-prefs');
  await CrSettingsPrefs.initialized;
});

suite('<os-settings-main>', () => {
  let settingsMain: OsSettingsMainElement;

  setup(() => {
    fakeContactManager = new FakeContactManager();
    setContactManagerForTesting(fakeContactManager);
    fakeContactManager.setupContactRecords();

    fakeSettings = new FakeNearbyShareSettings();
    setNearbyShareSettingsForTesting(fakeSettings);

    Router.getInstance().navigateTo(routes.BASIC);

    settingsMain = document.createElement('os-settings-main');
    settingsMain.prefs = settingsPrefs.prefs!;
    settingsMain.toolbarSpinnerActive = false;
    settingsMain.pageAvailability = createPageAvailabilityForTesting();
    document.body.appendChild(settingsMain);
    flush();
  });

  teardown(() => {
    settingsMain.remove();
    Router.getInstance().resetRouteForTesting();
  });

  function isShowingManagedHeader(): boolean {
    return !!settingsMain.shadowRoot!.querySelector('managed-footnote');
  }

  test('managed header hides when showing subpage', () => {
    assertTrue(isShowingManagedHeader());

    const mainPageContainer =
        settingsMain.shadowRoot!.querySelector('main-page-container');
    assertTrue(!!mainPageContainer);

    const showingSubpageEvent =
        new CustomEvent('showing-subpage', {bubbles: true, composed: true});
    mainPageContainer.dispatchEvent(showingSubpageEvent);
    flush();
    assertFalse(isShowingManagedHeader());
  });

  test('managed header hides when showing about page', () => {
    assertTrue(isShowingManagedHeader());
    Router.getInstance().navigateTo(routes.ABOUT);
    flush();
    assertFalse(isShowingManagedHeader());
  });

  test('Basic page has the default title', () => {
    Router.getInstance().navigateTo(routes.BASIC);
    assertEquals(loadTimeData.getString('settings'), document.title);
  });

  test('About page has a custom title', () => {
    Router.getInstance().navigateTo(routes.ABOUT);
    assertEquals(
        loadTimeData.getStringF(
            'settingsAltPageTitle', loadTimeData.getString('aboutPageTitle')),
        document.title);
  });
});
