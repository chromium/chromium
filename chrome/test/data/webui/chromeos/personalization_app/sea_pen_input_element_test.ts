// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {emptyState, PersonalizationRouterElement, SeaPenInputElement, WallpaperActionName} from 'chrome://personalization/js/personalization_app.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

suite('SeaPenInputElementTest', function() {
  let personalizationStore: TestPersonalizationStore;
  let seaPenInputElement: SeaPenInputElement|null;

  setup(() => {
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
  });

  teardown(async () => {
    await teardownElement(seaPenInputElement);
    seaPenInputElement = null;
  });

  test('displays input query tab', async () => {
    // Initialize |seaPenInputElement|.
    seaPenInputElement = initElement(SeaPenInputElement);
    await waitAfterNextRender(seaPenInputElement);

    const seaPenInputQueryElement =
        seaPenInputElement.shadowRoot!.querySelector('sea-pen-input-query');
    assertTrue(!!seaPenInputQueryElement, 'input query should display.');

    const templateQuery =
        seaPenInputElement.shadowRoot!.querySelector('sea-pen-template-query');
    assertFalse(!!templateQuery, 'template query should not display.');

    // Verify that the text input area and search button are displayed.
    const inputQuery =
        seaPenInputQueryElement!.shadowRoot!.querySelector('cr-input');
    assertTrue(!!inputQuery, 'input query should display.');

    const searchButton = seaPenInputQueryElement!.shadowRoot!.querySelector(
                             '#searchButton') as HTMLElement;
    assertTrue(!!searchButton, 'search button should display.');

    // Mock singleton |PersonalizationRouter|.
    const router = TestMock.fromClass(PersonalizationRouterElement);
    PersonalizationRouterElement.instance = () => router;

    // Mock |PersonalizationRouter.selectSeaPenTemplate()|.
    let selectedTemplateId: string|undefined;
    router.selectSeaPenTemplate = (templateId: string) => {
      selectedTemplateId = templateId;
    };

    // Make sure state starts at expected value.
    assertDeepEquals(emptyState(), personalizationStore.data);
    // Actually run the reducers.
    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(WallpaperActionName.SET_IMAGE_THUMBNAILS);

    // Update input query and click on search button.
    inputQuery.value = 'this is a test query';
    searchButton.click();

    assertEquals(selectedTemplateId, 'query');

    await personalizationStore.waitForAction(
        WallpaperActionName.SET_IMAGE_THUMBNAILS);

    assertDeepEquals(
        {
          query: 'this is a test query',
          thumbnailsLoading: false,
          thumbnails: [
            {
              id: BigInt(1),
              url: {url: 'chrome://personalization/images/feel_the_breeze.png'},
            },
            {
              id: BigInt(2),
              url: {url: 'chrome://personalization/images/float_on_by.png'},
            },
            {
              id: BigInt(3),
              url: {url: 'chrome://personalization/images/slideshow.png'},
            },
            {
              id: BigInt(4),
              url: {url: 'chrome://personalization/images/feel_the_breeze.png'},
            },
          ],
        },
        personalizationStore.data.wallpaper.seaPen,
        'expected SeaPen state is set',
    );
  });

  test('displays template query content', async () => {
    // Initialize |seaPenInputElement|.
    seaPenInputElement = initElement(SeaPenInputElement, {'templateId': '4'});
    await waitAfterNextRender(seaPenInputElement);

    const seaPenTemplateQueryElement =
        seaPenInputElement.shadowRoot!.querySelector('sea-pen-template-query');

    assertTrue(!!seaPenTemplateQueryElement, 'template query should display.');
  });
});
