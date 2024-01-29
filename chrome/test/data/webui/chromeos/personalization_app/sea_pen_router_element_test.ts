// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {SeaPenInputQueryElement, SeaPenPaths, SeaPenRecentWallpapersElement, SeaPenRouterElement, SeaPenTemplateQueryElement, SeaPenTemplatesElement, SeaPenTermsOfServiceDialogElement} from 'chrome://personalization/js/personalization_app.js';
import {SeaPenQuery} from 'chrome://resources/ash/common/sea_pen/sea_pen.mojom-webui.js';
import {SeaPenTemplateId} from 'chrome://resources/ash/common/sea_pen/sea_pen_generated.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestSeaPenProvider} from './test_sea_pen_interface_provider.js';

suite('SeaPenRouterElementTest', function() {
  let personalizationStore: TestPersonalizationStore;
  let seaPenProvider: TestSeaPenProvider;
  let routerElement: SeaPenRouterElement|null = null;

  async function clickSeaPenTermsDialogButton(id: string) {
    const termsDialog = routerElement!.shadowRoot!
                            .querySelector<SeaPenTermsOfServiceDialogElement>(
                                SeaPenTermsOfServiceDialogElement.is);
    assertTrue(!!termsDialog, 'dialog element must exist to click button');
    const button = termsDialog!.shadowRoot!.getElementById(id);
    assertTrue(!!button, `button with id ${id} must exist`);
    button!.click();
    await waitAfterNextRender(routerElement!);
  }

  setup(() => {
    loadTimeData.overrideValues(
        {isSeaPenEnabled: true, isSeaPenTextInputEnabled: false});
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    seaPenProvider = mocks.seaPenProvider;
  });

  teardown(async () => {
    await teardownElement(routerElement);
    routerElement = null;
  });

  test('shows templates and recent elements', async () => {
    routerElement = initElement(SeaPenRouterElement, {basePath: '/base'});
    routerElement.goToRoute(SeaPenPaths.ROOT);
    await waitAfterNextRender(routerElement);

    assertTrue(
        !!routerElement.shadowRoot!.querySelector(SeaPenTemplatesElement.is),
        'sea-pen-templates shown on root');
    assertTrue(
        !!routerElement.shadowRoot!.querySelector(
            SeaPenRecentWallpapersElement.is),
        'sea-pen-recent-wallpapers shown on root');

    assertFalse(
        !!routerElement.shadowRoot!.querySelector(SeaPenInputQueryElement.is),
        'no input query element on root');
    assertFalse(
        !!routerElement.shadowRoot!.querySelector(
            SeaPenTemplateQueryElement.is),
        'no template query element on root');

    routerElement.selectSeaPenTemplate(0 as SeaPenTemplateId);
    await waitAfterNextRender(routerElement);

    assertTrue(
        !!routerElement.shadowRoot!.querySelector(
            SeaPenTemplateQueryElement.is),
        'template query element is shown after selecting template');
  });

  test(
      'shows input query element if text input enabled and free form template is selected',
      async () => {
        loadTimeData.overrideValues({isSeaPenTextInputEnabled: true});
        routerElement = initElement(SeaPenRouterElement, {
          basePath: '/base',
        });
        routerElement.goToRoute(SeaPenPaths.ROOT, {seaPenTemplateId: 'Query'});
        await waitAfterNextRender(routerElement);

        assertTrue(
            !!routerElement.shadowRoot!.querySelector(
                SeaPenInputQueryElement.is),
            'input query element shown on root');
      });

  test('remove thumbnail images when templateId changes', async () => {
    personalizationStore.setReducersEnabled(true);
    routerElement = initElement(SeaPenRouterElement, {basePath: '/base'});

    // Navigate to a template with thumbnails.
    routerElement.goToRoute(
        SeaPenPaths.RESULTS,
        {seaPenTemplateId: SeaPenTemplateId.kFlower.toString()});
    await waitAfterNextRender(routerElement);
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = false;
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.images;
    personalizationStore.notifyObservers();

    assertEquals(
        4, personalizationStore.data.wallpaper.seaPen.thumbnails.length,
        'thumbnails exist');

    // Update the template id, such as when switching templates via the
    // breadcrumb dropdown.
    routerElement.goToRoute(
        SeaPenPaths.RESULTS,
        {seaPenTemplateId: SeaPenTemplateId.kTranslucent.toString()});
    await flushTasks();

    assertEquals(
        null, personalizationStore.data.wallpaper.seaPen.thumbnails,
        'thumbnails removed after routing');
  });

  test('update template query when templateId changes', async () => {
    routerElement = initElement(SeaPenRouterElement, {basePath: '/base'});
    const initialTemplate = SeaPenTemplateId.kMineral;
    const finalTemplate = SeaPenTemplateId.kFlower;
    routerElement.goToRoute(
        SeaPenPaths.RESULTS, {seaPenTemplateId: initialTemplate.toString()});
    await waitAfterNextRender(routerElement);
    const seaPenTemplateQueryElement =
        routerElement.shadowRoot!.querySelector('sea-pen-template-query')!;
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
    routerElement.goToRoute(
        SeaPenPaths.RESULTS, {seaPenTemplateId: finalTemplate.toString()});
    await waitAfterNextRender(routerElement);

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
    routerElement = initElement(SeaPenRouterElement, {basePath: '/base'});
    routerElement.goToRoute(SeaPenPaths.RESULTS);
    await waitAfterNextRender(routerElement);

    assertEquals(
        '/base/results',
        routerElement.shadowRoot?.querySelector('iron-location')?.path,
        'expected path is set');

    routerElement.goToRoute('/unknown' as SeaPenPaths);
    await waitAfterNextRender(routerElement);

    assertEquals(
        '/base', routerElement.shadowRoot?.querySelector('iron-location')?.path,
        'path set back to root');
  });


  test('shows SeaPen terms dialog', async () => {
    personalizationStore.setReducersEnabled(true);
    routerElement = initElement(SeaPenRouterElement, {
      basePath: '/base',
    });

    await seaPenProvider.whenCalled('shouldShowSeaPenTermsOfServiceDialog');
    await waitAfterNextRender(routerElement);

    const seaPenTermsDialog = routerElement.shadowRoot!.querySelector(
        SeaPenTermsOfServiceDialogElement.is);
    assertTrue(
        !!seaPenTermsDialog, 'SeaPen terms of service dialog is displayed');
  });

  test(
      'accepts the SeaPen wallpaper terms and closes the terms dialog',
      async () => {
        personalizationStore.setReducersEnabled(true);
        routerElement = initElement(SeaPenRouterElement, {
          basePath: '/base',
        });

        await seaPenProvider.whenCalled('shouldShowSeaPenTermsOfServiceDialog');
        await waitAfterNextRender(routerElement);

        let seaPenTermsDialog = routerElement.shadowRoot!.querySelector(
            SeaPenTermsOfServiceDialogElement.is);
        assertTrue(
            !!seaPenTermsDialog, 'SeaPen terms of service dialog is displayed');

        await clickSeaPenTermsDialogButton('accept');

        seaPenTermsDialog = routerElement.shadowRoot!.querySelector(
            SeaPenTermsOfServiceDialogElement.is);
        assertFalse(
            !!seaPenTermsDialog, 'Sea Pen wallpaper terms dialog is closed');

        assertEquals(
            '/base',
            routerElement.shadowRoot?.querySelector('iron-location')?.path,
            'path remains the same');
      });
});
