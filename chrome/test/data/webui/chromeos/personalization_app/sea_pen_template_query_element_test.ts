// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {SeaPenOptionsElement, SeaPenPaths, SeaPenRouterElement, SeaPenTemplateQueryElement} from 'chrome://personalization/js/personalization_app.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {SeaPenQuery} from 'chrome://resources/ash/common/sea_pen/sea_pen.mojom-webui.js';
import {SeaPenTemplateId} from 'chrome://resources/ash/common/sea_pen/sea_pen_generated.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestSeaPenProvider} from './test_sea_pen_interface_provider.js';

suite('SeaPenTemplateQueryElementTest', function() {
  let seaPenTemplateQueryElement: SeaPenTemplateQueryElement|null;
  let personalizationStore: TestPersonalizationStore;
  let seaPenProvider: TestSeaPenProvider;

  function getThumbnailsLoadingText(): HTMLSpanElement|null {
    return seaPenTemplateQueryElement!.shadowRoot!.querySelector(
        '#thumbnailsLoadingText');
  }

  function getSearchButtons(): CrButtonElement[] {
    return Array.from(seaPenTemplateQueryElement!.shadowRoot!.querySelectorAll(
        '#searchButtons cr-button'));
  }

  setup(() => {
    loadTimeData.overrideValues(
        {isSeaPenEnabled: true, isSeaPenTextInputEnabled: false});
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    seaPenProvider = mocks.seaPenProvider;
  });

  teardown(async () => {
    await teardownElement(seaPenTemplateQueryElement);
    seaPenTemplateQueryElement = null;
  });

  test('displays sea pen template', async () => {
    seaPenTemplateQueryElement = initElement(SeaPenTemplateQueryElement, {
      path: SeaPenPaths.ROOT,
      templateId: SeaPenTemplateId.kFlower.toString(),
    });
    await waitAfterNextRender(seaPenTemplateQueryElement);

    const chips =
        seaPenTemplateQueryElement.shadowRoot!.querySelectorAll('.chip-text');
    const options = seaPenTemplateQueryElement.shadowRoot!.querySelectorAll(
        'button.dropdown-item:not([hidden])');
    const unselectedTemplate =
        seaPenTemplateQueryElement.shadowRoot!.querySelectorAll(
            '#template .unselected');
    const searchButton =
        seaPenTemplateQueryElement.shadowRoot!.querySelector<CrButtonElement>(
            '#searchButton');

    assertTrue(chips.length > 0, 'there should be chips to select');
    assertEquals(
        0, options.length, 'there should be no options available to select');
    assertEquals(
        2, getSearchButtons().length, 'there should be two search buttons');
    assertEquals(
        0, unselectedTemplate.length,
        'there should be no unselected templates');
    assertEquals(
        seaPenTemplateQueryElement.i18n('seaPenCreateButton'),
        searchButton!.innerText);
  });

  test('displays search again button on results page', async () => {
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.images;
    seaPenTemplateQueryElement = initElement(SeaPenTemplateQueryElement, {
      path: SeaPenPaths.RESULTS,
      templateId: SeaPenTemplateId.kFlower.toString(),
    });
    await waitAfterNextRender(seaPenTemplateQueryElement);

    const searchButton =
        seaPenTemplateQueryElement.shadowRoot!.querySelector<HTMLElement>(
            '#searchButton');
    const icon = searchButton!.querySelector<HTMLElement>('iron-icon');
    assertEquals(
        seaPenTemplateQueryElement.i18n('seaPenRecreateButton'),
        searchButton!.innerText);
    assertEquals('personalization-shared:refresh', icon!.getAttribute('icon'));
  });

  test('displays create button when no thumbnails are generated', async () => {
    seaPenTemplateQueryElement = initElement(SeaPenTemplateQueryElement, {
      path: SeaPenPaths.RESULTS,
      templateId: SeaPenTemplateId.kFlower.toString(),
    });
    await waitAfterNextRender(seaPenTemplateQueryElement);

    const searchButton =
        seaPenTemplateQueryElement.shadowRoot!.querySelector<HTMLElement>(
            '#searchButton');
    const icon = searchButton!.querySelector<HTMLElement>('iron-icon');
    assertEquals(
        seaPenTemplateQueryElement.i18n('seaPenCreateButton'),
        searchButton!.innerText);
    assertEquals('sea-pen:photo-spark', icon!.getAttribute('icon'));
  });

  test('displays create button when selected option changes', async () => {
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.images;
    seaPenTemplateQueryElement = initElement(SeaPenTemplateQueryElement, {
      path: SeaPenPaths.RESULTS,
      templateId: SeaPenTemplateId.kFlower.toString(),
    });
    await waitAfterNextRender(seaPenTemplateQueryElement);

    const searchButton =
        seaPenTemplateQueryElement.shadowRoot!.querySelector<HTMLElement>(
            '#searchButton');
    const icon = searchButton!.querySelector<HTMLElement>('iron-icon');
    assertEquals(
        seaPenTemplateQueryElement.i18n('seaPenRecreateButton'),
        searchButton!.innerText);
    assertEquals('personalization-shared:refresh', icon!.getAttribute('icon'));

    const chips =
        seaPenTemplateQueryElement.shadowRoot!.querySelectorAll('.chip-text');
    const chip = chips[0] as HTMLElement;
    chip!.click();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    const seaPenOptionsElement =
        seaPenTemplateQueryElement.shadowRoot!.querySelector(
            SeaPenOptionsElement.is);
    assertTrue(
        !!seaPenOptionsElement,
        'the options chips should show after clicking a chip');
    const optionToSelect =
        seaPenOptionsElement.shadowRoot!.querySelector<CrButtonElement>(
            '#container cr-button:not([aria-selected])');
    const optionText = optionToSelect!.innerText;
    assertTrue(
        optionText !== chip.innerText,
        'unselected option should not match text');

    optionToSelect!.click();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    assertEquals(
        seaPenTemplateQueryElement.i18n('seaPenCreateButton'),
        searchButton!.innerText);
    assertEquals('sea-pen:photo-spark', icon!.getAttribute('icon'));
  });

  test('selects chip', async () => {
    seaPenTemplateQueryElement = initElement(
        SeaPenTemplateQueryElement,
        {templateId: SeaPenTemplateId.kFlower.toString()});
    await waitAfterNextRender(seaPenTemplateQueryElement);

    const chips =
        seaPenTemplateQueryElement.shadowRoot!.querySelectorAll('.chip-text');
    const chipToSelect = chips[0] as HTMLElement;

    chipToSelect!.click();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    const seaPenOptionsElement =
        seaPenTemplateQueryElement.shadowRoot!.querySelector(
            SeaPenOptionsElement.is);
    assertTrue(
        !!seaPenOptionsElement,
        'the options chips should show after clicking a chip');
    const options = seaPenOptionsElement.shadowRoot!.querySelectorAll(
        '#container cr-button');
    assertTrue(
        options.length > 0, 'there should be options available to select');
    const selectedOption =
        seaPenOptionsElement.shadowRoot!.querySelector<CrButtonElement>(
            '#container cr-button[aria-selected]');
    assertEquals(
        chipToSelect.innerText, selectedOption!.innerText,
        'the selected chip should have an equivalent selected option');
    const selectedChip =
        seaPenTemplateQueryElement.shadowRoot!.querySelectorAll(
            '#template .selected .chip-text');
    assertEquals(
        1, selectedChip.length,
        'There should be exactly one chip that is selected.');
    assertEquals(selectedChip[0] as HTMLElement, chipToSelect);
  });

  test('option contains preview image', async () => {
    seaPenTemplateQueryElement = initElement(
        SeaPenTemplateQueryElement,
        {templateId: SeaPenTemplateId.kFlower.toString()});
    await waitAfterNextRender(seaPenTemplateQueryElement);

    const chips =
        seaPenTemplateQueryElement.shadowRoot!.querySelectorAll('.chip-text');
    const chipToSelect = chips[1] as HTMLElement;

    chipToSelect!.click();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    const seaPenOptionsElement =
        seaPenTemplateQueryElement.shadowRoot!.querySelector(
            SeaPenOptionsElement.is);
    assertTrue(
        !!seaPenOptionsElement,
        'the options chips should show after clicking a chip');
    const options = seaPenOptionsElement.shadowRoot!.querySelectorAll(
        '#container cr-button');
    assertTrue(
        options.length > 0, 'there should be options available to select');
    const selectedOption =
        seaPenOptionsElement.shadowRoot!.querySelector<CrButtonElement>(
            '#container cr-button[aria-selected]');
    assertTrue(
        !!selectedOption!.querySelector('img'),
        'the selected option should contain a preview image');
    assertEquals(
        chipToSelect.innerText, selectedOption!.innerText,
        'the selected chip should have an equivalent selected option');
    const selectedChip =
        seaPenTemplateQueryElement.shadowRoot!.querySelectorAll(
            '#template .selected .chip-text');
    assertEquals(
        1, selectedChip.length,
        'There should be exactly one chip that is selected.');
    assertEquals(selectedChip[0] as HTMLElement, chipToSelect);
  });

  test('selecting option updates chip', async () => {
    seaPenTemplateQueryElement = initElement(
        SeaPenTemplateQueryElement,
        {templateId: SeaPenTemplateId.kFlower.toString()});
    await waitAfterNextRender(seaPenTemplateQueryElement);
    const chips =
        seaPenTemplateQueryElement.shadowRoot!.querySelectorAll('.chip-text');
    const chip = chips[0] as HTMLElement;
    chip!.click();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    const seaPenOptionsElement =
        seaPenTemplateQueryElement.shadowRoot!.querySelector(
            SeaPenOptionsElement.is);
    assertTrue(
        !!seaPenOptionsElement,
        'the options chips should show after clicking a chip');
    const optionToSelect =
        seaPenOptionsElement.shadowRoot!.querySelector<CrButtonElement>(
            '#container cr-button:not([aria-selected])');
    const optionText = optionToSelect!.innerText;
    assertTrue(
        optionText !== chip.innerText,
        'unselected option should not match text');

    optionToSelect!.click();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    let selectedOption =
        seaPenOptionsElement.shadowRoot!.querySelector<CrButtonElement>(
            '#container cr-button[aria-selected]');
    assertEquals(
        selectedOption!.innerText, optionText,
        'the new option should now be selected');

    const selectedChip =
        seaPenTemplateQueryElement.shadowRoot!.querySelector<CrButtonElement>(
            '#template .selected');
    assertEquals(
        selectedChip!.innerText, optionText,
        'the chip should update to match the new selected option');

    chip!.click();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    selectedOption =
        seaPenOptionsElement.shadowRoot!.querySelector<CrButtonElement>(
            '#container cr-button[aria-selected]');
    assertTrue(!selectedOption, 'Clicking the chip again will hide options.');
  });

  test('inspires me', async () => {
    seaPenTemplateQueryElement = initElement(
        SeaPenTemplateQueryElement,
        {templateId: SeaPenTemplateId.kFlower.toString()});
    initElement(SeaPenRouterElement, {basePath: '/base'});
    await waitAfterNextRender(seaPenTemplateQueryElement);
    const inspireButton =
        seaPenTemplateQueryElement.shadowRoot!.getElementById('inspire');
    assertTrue(!!inspireButton);
    inspireButton!.click();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    const chips =
        seaPenTemplateQueryElement.shadowRoot!.querySelectorAll<HTMLElement>(
            '.chip-text');
    assertTrue(chips.length >= 2, 'there should be chips to select');
    chips[0]!.click();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    const seaPenOptionsElement =
        seaPenTemplateQueryElement.shadowRoot!.querySelector(
            SeaPenOptionsElement.is);
    assertTrue(
        !!seaPenOptionsElement,
        'the options chips should show after clicking a chip');
    let selectedOption =
        seaPenOptionsElement.shadowRoot!.querySelector<CrButtonElement>(
            '#container cr-button[aria-selected]');
    let optionText = selectedOption!.innerText;
    assertTrue(
        optionText === chips[0]!.innerText,
        'selected option should match text');

    chips[1]!.click();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    selectedOption =
        seaPenOptionsElement.shadowRoot!.querySelector<CrButtonElement>(
            '#container cr-button[aria-selected]');
    optionText = selectedOption!.innerText;
    assertTrue(
        optionText === chips[1]!.innerText,
        'selected option should match text');
  });

  test('inspire click clears selected chip', async () => {
    seaPenTemplateQueryElement = initElement(
        SeaPenTemplateQueryElement,
        {templateId: SeaPenTemplateId.kFlower.toString()});
    initElement(SeaPenRouterElement, {basePath: '/base'});
    await waitAfterNextRender(seaPenTemplateQueryElement);
    const chips =
        seaPenTemplateQueryElement.shadowRoot!.querySelectorAll<HTMLElement>(
            '.chip-text');
    const inspireButton =
        seaPenTemplateQueryElement.shadowRoot!.getElementById('inspire');

    // Select a chip.
    chips[0]!.click();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    let unselected =
        seaPenTemplateQueryElement.shadowRoot!.querySelectorAll<HTMLElement>(
            '.unselected');
    assertTrue(
        unselected.length > 0, 'template should have unselected elements');

    // Click inspire button.
    inspireButton!.click();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    unselected =
        seaPenTemplateQueryElement.shadowRoot!.querySelectorAll<HTMLElement>(
            '.unselected');
    assertEquals(
        0, unselected.length, 'template should be in the default state');
  });

  test('clicking inspire button triggers search', async () => {
    seaPenTemplateQueryElement = initElement(
        SeaPenTemplateQueryElement,
        {templateId: SeaPenTemplateId.kFlower.toString()});
    initElement(SeaPenRouterElement, {basePath: '/base'});
    await waitAfterNextRender(seaPenTemplateQueryElement);

    const inspireButton =
        seaPenTemplateQueryElement.shadowRoot!.getElementById('inspire');
    assertTrue(!!inspireButton);
    inspireButton!.click();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    const query: SeaPenQuery =
        await seaPenProvider.whenCalled('searchWallpaper');
    assertEquals(
        query.templateQuery!.id, SeaPenTemplateId.kFlower,
        'Query template id should match');
  });

  test('shows loading text when thumbnails loading', async () => {
    seaPenTemplateQueryElement = initElement(
        SeaPenTemplateQueryElement,
        {templateId: SeaPenTemplateId.kFlower.toString()});
    await waitAfterNextRender(seaPenTemplateQueryElement);

    assertFalse(!!getThumbnailsLoadingText(), 'no thumbnails loading text');
    assertEquals(
        2, getSearchButtons().length, 'inspire me and create buttons exist');
    assertTrue(getSearchButtons().every(isVisible), 'buttons are visible');

    // Simulate loading start.
    personalizationStore.data.wallpaper.seaPen = {
        ...personalizationStore.data.wallpaper.seaPen};
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = true;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    assertTrue(!!getThumbnailsLoadingText(), 'thumbnails loading text exists');
    assertTrue(
        isVisible(getThumbnailsLoadingText()),
        'thumbnails loading text is visible');
    assertEquals(
        2, getSearchButtons().length,
        'inspire me and create buttons still exist');
    assertTrue(
        getSearchButtons().every(button => !isVisible(button)),
        'buttons are hidden');

    // Simulate loading end.
    personalizationStore.data.wallpaper.seaPen = {
        ...personalizationStore.data.wallpaper.seaPen};
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = false;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    assertTrue(
        !!getThumbnailsLoadingText(), 'thumbnails loading text still exists');
    assertFalse(
        isVisible(getThumbnailsLoadingText()),
        'thumbnails loading text is not visible');
    assertEquals(
        2, getSearchButtons().length, 'inspire me and create buttons exist');
    assertTrue(
        getSearchButtons().every(isVisible), 'buttons are visible again');
  });
});
