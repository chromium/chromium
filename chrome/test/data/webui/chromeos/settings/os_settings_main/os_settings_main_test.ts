// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {OsSettingsMainElement, SettingsPrefsElement} from 'chrome://os-settings/os_settings.js';
import {createPageAvailabilityForTesting, CrSettingsPrefs, Router, routes, setContactManagerForTesting, setNearbyShareSettingsForTesting} from 'chrome://os-settings/os_settings.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeContactManager} from 'chrome://webui-test/chromeos/nearby_share/shared/fake_nearby_contact_manager.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/chromeos/nearby_share/shared/fake_nearby_share_settings.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

let settingsPrefs: SettingsPrefsElement;
let fakeContactManager: FakeContactManager;
let fakeSettings: FakeNearbyShareSettings;

suiteSetup(async () => {
  settingsPrefs = document.createElement('settings-prefs');
  await CrSettingsPrefs.initialized;
});

suite('<os-settings-main>', () => {
  let settingsMain: OsSettingsMainElement;

  setup(async () => {
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
    await flushTasks();
  });

  teardown(() => {
    settingsMain.remove();
    Router.getInstance().resetRouteForTesting();
  });

  function isShowingManagedHeader(): boolean {
    return !!settingsMain.shadowRoot!.querySelector('managed-footnote');
  }

  test('managed header hides when showing subpage', async () => {
    const mainPageContainer =
        settingsMain.shadowRoot!.querySelector('main-page-container');
    assertTrue(!!mainPageContainer);

    const showingMainPageEvent =
        new CustomEvent('showing-main-page', {bubbles: true, composed: true});
    mainPageContainer.dispatchEvent(showingMainPageEvent);
    await flushTasks();
    assertTrue(isShowingManagedHeader());

    const showingSubpageEvent =
        new CustomEvent('showing-subpage', {bubbles: true, composed: true});
    mainPageContainer.dispatchEvent(showingSubpageEvent);
    await flushTasks();
    assertFalse(isShowingManagedHeader());
  });
});
