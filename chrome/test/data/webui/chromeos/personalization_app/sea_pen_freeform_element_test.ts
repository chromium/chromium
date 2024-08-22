// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {SeaPenFreeformElement, SeaPenImagesElement, SeaPenRecentWallpapersElement, SeaPenSamplesElement, setTransitionsEnabled, WallpaperGridItemElement} from 'chrome://personalization/js/personalization_app.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {MantaStatusCode} from 'chrome://resources/ash/common/sea_pen/sea_pen.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assert} from 'chrome://webui-test/chai.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestSeaPenProvider} from './test_sea_pen_interface_provider.js';

suite('SeaPenFreeformElementTest', function() {
  let freeformElement: SeaPenFreeformElement|null = null;
  let personalizationStore: TestPersonalizationStore;
  let seaPenProvider: TestSeaPenProvider;

  function getSamples(): string[] {
    const seaPenSamplesElement =
        freeformElement!.shadowRoot!.querySelector(SeaPenSamplesElement.is);
    const samples = seaPenSamplesElement!.shadowRoot!
                        .querySelectorAll<WallpaperGridItemElement>(
                            `${WallpaperGridItemElement.is}:not([hidden])`);
    assertTrue(!!samples, 'samples should exist');

    return Array.from(samples).flatMap(sample => {
      const text =
          sample!.shadowRoot!.querySelector('.primary-text')?.innerHTML;
      return text ? [text] : [];
    });
  }

  setup(() => {
    loadTimeData.overrideValues(
        {isSeaPenEnabled: true, isSeaPenTextInputEnabled: true});
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    seaPenProvider = mocks.seaPenProvider;

    // Disables page transition by default.
    setTransitionsEnabled(false);
  });

  teardown(async () => {
    await teardownElement(freeformElement);
    freeformElement = null;
  });

  test('shows default freeform page without tab container', async () => {
    // Initialize |freeformElement|.
    freeformElement = initElement(SeaPenFreeformElement);
    await waitAfterNextRender(freeformElement);

    assertTrue(
        !!freeformElement.shadowRoot!.querySelector(SeaPenSamplesElement.is),
        'sample prompts element shown on freeform page');

    assertFalse(
        !!freeformElement.shadowRoot!.querySelector('#tabContainer'),
        'tab container is not shown');
    assertTrue(
        !!freeformElement.shadowRoot!.querySelector('#promptingGuide'),
        'Prompting guide is shown');
    assertTrue(
        !!freeformElement.shadowRoot!.querySelector(SeaPenSamplesElement.is),
        'sea-pen-samples shown on freeform page');
    assertTrue(
        !!freeformElement.shadowRoot!.querySelector(
            SeaPenRecentWallpapersElement.is),
        'sea-pen-recent-wallpapers shown on freeform page');
    assertTrue(
        !!freeformElement!.shadowRoot!.getElementById('shuffle'),
        'shuffle button should be shown');
    assertTrue(
        !!freeformElement!.shadowRoot!.getElementById('promptingGuide'),
        'prompting guide should exist');

    assertFalse(
        !!freeformElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenImagesElement.is),
        'sea-pen-images is not shown');
  });

  test('shows 6 sample prompts in freeform freeform page', async () => {
    // Initialize |freeformElement|.
    freeformElement = initElement(SeaPenFreeformElement);
    await waitAfterNextRender(freeformElement);

    const samplesElement =
        freeformElement.shadowRoot!.querySelector(SeaPenSamplesElement.is);
    await waitAfterNextRender(samplesElement as HTMLElement);

    assertTrue(
        !!samplesElement, 'sample prompts element shown on freeform page');
    assertEquals(6, getSamples().length, 'there are 6 sample prompts');
  });

  test('shows freeform page with tab container', async () => {
    personalizationStore.data.wallpaper.seaPen.currentSeaPenQuery =
        seaPenProvider.seaPenFreeformQuery;

    // Initialize |freeformElement|.
    freeformElement = initElement(SeaPenFreeformElement);
    await waitAfterNextRender(freeformElement);

    const tabContainer =
        freeformElement.shadowRoot!.querySelector('#tabContainer');
    assertTrue(!!tabContainer, 'tab container displays');

    // Sample prompts tab should be present and but not pressed.
    const samplePromptsTabButton =
        tabContainer!.querySelector<CrButtonElement>('#samplePromptsTab');
    assertTrue(!!samplePromptsTabButton, 'sample prompts tab displays');
    assertEquals(samplePromptsTabButton.getAttribute('aria-pressed'), 'false');

    // Results tab should be present and pressed.
    const resultsTabButton =
        tabContainer!.querySelector<CrButtonElement>('#resultsTab');
    assertTrue(!!resultsTabButton, 'results tab display');
    assertEquals(resultsTabButton.getAttribute('aria-pressed'), 'true');

    assertFalse(
        !!freeformElement.shadowRoot!.querySelector(SeaPenSamplesElement.is),
        'sea-pen-samples is not shown');

    assertFalse(
        !!freeformElement.shadowRoot!.querySelector(
            SeaPenRecentWallpapersElement.is),
        'sea-pen-recent-wallpapers is not shown');
    assertFalse(
        !!freeformElement!.shadowRoot!.getElementById('shuffle'),
        'shuffle button should be hidden for results tab');
    assertFalse(
        !!freeformElement!.shadowRoot!.getElementById('promptingGuide'),
        'prompting guide should be hidden for results tab');

    assertTrue(
        !!freeformElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenImagesElement.is),
        'sea-pen-images is shown');
  });

  test('switches tab in freeform page', async () => {
    personalizationStore.data.wallpaper.seaPen.currentSeaPenQuery =
        seaPenProvider.seaPenFreeformQuery;

    // Initialize |freeformElement|.
    freeformElement = initElement(SeaPenFreeformElement);
    await waitAfterNextRender(freeformElement);

    const tabContainer =
        freeformElement.shadowRoot!.querySelector('#tabContainer');
    assertTrue(!!tabContainer, 'tab container displays');

    // Switch to Sample Prompts tab
    const samplePromptsTabButton =
        tabContainer!.querySelector<CrButtonElement>('#samplePromptsTab');
    samplePromptsTabButton!.click();
    await waitAfterNextRender(freeformElement);

    assertTrue(
        !!freeformElement.shadowRoot!.querySelector(SeaPenSamplesElement.is),
        'sea-pen-samples is shown in Sample Prompts tab');
    assertTrue(
        !!freeformElement.shadowRoot!.querySelector(
            SeaPenRecentWallpapersElement.is),
        'sea-pen-recent-wallpapers is shown in Sample Prompts tab');
    assertTrue(
        !!freeformElement!.shadowRoot!.getElementById('shuffle'),
        'shuffle button should be shown for samples tab');
    assertTrue(
        !!freeformElement!.shadowRoot!.getElementById('promptingGuide'),
        'prompting guide should be shown for samples tab');
    assertFalse(
        !!freeformElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenImagesElement.is),
        'sea-pen-images is not shown');
  });

  test('shuffles samples', async () => {
    freeformElement = initElement(SeaPenFreeformElement);
    await waitAfterNextRender(freeformElement);

    let seaPenSamplesElement =
        freeformElement.shadowRoot!.querySelector(SeaPenSamplesElement.is);
    await waitAfterNextRender(seaPenSamplesElement as HTMLElement);
    assertTrue(!!seaPenSamplesElement, 'sea-pen-samples is visible');
    const originalSamples = getSamples();

    // Click shuffle button
    const shuffleButton =
        freeformElement!.shadowRoot!.getElementById('shuffle');
    shuffleButton!.click();
    await waitAfterNextRender(freeformElement);
    seaPenSamplesElement =
        freeformElement.shadowRoot!.querySelector(SeaPenSamplesElement.is);
    await waitAfterNextRender(seaPenSamplesElement as HTMLElement);

    assert.notSameOrderedMembers(
        originalSamples, getSamples(), 'the order should be different');
  });

  test('shuffles samples with tab', async () => {
    personalizationStore.data.wallpaper.seaPen.currentSeaPenQuery =
        seaPenProvider.seaPenFreeformQuery;
    freeformElement = initElement(SeaPenFreeformElement);
    await waitAfterNextRender(freeformElement);
    const tabContainer =
        freeformElement.shadowRoot!.querySelector('#tabContainer');

    // Switch to Sample Prompts tab
    const samplePromptsTabButton =
        tabContainer!.querySelector<CrButtonElement>('#samplePromptsTab');
    samplePromptsTabButton!.click();
    await waitAfterNextRender(freeformElement);
    let seaPenSamplesElement =
        freeformElement.shadowRoot!.querySelector(SeaPenSamplesElement.is);
    await waitAfterNextRender(seaPenSamplesElement as HTMLElement);
    assertTrue(
        !!seaPenSamplesElement,
        'sea-pen-samples is shown in Sample Prompts tab');
    const originalSamples = getSamples();

    // Click shuffle button
    const shuffleButton =
        freeformElement!.shadowRoot!.getElementById('shuffle');
    shuffleButton!.click();
    await waitAfterNextRender(freeformElement);
    seaPenSamplesElement =
        freeformElement.shadowRoot!.querySelector(SeaPenSamplesElement.is);
    await waitAfterNextRender(seaPenSamplesElement as HTMLElement);

    assert.notSameOrderedMembers(
        originalSamples, getSamples(), 'the order should be different');
  });

  test('shows results page for errors', async () => {
    personalizationStore.data.wallpaper.seaPen.thumbnailResponseStatusCode =
        MantaStatusCode.kGenericError;
    freeformElement = initElement(SeaPenFreeformElement);
    await waitAfterNextRender(freeformElement);

    // Results tab should be present and pressed.
    const tabContainer =
        freeformElement.shadowRoot!.querySelector('#tabContainer');
    assertTrue(!!tabContainer, 'tab container should exist');
    const resultsTabButton =
        tabContainer!.querySelector<CrButtonElement>('#resultsTab');
    assertTrue(!!resultsTabButton, 'results tab displays');
    assertEquals(
        'true', resultsTabButton.getAttribute('aria-pressed'),
        'results tab is pressed');
    assertTrue(
        !!freeformElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenImagesElement.is),
        'sea-pen-images is shown');
  });
});
