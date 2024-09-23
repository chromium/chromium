// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://personalization/strings.m.js';

import {AmbientActionName, PersonalizationMainElement, SetShouldShowTimeOfDayBannerAction} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

suite('PersonalizationMainElementTest', function() {
  let personalizationMainElement: PersonalizationMainElement|null;
  let personalizationStore: TestPersonalizationStore;

  setup(() => {
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
  });

  teardown(async () => {
    await teardownElement(personalizationMainElement);
    personalizationMainElement = null;
  });

  test('has large ambient preview when ambient allowed', async () => {
    loadTimeData.overrideValues({isAmbientModeAllowed: true});
    personalizationMainElement = initElement(PersonalizationMainElement);
    await waitAfterNextRender(personalizationMainElement);

    const preview = personalizationMainElement!.shadowRoot!.querySelector(
        'ambient-preview-large')!;
    assertTrue(!!preview, 'ambient preview exists');
  });

  test('has preview when ambient disallowed', async () => {
    loadTimeData.overrideValues({isAmbientModeAllowed: false});
    personalizationMainElement = initElement(PersonalizationMainElement);
    await waitAfterNextRender(personalizationMainElement);

    const preview = personalizationMainElement!.shadowRoot!.querySelector(
        'ambient-preview-large')!;
    assertTrue(!!preview, 'ambient preview exists');
  });

  test('time of day banner', async () => {
    personalizationStore.setReducersEnabled(true);
    personalizationStore.data.ambient.shouldShowTimeOfDayBanner = true;
    personalizationStore.notifyObservers();
    personalizationMainElement = initElement(PersonalizationMainElement);
    await waitAfterNextRender(personalizationMainElement);

    const banner = personalizationMainElement!.shadowRoot!.querySelector(
        'time-of-day-banner');
    assertTrue(!!banner, 'time of day banner exists');

    // Verify clicking on dismiss button hides the banner.
    const dismissButton = banner.shadowRoot!.getElementById('dismissButton');
    assertTrue(!!dismissButton, 'dismiss button exists');
    personalizationStore.expectAction(
        AmbientActionName.SET_SHOULD_SHOW_TIME_OF_DAY_BANNER);
    dismissButton.click();
    const action = await personalizationStore.waitForAction(
                       AmbientActionName.SET_SHOULD_SHOW_TIME_OF_DAY_BANNER) as
        SetShouldShowTimeOfDayBannerAction;
    assertFalse(action.shouldShowTimeOfDayBanner);
    assertEquals(
        'none', getComputedStyle(banner).display, 'banner is displayed none');
  });
});
