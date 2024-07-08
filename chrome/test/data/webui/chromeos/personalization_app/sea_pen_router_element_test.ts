// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {SeaPenFreeformElement, SeaPenImagesElement, SeaPenInputQueryElement, SeaPenIntroductionDialogElement, SeaPenOptionsElement, SeaPenPaths, SeaPenRecentWallpapersElement, SeaPenRouterElement, SeaPenTemplateQueryElement, SeaPenTemplatesElement, SeaPenZeroStateSvgElement, setTransitionsEnabled, WallpaperGridItemElement} from 'chrome://personalization/js/personalization_app.js';
import {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {SeaPenQuery} from 'chrome://resources/ash/common/sea_pen/sea_pen.mojom-webui.js';
import {SeaPenTemplateId} from 'chrome://resources/ash/common/sea_pen/sea_pen_generated.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestSeaPenProvider} from './test_sea_pen_interface_provider.js';

suite('SeaPenRouterElementTest', function() {
  let personalizationStore: TestPersonalizationStore;
  let seaPenProvider: TestSeaPenProvider;
  let routerElement: SeaPenRouterElement|null = null;

  async function clickSeaPenIntroDialogCloseButton() {
    const introDialog = routerElement!.shadowRoot!
                            .querySelector<SeaPenIntroductionDialogElement>(
                                SeaPenIntroductionDialogElement.is);
    assertTrue(!!introDialog, 'dialog element must exist to click button');
    const button = introDialog!.shadowRoot!.getElementById('close');
    assertTrue(!!button, `close button must exist`);
    button!.click();
    await waitAfterNextRender(routerElement!);
  }

  setup(() => {
    loadTimeData.overrideValues(
        {isSeaPenEnabled: true, isSeaPenTextInputEnabled: false});
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    seaPenProvider = mocks.seaPenProvider;

    // Disables page transition by default.
    setTransitionsEnabled(false);
  });

  teardown(async () => {
    await teardownElement(routerElement);
    routerElement = null;
  });

  test('shows templates and recent elements', async () => {
    routerElement = initElement(SeaPenRouterElement, {basePath: '/base'});
    routerElement.goToRoute(SeaPenPaths.TEMPLATES);
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
      'shows freeform page with input query, sample prompts, recent images and images elements',
      async () => {
        loadTimeData.overrideValues({isSeaPenTextInputEnabled: true});
        routerElement = initElement(SeaPenRouterElement, {
          basePath: '/base',
        });
        routerElement.goToRoute(SeaPenPaths.FREEFORM);
        await waitAfterNextRender(routerElement);

        assertTrue(
            !!routerElement.shadowRoot!.querySelector(
                SeaPenInputQueryElement.is),
            'input query element shown on freeform page');

        assertTrue(
            !!routerElement.shadowRoot!.querySelector(SeaPenFreeformElement.is),
            'freeform element shown on freeform page');
      });

  test('creates freeform images switching to Results tab', async () => {
    personalizationStore.setReducersEnabled(true);
    loadTimeData.overrideValues({isSeaPenTextInputEnabled: true});
    routerElement = initElement(SeaPenRouterElement, {
      basePath: '/base',
    });
    routerElement.goToRoute(SeaPenPaths.FREEFORM);
    await waitAfterNextRender(routerElement);

    const seaPenInputQuery =
        routerElement.shadowRoot!.querySelector(SeaPenInputQueryElement.is);
    assertTrue(!!seaPenInputQuery, 'input query element exists');

    const inputElement =
        seaPenInputQuery.shadowRoot!.querySelector<CrInputElement>(
            '#queryInput');
    assertTrue(!!inputElement, 'input text box exists');

    // Set a freeform query.
    inputElement!.value = 'a cool castle';

    // Start freeform query search.
    seaPenInputQuery.shadowRoot!.getElementById('searchButton')!.click();
    await waitAfterNextRender(routerElement);

    assertTrue(
        window.location.href.endsWith(SeaPenPaths.FREEFORM),
        'remains in the same Freeform page');

    const seaPenFreeformElement =
        routerElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenFreeformElement.is);
    assertTrue(!!seaPenFreeformElement, 'freeform element exists');

    // Sea Pen images should be present and visible as the search activates
    // Results tab.
    const seaPenImagesElement =
        seaPenFreeformElement!.shadowRoot!.querySelector<HTMLElement>(
            SeaPenImagesElement.is);
    assertTrue(!!seaPenImagesElement, 'sea-pen-images is available');
    assertFalse(seaPenImagesElement.hidden, 'sea-pen-images is visible');

    // Recent images element is no longer available.
    assertFalse(
        !!seaPenFreeformElement!.shadowRoot!.querySelector(
            SeaPenRecentWallpapersElement.is),
        'sea-pen-recent-wallpapers is not shown in Results tab');
  });

  test(
      'shows zero state svg when a template is selected from root',
      async () => {
        routerElement = initElement(SeaPenRouterElement, {
          basePath: '/base',
        });
        routerElement.goToRoute(SeaPenPaths.TEMPLATES);
        await waitAfterNextRender(routerElement);

        const seaPenTemplatesElement =
            routerElement.shadowRoot!.querySelector(SeaPenTemplatesElement.is);
        assertTrue(!!seaPenTemplatesElement, 'sea-pen-templates shown on root');

        const templates = seaPenTemplatesElement.shadowRoot!.querySelectorAll<
            WallpaperGridItemElement>(
            `${WallpaperGridItemElement.is}[data-sea-pen-image]:not([hidden])`);
        assertEquals(8, templates.length, 'there are 8 templates');

        templates[2]!.click();
        await waitAfterNextRender(routerElement);

        // Navigates to result page and shows zero state svg element.
        assertEquals(
            '/base/results',
            routerElement.shadowRoot?.querySelector('iron-location')?.path,
            'navigates to result page');
        assertEquals(
            'seaPenTemplateId=4',
            routerElement.shadowRoot?.querySelector('iron-location')?.query,
            'query as selected template id');

        const seaPenImagesElement =
            routerElement.shadowRoot!.querySelector(SeaPenImagesElement.is);
        assertTrue(
            !!seaPenImagesElement, 'sea-pen-images shown on result page');

        assertTrue(
            !!seaPenImagesElement.shadowRoot!.querySelector(
                SeaPenZeroStateSvgElement.is),
            'zero state svg is shown after selecting a template from root');
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
        seaPenProvider.thumbnails;
    personalizationStore.notifyObservers();

    assertEquals(
        4, personalizationStore.data.wallpaper.seaPen.thumbnails.length,
        'thumbnails exist');

    // Update the template id, such as when switching templates via the
    // breadcrumb dropdown.
    routerElement.goToRoute(
        SeaPenPaths.RESULTS,
        {seaPenTemplateId: SeaPenTemplateId.kCharacters.toString()});
    await waitAfterNextRender(routerElement);

    assertEquals(
        null, personalizationStore.data.wallpaper.seaPen.thumbnails,
        'thumbnails removed after routing');

    const seaPenImagesElement =
        routerElement.shadowRoot!.querySelector(SeaPenImagesElement.is);
    assertTrue(!!seaPenImagesElement);

    assertTrue(
        !!seaPenImagesElement.shadowRoot!.querySelector(
            SeaPenZeroStateSvgElement.is),
        'zero state svg is shown after a template is switched');
  });

  test('update template query when templateId changes', async () => {
    routerElement = initElement(SeaPenRouterElement, {basePath: '/base'});
    const initialTemplate = SeaPenTemplateId.kCharacters;
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
        await seaPenProvider.whenCalled('getSeaPenThumbnails');
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
        await seaPenProvider.whenCalled('getSeaPenThumbnails');
    assertEquals(
        finalQuery.templateQuery!.id, finalTemplate,
        'Final query template id should match');
  });

  test('clicking on sea-pen-images hide options UI', async () => {
    routerElement = initElement(SeaPenRouterElement, {basePath: '/base'});
    const initialTemplate = SeaPenTemplateId.kCharacters;
    routerElement.goToRoute(
        SeaPenPaths.RESULTS, {seaPenTemplateId: initialTemplate.toString()});
    await waitAfterNextRender(routerElement);
    const seaPenTemplateQueryElement =
        routerElement.shadowRoot!.querySelector('sea-pen-template-query')!;
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

    const seaPenImages = routerElement.shadowRoot!.querySelector(
                             'sea-pen-images') as HTMLElement;
    assertTrue(!!seaPenImages);
    seaPenImages.click();
    await waitAfterNextRender(seaPenTemplateQueryElement);

    const selectedOption =
        seaPenOptionsElement.shadowRoot!.querySelector(
            '#options cr-button[aria-selected]') as HTMLElement;
    assertTrue(
        !selectedOption,
        'Clicking anywhere else on the router container will hide options.');
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


  test('shows SeaPen introduction dialog', async () => {
    personalizationStore.setReducersEnabled(true);
    routerElement = initElement(SeaPenRouterElement, {
      basePath: '/base',
    });

    await seaPenProvider.whenCalled('shouldShowSeaPenIntroductionDialog');
    await waitAfterNextRender(routerElement);

    const seaPenIntroDialog = routerElement.shadowRoot!.querySelector(
        SeaPenIntroductionDialogElement.is);
    assertTrue(!!seaPenIntroDialog, 'SeaPen introduction dialog is displayed');
  });

  test(
      'closes Sea Pen introduction dialog', async () => {
        personalizationStore.setReducersEnabled(true);
        routerElement = initElement(SeaPenRouterElement, {
          basePath: '/base',
        });

        await seaPenProvider.whenCalled('shouldShowSeaPenIntroductionDialog');
        await waitAfterNextRender(routerElement);

        let seaPenIntroDialog = routerElement.shadowRoot!.querySelector(
            SeaPenIntroductionDialogElement.is);
        assertTrue(
            !!seaPenIntroDialog, 'SeaPen introduction dialog is displayed');

        await clickSeaPenIntroDialogCloseButton();

        seaPenIntroDialog = routerElement.shadowRoot!.querySelector(
            SeaPenIntroductionDialogElement.is);
        assertFalse(
            !!seaPenIntroDialog, 'Sea Pen introduction dialog is closed');

        assertEquals(
            '/base',
            routerElement.shadowRoot?.querySelector('iron-location')?.path,
            'path remains the same');
      });

  test('supports transition animation', async () => {
    const routerElement = initElement(SeaPenRouterElement, {basePath: '/base'});
    setTransitionsEnabled(true);
    await waitAfterNextRender(routerElement);

    // Forces transition to execute.
    await routerElement.goToRoute(SeaPenPaths.RESULTS);
    await waitAfterNextRender(routerElement);

    const seaPenImages =
        routerElement.shadowRoot!.querySelector('sea-pen-images');
    assertTrue(!!seaPenImages, 'sea-pen-images now exists');
  });

  test(
      'show shadow overlay when sea pen options are being selected',
      async () => {
        const routerElement =
            initElement(SeaPenRouterElement, {basePath: '/base'});
        await waitAfterNextRender(routerElement);

        // Navigate to a template, zero state is first shown.
        routerElement.goToRoute(
            SeaPenPaths.RESULTS,
            {seaPenTemplateId: SeaPenTemplateId.kFlower.toString()});
        await waitAfterNextRender(routerElement);

        const seaPenTemplateQuery =
            routerElement.shadowRoot!.querySelector('sea-pen-template-query');
        assertTrue(
            !!seaPenTemplateQuery, 'sea-pen-template-query should exist');

        const seaPenImages =
            routerElement.shadowRoot!.querySelector('sea-pen-images');
        assertTrue(!!seaPenImages, 'sea-pen-images should exist');

        assertEquals(
            'auto', window.getComputedStyle(seaPenImages).pointerEvents,
            'sea-pen-images should have pointer-events');

        assertEquals(
            'none', window.getComputedStyle(seaPenImages, '::after')!.content,
            'sea-pen-images has no after style content ');

        // Click on a chip in sea-pen-template-query. This should disable
        // pointer events of sea-pen-images.
        const chips =
            seaPenTemplateQuery.shadowRoot!.querySelectorAll('.chip-text');
        const chip = chips[0] as HTMLElement;
        chip!.click();
        await waitAfterNextRender(routerElement);

        // an overlay shadow displays for sea-pen-images.
        assertEquals(
            '""', window.getComputedStyle(seaPenImages, '::after')!.content,
            'after style content should match');

        // Click on the prior selected chip again. Chip state should be cleared
        // and pointer events are enabled again for sea-pen-images.
        chip!.click();
        await waitAfterNextRender(routerElement);

        assertEquals(
            'auto', window.getComputedStyle(seaPenImages).pointerEvents,
            'sea-pen-images should have pointer-events again');

        // The overlay shadow is no longer shown.
        assertEquals(
            'none', window.getComputedStyle(seaPenImages, '::after')!.content,
            'sea-pen-images no longer has after style content');
      });
});
