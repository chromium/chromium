// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {PersonalizationRouterElement, SeaPenCollectionElement} from 'chrome://personalization/js/personalization_app.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {initElement, teardownElement} from './personalization_app_test_utils.js';

suite('SeaPenCollectionElementTest', function() {
  let seaPenCollectionElement: SeaPenCollectionElement|null;

  setup(() => {});

  teardown(async () => {
    await teardownElement(seaPenCollectionElement);
    seaPenCollectionElement = null;
  });

  test('displays templates', async () => {
    // Initialize |seaPenCollectionElement|.
    seaPenCollectionElement = initElement(SeaPenCollectionElement);
    await waitAfterNextRender(seaPenCollectionElement);

    const seaPenTemplatesElement =
        seaPenCollectionElement.shadowRoot!.querySelector('sea-pen-templates');
    assertTrue(!!seaPenTemplatesElement, 'Templates should display.');

    const templateElements =
        seaPenTemplatesElement!.shadowRoot!.querySelectorAll(
            'wallpaper-grid-item');
    assertTrue(!!templateElements, 'At least one template is shown.');

    // select a template.
    const template = (templateElements![0] as HTMLElement);
    assertFalse(!!template.ariaSelected);

    // Mock singleton |PersonalizationRouter|.
    const router = TestMock.fromClass(PersonalizationRouterElement);
    PersonalizationRouterElement.instance = () => router;

    // Mock |PersonalizationRouter.selectSeaPenTemplate()|.
    let selectedTemplateId: string|undefined;
    router.selectSeaPenTemplate = (templateId: string) => {
      selectedTemplateId = templateId;
    };

    template.click();

    assertEquals(selectedTemplateId, 'ChromeOSWallpaperTemplateSamplePark');
    assertEquals(null, template.ariaSelected);
  });

  test('displays template content', async () => {
    // Initialize |seaPenCollectionElement|.
    seaPenCollectionElement = initElement(
        SeaPenCollectionElement,
        {'templateId': 'ChromeOSWallpaperTemplateSamplePark'});
    await waitAfterNextRender(seaPenCollectionElement);

    const seaPenImagesElement =
        seaPenCollectionElement.shadowRoot!.querySelector('sea-pen-images');

    assertTrue(!!seaPenImagesElement, 'Template content should display.');
  });
});
