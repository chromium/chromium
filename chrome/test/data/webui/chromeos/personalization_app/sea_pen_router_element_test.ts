// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {SeaPenInputQueryElement, SeaPenPaths, SeaPenRecentWallpapersElement, SeaPenRouterElement, SeaPenTemplateQueryElement, SeaPenTemplatesElement} from 'chrome://personalization/js/personalization_app.js';
import {SeaPenQuery, SeaPenTemplateId} from 'chrome://resources/ash/common/sea_pen/sea_pen.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestSeaPenProvider} from './test_sea_pen_interface_provider.js';

suite('SeaPenRouterElementTest', function() {
  let personalizationStore: TestPersonalizationStore;
  let seaPenProvider: TestSeaPenProvider;

  setup(() => {
    loadTimeData.overrideValues(
        {isSeaPenEnabled: true, isSeaPenTextInputEnabled: false});
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    seaPenProvider = mocks.seaPenProvider;
  });

  test('shows templates and recent elements', async () => {
    const router = initElement(SeaPenRouterElement, {basePath: '/base'});
    router.goToRoute(SeaPenPaths.ROOT);
    await waitAfterNextRender(router);

    assertTrue(
        !!router.shadowRoot!.querySelector(SeaPenTemplatesElement.is),
        'sea-pen-templates shown on root');
    assertTrue(
        !!router.shadowRoot!.querySelector(SeaPenRecentWallpapersElement.is),
        'sea-pen-recent-wallpapers shown on root');

    assertFalse(
        !!router.shadowRoot!.querySelector(SeaPenInputQueryElement.is),
        'no input query element on root');
    assertFalse(
        !!router.shadowRoot!.querySelector(SeaPenTemplateQueryElement.is),
        'no template query element on root');

    router.selectSeaPenTemplate(0 as SeaPenTemplateId);
    await waitAfterNextRender(router);

    assertTrue(
        !!router.shadowRoot!.querySelector(SeaPenTemplateQueryElement.is),
        'template query element is shown after selecting template');
  });

  test(
      'shows input query element if text input enabled and free form template is selected',
      async () => {
        loadTimeData.overrideValues({isSeaPenTextInputEnabled: true});
        const router = initElement(SeaPenRouterElement, {
          basePath: '/base',
        });
        router.goToRoute(SeaPenPaths.ROOT, {seaPenTemplateId: 'Query'});
        await waitAfterNextRender(router);

        assertTrue(
            !!router.shadowRoot!.querySelector(SeaPenInputQueryElement.is),
            'input query element shown on root');
      });

  test('remove thumbnail images when templateId changes', async () => {
    personalizationStore.setReducersEnabled(true);
    const router = initElement(SeaPenRouterElement, {basePath: '/base'});

    // Navigate to a template with thumbnails.
    router.goToRoute(
        SeaPenPaths.RESULTS,
        {seaPenTemplateId: SeaPenTemplateId.kFlower.toString()});
    await waitAfterNextRender(router);
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = false;
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.images;
    personalizationStore.notifyObservers();

    assertEquals(
        4, personalizationStore.data.wallpaper.seaPen.thumbnails.length,
        'thumbnails exist');

    // Update the template id, such as when switching templates via the
    // breadcrumb dropdown.
    router.goToRoute(
        SeaPenPaths.RESULTS,
        {seaPenTemplateId: SeaPenTemplateId.kTranslucent.toString()});
    await flushTasks();

    assertEquals(
        null, personalizationStore.data.wallpaper.seaPen.thumbnails,
        'thumbnails removed after routing');
  });

  test('update template query when templateId changes', async () => {
    const router = initElement(SeaPenRouterElement, {basePath: '/base'});
    const initialTemplate = SeaPenTemplateId.kMineral;
    const finalTemplate = SeaPenTemplateId.kFlower;
    router.goToRoute(
        SeaPenPaths.RESULTS, {seaPenTemplateId: initialTemplate.toString()});
    await waitAfterNextRender(router);
    const seaPenTemplateQueryElement =
        router.shadowRoot!.querySelector('sea-pen-template-query')!;
    const inspireButton =
        seaPenTemplateQueryElement.shadowRoot!.getElementById('inspire');

    inspireButton!.click();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    // Clicking the inspire button should match the rendered template.
    const initialQuery: SeaPenQuery =
        await seaPenProvider.whenCalled('searchWallpaper');
    assertEquals(
        initialQuery.templateQuery!.id, initialTemplate,
        'Initial query template id should match');
    seaPenProvider.reset();

    // Navigate to a new template.
    router.goToRoute(
        SeaPenPaths.RESULTS, {seaPenTemplateId: finalTemplate.toString()});
    await waitAfterNextRender(router);

    // Clicking the inspire button should match the new rendered template.
    inspireButton!.click();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    // After switching templates, we should match the new template.
    const finalQuery: SeaPenQuery =
        await seaPenProvider.whenCalled('searchWallpaper');
    assertEquals(
        finalQuery.templateQuery!.id, finalTemplate,
        'Final query template id should match');
  });

  test('navigates back to root if unknown path', async () => {
    const router = initElement(SeaPenRouterElement, {basePath: '/base'});
    router.goToRoute(SeaPenPaths.RESULTS);
    await waitAfterNextRender(router);

    assertEquals(
        '/base/results',
        router.shadowRoot?.querySelector('iron-location')?.path,
        'expected path is set');

    router.goToRoute('/unknown' as SeaPenPaths);
    await waitAfterNextRender(router);

    assertEquals(
        '/base', router.shadowRoot?.querySelector('iron-location')?.path,
        'path set back to root');
  });
});
