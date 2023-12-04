// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {Paths, SeaPenTemplateId, SeaPenTemplateQueryElement} from 'chrome://personalization/js/personalization_app.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {initElement, teardownElement} from './personalization_app_test_utils.js';

suite('SeaPenTemplateQueryElementTest', function() {
  let seaPenTemplateQueryElement: SeaPenTemplateQueryElement|null;

  teardown(async () => {
    await teardownElement(seaPenTemplateQueryElement);
    seaPenTemplateQueryElement = null;
  });

  test('displays sea pen template', async () => {
    seaPenTemplateQueryElement = initElement(SeaPenTemplateQueryElement, {
      'templateId': SeaPenTemplateId.kFlower.toString(),
      'path': Paths.SEA_PEN_COLLECTION,
    });
    await waitAfterNextRender(seaPenTemplateQueryElement);

    const chips =
        seaPenTemplateQueryElement.shadowRoot!.querySelectorAll('.clickable');
    const options = seaPenTemplateQueryElement.shadowRoot!.querySelectorAll(
        '#options cr-button');
    const searchButtons =
        seaPenTemplateQueryElement.shadowRoot!.querySelectorAll(
            '#searchButtons cr-button');
    const unselectedTemplate =
        seaPenTemplateQueryElement.shadowRoot!.querySelectorAll(
            '#template .unselected');
    const searchButton = seaPenTemplateQueryElement.shadowRoot!.querySelector(
                             '#searchButton') as HTMLElement;

    assertTrue(chips.length > 0, 'there should be chips to select');
    assertEquals(
        0, options.length, 'there should be no options available to select');
    assertEquals(2, searchButtons.length, 'there should be two search buttons');
    assertEquals(
        0, unselectedTemplate.length,
        'there should be no unselected templates');
    assertEquals('Search', searchButton!.innerText);
  });

  test('displays search again button on results page', async () => {
    seaPenTemplateQueryElement = initElement(SeaPenTemplateQueryElement, {
      'templateId': SeaPenTemplateId.kFlower.toString(),
      'path': Paths.SEA_PEN_RESULTS,
    });
    await waitAfterNextRender(seaPenTemplateQueryElement);

    const searchButton = seaPenTemplateQueryElement.shadowRoot!.querySelector(
                             '#searchButton') as HTMLElement;

    assertEquals('Search again', searchButton!.innerText);
  });

  test('selects chip', async () => {
    seaPenTemplateQueryElement = initElement(
        SeaPenTemplateQueryElement,
        {'templateId': SeaPenTemplateId.kFlower.toString()});
    await waitAfterNextRender(seaPenTemplateQueryElement);

    const chips =
        seaPenTemplateQueryElement.shadowRoot!.querySelectorAll('.clickable');
    const chipToSelect = chips[0] as HTMLElement;

    chipToSelect!.click();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    const options = seaPenTemplateQueryElement.shadowRoot!.querySelectorAll(
        '#options cr-button');
    assertTrue(
        options.length > 0, 'there should be options available to select');
    const selectedOption =
        seaPenTemplateQueryElement.shadowRoot!.querySelector(
            '#options cr-button.action-button') as HTMLElement;
    assertEquals(
        chipToSelect.innerText, selectedOption!.innerText,
        'the selected chip should have an equivalent selected option');
    const selectedTemplate =
        seaPenTemplateQueryElement.shadowRoot!.querySelectorAll(
            '#template .selected');
    assertEquals(
        1, selectedTemplate.length,
        'There should be exactly one template div that is selected.');
    assertEquals(selectedTemplate[0] as HTMLElement, chipToSelect);
  });

  test('selecting option updates chip', async () => {
    seaPenTemplateQueryElement = initElement(
        SeaPenTemplateQueryElement,
        {'templateId': SeaPenTemplateId.kFlower.toString()});
    await waitAfterNextRender(seaPenTemplateQueryElement);
    const chips =
        seaPenTemplateQueryElement.shadowRoot!.querySelectorAll('.clickable');
    const chip = chips[0] as HTMLElement;
    chip!.click();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    const optionToSelect =
        seaPenTemplateQueryElement.shadowRoot!.querySelector(
            '#options cr-button.unselected-option') as HTMLElement;
    const optionText = optionToSelect!.innerText;
    assertTrue(
        optionText !== chip.innerText,
        'unselected option should not match text');

    optionToSelect!.click();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    const selectedChip = seaPenTemplateQueryElement.shadowRoot!.querySelector(
                             '#template .selected') as HTMLElement;
    assertEquals(
        selectedChip!.innerText, optionText,
        'the chip should update to match the new selected option');
    const selectedOption =
        seaPenTemplateQueryElement.shadowRoot!.querySelector(
            '#options cr-button.action-button') as HTMLElement;
    assertEquals(
        selectedOption!.innerText, optionText,
        'the option should now be selected');
  });

  test('inspires me', async () => {
    seaPenTemplateQueryElement = initElement(
        SeaPenTemplateQueryElement,
        {'templateId': SeaPenTemplateId.kFlower.toString()});
    await waitAfterNextRender(seaPenTemplateQueryElement);
    const inspireButton =
        seaPenTemplateQueryElement.shadowRoot!.getElementById('inspire');
    assertTrue(!!inspireButton);
    inspireButton!.click();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    const chips =
        seaPenTemplateQueryElement.shadowRoot!.querySelectorAll<HTMLElement>(
            '.clickable');
    assertTrue(chips.length >= 2, 'there should be chips to select');
    chips[0]!.click();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    let selectedOption = seaPenTemplateQueryElement.shadowRoot!.querySelector(
                             '#options cr-button.action-button') as HTMLElement;
    let optionText = selectedOption!.innerText;
    assertTrue(
        optionText === chips[0]!.innerText,
        'selected option should match text');

    chips[1]!.click();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    selectedOption = seaPenTemplateQueryElement.shadowRoot!.querySelector(
                         '#options cr-button.action-button') as HTMLElement;
    optionText = selectedOption!.innerText;
    assertTrue(
        optionText === chips[1]!.innerText,
        'selected option should match text');
  });
});
