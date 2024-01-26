// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {SeaPenPaths, SeaPenRouterElement, SeaPenTemplateQueryElement} from 'chrome://personalization/js/personalization_app.js';
import {SeaPenQuery, SeaPenTemplateId} from 'chrome://resources/ash/common/sea_pen/sea_pen.mojom-webui.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
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
    const searchButton = seaPenTemplateQueryElement.shadowRoot!.querySelector(
                             '#searchButton') as HTMLElement;

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
    seaPenTemplateQueryElement = initElement(SeaPenTemplateQueryElement, {
      path: SeaPenPaths.RESULTS,
      templateId: SeaPenTemplateId.kFlower.toString(),
    });
    await waitAfterNextRender(seaPenTemplateQueryElement);

    const searchButton = seaPenTemplateQueryElement.shadowRoot!.querySelector(
                             '#searchButton') as HTMLElement;

    assertEquals(
        seaPenTemplateQueryElement.i18n('seaPenRecreateButton'),
        searchButton!.innerText);
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

    assertTrue(
        !!seaPenTemplateQueryElement.shadowRoot!
              .querySelector('cr-action-menu')
              ?.open,
        'the action menu should open after clicking a chip');
    const options = seaPenTemplateQueryElement.shadowRoot!.querySelectorAll(
        'button.dropdown-item:not([hidden])');
    assertTrue(
        options.length > 0, 'there should be options available to select');
    const selectedOption = seaPenTemplateQueryElement.shadowRoot!.querySelector(
                               'button[aria-selected=\'true\']') as HTMLElement;
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

    const optionToSelect =
        seaPenTemplateQueryElement.shadowRoot!.querySelector(
            'button[aria-selected=\'false\']') as HTMLElement;
    const optionText = optionToSelect!.innerText;
    assertTrue(
        optionText !== chip.innerText,
        'unselected option should not match text');

    optionToSelect!.click();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    assertFalse(
        !!seaPenTemplateQueryElement.shadowRoot!
              .querySelector('cr-action-menu')
              ?.open,
        'the action menu should close after clicking an option');

    const selectedChip = seaPenTemplateQueryElement.shadowRoot!.querySelector(
                             '#template .selected') as HTMLElement;
    assertEquals(
        selectedChip!.innerText, optionText,
        'the chip should update to match the new selected option');

    chip!.click();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    const selectedOption = seaPenTemplateQueryElement.shadowRoot!.querySelector(
                               'button[aria-selected=\'true\']') as HTMLElement;
    assertEquals(
        selectedOption!.innerText, optionText,
        'the option should now be selected');
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

    let selectedOption = seaPenTemplateQueryElement.shadowRoot!.querySelector(
                             'button[aria-selected=\'true\']') as HTMLElement;
    let optionText = selectedOption!.innerText;
    assertTrue(
        optionText === chips[0]!.innerText,
        'selected option should match text');

    chips[1]!.click();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    selectedOption = seaPenTemplateQueryElement.shadowRoot!.querySelector(
                         'button[aria-selected=\'true\']') as HTMLElement;
    optionText = selectedOption!.innerText;
    assertTrue(
        optionText === chips[1]!.innerText,
        'selected option should match text');
  });

  test('clicking inspire button triggers search', async () => {
    seaPenTemplateQueryElement = initElement(
        SeaPenTemplateQueryElement,
        {templateId: SeaPenTemplateId.kMineral.toString()});
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
        query.templateQuery!.id, SeaPenTemplateId.kMineral,
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
