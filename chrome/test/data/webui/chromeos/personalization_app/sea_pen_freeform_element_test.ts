// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {SeaPenFreeformElement, SeaPenImagesElement, SeaPenRecentWallpapersElement, SeaPenSamplesElement, setTransitionsEnabled, WallpaperGridItemElement} from 'chrome://personalization/js/personalization_app.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';

suite('SeaPenFreeformElementTest', function() {
  let freeformElement: SeaPenFreeformElement|null = null;

  setup(() => {
    loadTimeData.overrideValues(
        {isSeaPenEnabled: true, isSeaPenTextInputEnabled: true});
    baseSetup();

    // Disables page transition by default.
    setTransitionsEnabled(false);
  });

  teardown(async () => {
    await teardownElement(freeformElement);
    freeformElement = null;
  });

  test('shows default freeform page', async () => {
    // Initialize |freeformElement|.
    freeformElement = initElement(SeaPenFreeformElement);
    await waitAfterNextRender(freeformElement);

    assertTrue(
        !!freeformElement.shadowRoot!.querySelector(SeaPenSamplesElement.is),
        'sample prompts element shown on freeform page');

    const freeformTabsElement =
        freeformElement.shadowRoot!.querySelector('sea-pen-freeform-tabs');
    assertTrue(!!freeformTabsElement, 'freeform tabs element displays');

    const tabStrip = freeformTabsElement.shadowRoot!.querySelector('#tabStrip');
    assertTrue(!!tabStrip, 'tab strip displays');

    // Sample prompts tab should be present and pressed.
    const samplePromptsTabButton =
        tabStrip!.querySelector<CrButtonElement>('#samplePromptsTab');
    assertTrue(!!samplePromptsTabButton, 'sample prompts tab displays');
    assertEquals(samplePromptsTabButton.getAttribute('aria-pressed'), 'true');

    // Results tab should be present but not pressed.
    // TODO(b/347324352): add a test to verify the display elements when the
    // results tab button is hidden.
    const resultsTabButton =
        tabStrip!.querySelector<CrButtonElement>('#resultsTab');
    assertTrue(!!resultsTabButton, 'results tab display');
    assertEquals(resultsTabButton.getAttribute('aria-pressed'), 'false');

    assertTrue(
        !!freeformElement.shadowRoot!.querySelector(
            SeaPenRecentWallpapersElement.is),
        'sea-pen-recent-wallpapers shown on freeform page');

    // sea-pen-images is present but hidden initially as the default tab is
    // Sample Prompts.
    const seaPenImagesElement =
        freeformElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenImagesElement.is);
    assertTrue(!!seaPenImagesElement, 'sea-pen-images is available');
    assertTrue(
        seaPenImagesElement.hidden, 'sea-pen-images is hidden initially');
  });

  test('shows 6 sample prompts in freeform freeform page', async () => {
    // Initialize |freeformElement|.
    freeformElement = initElement(SeaPenFreeformElement);
    await waitAfterNextRender(freeformElement);

    const samplesElement =
        freeformElement.shadowRoot!.querySelector(SeaPenSamplesElement.is);
    assertTrue(
        !!samplesElement, 'sample prompts element shown on freeform page');
    const samples =
        samplesElement.shadowRoot!.querySelectorAll<WallpaperGridItemElement>(
            `${WallpaperGridItemElement.is}:not([hidden])`);
    assertEquals(6, samples.length, 'there are 6 sample prompts');
  });

  test('switches tab in freeform page', async () => {
    // Initialize |freeformElement|.
    freeformElement = initElement(SeaPenFreeformElement);
    await waitAfterNextRender(freeformElement);

    const freeformTabsElement =
        freeformElement.shadowRoot!.querySelector('sea-pen-freeform-tabs');
    assertTrue(!!freeformTabsElement, 'freeform tabs element displays');

    const tabStrip = freeformTabsElement.shadowRoot!.querySelector('#tabStrip');
    assertTrue(!!tabStrip, 'tab strip displays');

    // Switch to Results tab
    const resultsTabButton =
        tabStrip!.querySelector<CrButtonElement>('#resultsTab');
    resultsTabButton!.click();
    await waitAfterNextRender(freeformElement);

    assertFalse(
        !!freeformElement.shadowRoot!.querySelector(
            SeaPenRecentWallpapersElement.is),
        'sea-pen-recent-wallpapers is not shown in Results tab');

    const seaPenImagesElement =
        freeformElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenImagesElement.is);
    assertTrue(!!seaPenImagesElement, 'sea-pen-images is available');
    assertFalse(seaPenImagesElement.hidden, 'sea-pen-images is visible now');
  });
});
