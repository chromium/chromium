// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/app.js';

import type {AppElement} from 'chrome://customize-chrome-side-panel.top-chrome/app.js';
import {CustomizeChromeImpression} from 'chrome://customize-chrome-side-panel.top-chrome/common.js';
import type {BackgroundCollection, CustomizeChromePageRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote, CustomizeChromeSection} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {CustomizeToolbarClientCallbackRouter, CustomizeToolbarHandlerRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_toolbar.mojom-webui.js';
import type {CustomizeToolbarHandlerInterface} from 'chrome://customize-chrome-side-panel.top-chrome/customize_toolbar.mojom-webui.js';
import {CustomizeToolbarApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_toolbar/customize_toolbar_api_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertGE, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from './test_support.js';

suite('AppTest', () => {
  let customizeChromeApp: AppElement;
  let handler: TestMock<CustomizeChromePageHandlerRemote>;
  let callbackRouter: CustomizeChromePageRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        CustomizeChromePageHandlerRemote,
        (mock: CustomizeChromePageHandlerRemote) =>
            CustomizeChromeApiProxy.setInstance(
                mock, new CustomizeChromePageCallbackRouter()));
    handler.setResultFor('getBackgroundImages', new Promise(() => {}));
    handler.setResultFor('getBackgroundCollections', new Promise(() => {}));
    callbackRouter = CustomizeChromeApiProxy.getInstance()
                         .callbackRouter.$.bindNewPipeAndPassRemote();
    customizeChromeApp = document.createElement('customize-chrome-app');
    document.body.appendChild(customizeChromeApp);
  });

  suite('Metrics', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        'extensionsCardEnabled': true,
      });
    });

    test('rendering extensions card section sets metric', async () => {
      const metrics = fakeMetricsPrivate();
      window.dispatchEvent(new Event('load'));
      const eventPromise = eventToPromise(
          'detect-extensions-card-section-impression', customizeChromeApp);
      assertEquals(
          0, metrics.count('NewTabPage.CustomizeChromeSidePanelImpression'));
      assertEquals(
          0,
          metrics.count(
              'NewTabPage.CustomizeChromeSidePanelImpression',
              CustomizeChromeImpression.EXTENSIONS_CARD_SECTION_DISPLAYED));

      customizeChromeApp.shadowRoot!.querySelector('#extensions')!
          .scrollIntoView({'behavior': 'instant'});
      await eventPromise;

      assertEquals(
          1, metrics.count('NewTabPage.CustomizeChromeSidePanelImpression'));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.CustomizeChromeSidePanelImpression',
              CustomizeChromeImpression.EXTENSIONS_CARD_SECTION_DISPLAYED));
    });
  });

  test('app changes pages', async () => {
    const testCollection: BackgroundCollection = {
      id: 'test',
      label: 'test',
      previewImageUrl: {url: 'https://test.jpg'},
      imageVerified: false,
    };

    // Test initial page state.
    assertTrue(
        customizeChromeApp.$.overviewPage.classList.contains('selected'));
    assertEquals(document.body, document.activeElement);

    // Send event for edit theme being clicked.
    customizeChromeApp.$.appearanceElement.dispatchEvent(
        new Event('edit-theme-click'));
    await microtasksFinished();
    // Current page should now be categories.
    assertTrue(
        customizeChromeApp.$.categoriesPage.classList.contains('selected'));
    assertEquals(customizeChromeApp, document.activeElement);

    // Send event for category selected.
    customizeChromeApp.$.categoriesPage.dispatchEvent(
        new CustomEvent<BackgroundCollection>(
            'collection-select', {detail: testCollection}));
    await microtasksFinished();
    // Current page should now be themes.
    assertTrue(customizeChromeApp.$.themesPage.classList.contains('selected'));
    assertEquals(customizeChromeApp, document.activeElement);

    // Send event for back click.
    customizeChromeApp.$.themesPage.dispatchEvent(new Event('back-click'));
    await microtasksFinished();
    // Current page should now be categories.
    assertTrue(
        customizeChromeApp.$.categoriesPage.classList.contains('selected'));
    assertEquals(customizeChromeApp, document.activeElement);

    // Send event for upload image.
    customizeChromeApp.$.categoriesPage.dispatchEvent(
        new Event('local-image-upload'));
    await microtasksFinished();
    // Current page should now be overview.
    assertTrue(
        customizeChromeApp.$.overviewPage.classList.contains('selected'));
    assertEquals(customizeChromeApp, document.activeElement);

    // Set page back to categories.
    customizeChromeApp.$.appearanceElement.dispatchEvent(
        new Event('edit-theme-click'));
    await microtasksFinished();
    assertEquals(customizeChromeApp, document.activeElement);

    // Send event for back click.
    customizeChromeApp.$.categoriesPage.dispatchEvent(new Event('back-click'));
    await microtasksFinished();
    // Current page should now be overview.
    assertTrue(
        customizeChromeApp.$.overviewPage.classList.contains('selected'));
    assertEquals(customizeChromeApp, document.activeElement);
  });

  test('app requests scroll to section update', () => {
    window.dispatchEvent(new Event('load'));
    assertGE(handler.getCallCount('updateScrollToSection'), 1);
  });

  test('app scrolls to section', async () => {
    const sections =
        customizeChromeApp.shadowRoot!.querySelectorAll('.section');
    const sectionsScrolledTo: Element[] = [];
    sections.forEach((section) => {
      section.scrollIntoView = () => sectionsScrolledTo.push(section);
    });

    customizeChromeApp.$.appearanceElement.dispatchEvent(
        new Event('edit-theme-click'));
    callbackRouter.scrollToSection(CustomizeChromeSection.kShortcuts);
    await callbackRouter.$.flushForTesting();

    assertEquals(1, sectionsScrolledTo.length);
    assertEquals(
        customizeChromeApp.shadowRoot!.querySelector('#shortcuts'),
        sectionsScrolledTo[0]);
    assertTrue(
        customizeChromeApp.$.overviewPage.classList.contains('selected'));
  });

  suite('ExtensionCard', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        'extensionsCardEnabled': true,
      });
    });

    test(
        'clicking "coupon" card opens Chrome Web Store category page',
        async () => {
          const button =
              customizeChromeApp.shadowRoot!.querySelector<HTMLElement>(
                  '#couponsButton');
          assertTrue(!!button);
          button.click();
          assertEquals(
              1, handler.getCallCount('openChromeWebStoreCategoryPage'));
        });

    test(
        'clicking "writing" card opens Chrome Web Store collection page',
        async () => {
          const button =
              customizeChromeApp.shadowRoot!.querySelector<HTMLElement>(
                  '#writingButton');
          assertTrue(!!button);
          button.click();
          assertEquals(
              1, handler.getCallCount('openChromeWebStoreCollectionPage'));
        });

    test(
        'clicking "productivity" card opens Chrome Web Store category page',
        async () => {
          const button =
              customizeChromeApp.shadowRoot!.querySelector<HTMLElement>(
                  '#productivityButton');
          assertTrue(!!button);
          button.click();
          assertEquals(
              1, handler.getCallCount('openChromeWebStoreCategoryPage'));
        });

    test(
        'clicking Chrome Web Store link opens Chrome Web Store home page',
        async () => {
          const button =
              customizeChromeApp.shadowRoot!.querySelector<HTMLElement>(
                  '#chromeWebstoreLink');
          assertTrue(!!button);
          button.click();
          assertEquals(1, handler.getCallCount('openChromeWebStoreHomePage'));
        });
  });

  [true, false].forEach((flagEnabled) => {
    suite(`ExtensionCardEnabled_${flagEnabled}`, () => {
      suiteSetup(() => {
        loadTimeData.overrideValues({
          'extensionsCardEnabled': flagEnabled,
        });
      });

      test(`extension card does ${flagEnabled ? '' : 'not '}show`, async () => {
        assertEquals(
            !!customizeChromeApp.shadowRoot!.querySelector('#extensions'),
            flagEnabled);
      });
    });
  });

  test('isSourceTabFirstPartyNtp should update the cards', async () => {
    const idsControlledByIsSourceTabFirstPartyNtp = [
      '#shortcuts',
      '#modules',
      '#categoriesPage',
      '#themesPage',
      '#wallpaperSearchPage',
    ];

    const idsNotControlledByIsSourceTabFirstPartyNtp = [
      '#container',
      '#overviewPage',
      '#appearance',
      '#appearanceElement',
      '#toolbarButton',
      '#extensions',
      '#buttonContainer',
    ];

    const checkIdsVisibility = (isSourceTabFirstPartyNtp: boolean) => {
      idsControlledByIsSourceTabFirstPartyNtp.forEach(
          id => assertEquals(
              isSourceTabFirstPartyNtp,
              !!customizeChromeApp.shadowRoot!.querySelector(id)));
      idsNotControlledByIsSourceTabFirstPartyNtp.forEach(
          id => assertTrue(!!customizeChromeApp.shadowRoot!.querySelector(id)));
    };

    await[true, false].forEach(async b => {
      callbackRouter.attachedTabStateUpdated(b);
      await microtasksFinished();
      checkIdsVisibility(b);
    });
  });

  suite('PageTransitions', () => {
    let toolbarCustomizationHandler: TestMock<CustomizeToolbarHandlerInterface>;

    suiteSetup(() => {
      loadTimeData.overrideValues({
        'toolbarCustomizationEnabled': true,
      });
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      toolbarCustomizationHandler = installMock(
          CustomizeToolbarHandlerRemote,
          (mock: CustomizeToolbarHandlerRemote) =>
              CustomizeToolbarApiProxy.setInstance(
                  mock, new CustomizeToolbarClientCallbackRouter()));
      toolbarCustomizationHandler.setResultFor(
          'listActions', Promise.resolve([]));
      toolbarCustomizationHandler.setResultFor(
          'listCategories', Promise.resolve([]));
      toolbarCustomizationHandler.setResultFor(
          'getIsCustomized', Promise.resolve({customized: false}));
    });

    test(
        'page transitions back to overview if not supported by non first party',
        async () => {
          // start on the overview page
          assertTrue(
              customizeChromeApp.$.overviewPage.classList.contains('selected'));
          assertEquals(document.body, document.activeElement);

          // Send event for edit theme being clicked.
          customizeChromeApp.$.appearanceElement.dispatchEvent(
              new Event('edit-theme-click'));
          await microtasksFinished();

          // Current page should now be categories.
          assertTrue(customizeChromeApp.$.categoriesPage.classList.contains(
              'selected'));
          assertEquals(customizeChromeApp, document.activeElement);

          callbackRouter.attachedTabStateUpdated(false);
          await microtasksFinished();

          assertTrue(
              customizeChromeApp.$.overviewPage.classList.contains('selected'));
          assertEquals(document.body, document.activeElement);
        });

    test(
        'page does not transition back to overview if supported by non first ' +
            'party',
        async () => {
          assertTrue(
              !!customizeChromeApp.shadowRoot!.querySelector('#toolbarButton'));

          // Send event for toolbar button being clicked.
          customizeChromeApp.shadowRoot!.querySelector('#toolbarButton')!
              .dispatchEvent(new Event('click'));
          await microtasksFinished();
          // Current page should now be toolbar.
          assertTrue(
              customizeChromeApp.shadowRoot!.querySelector('#toolbarPage')!
                  .classList.contains('selected'));

          callbackRouter.attachedTabStateUpdated(false);
          await microtasksFinished();

          // Current page should now be toolbar.
          assertTrue(
              customizeChromeApp.shadowRoot!.querySelector('#toolbarPage')!
                  .classList.contains('selected'));
        });
  });
});
