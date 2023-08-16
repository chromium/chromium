// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/app.js';

import {AppElement} from 'chrome://customize-chrome-side-panel.top-chrome/app.js';
import {BackgroundCollection, CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote, CustomizeChromePageRemote, CustomizeChromeSection} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertGE, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

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

  test('app changes pages', async () => {
    const testCollection: BackgroundCollection = {
      id: 'test',
      label: 'test',
      previewImageUrl: {url: 'https://test.jpg'},
    };

    // Test initial page state.
    assertTrue(
        customizeChromeApp.$.overviewPage.classList.contains('iron-selected'));
    assertEquals(document.body, document.activeElement);

    // Send event for edit theme being clicked.
    customizeChromeApp.$.appearanceElement.dispatchEvent(
        new Event('edit-theme-click'));
    // Current page should now be categories.
    assertTrue(customizeChromeApp.$.categoriesPage.classList.contains(
        'iron-selected'));
    assertEquals(customizeChromeApp, document.activeElement);

    // Send event for category selected.
    customizeChromeApp.$.categoriesPage.dispatchEvent(
        new CustomEvent<BackgroundCollection>(
            'collection-select', {detail: testCollection}));
    // Current page should now be themes.
    assertTrue(
        customizeChromeApp.$.themesPage.classList.contains('iron-selected'));
    assertEquals(customizeChromeApp, document.activeElement);

    // Send event for back click.
    customizeChromeApp.$.themesPage.dispatchEvent(new Event('back-click'));
    // Current page should now be categories.
    assertTrue(customizeChromeApp.$.categoriesPage.classList.contains(
        'iron-selected'));
    assertEquals(customizeChromeApp, document.activeElement);

    // Send event for upload image.
    customizeChromeApp.$.categoriesPage.dispatchEvent(
        new Event('local-image-upload'));
    // Current page should now be overview.
    assertTrue(
        customizeChromeApp.$.overviewPage.classList.contains('iron-selected'));
    assertEquals(customizeChromeApp, document.activeElement);

    // Set page back to categories.
    customizeChromeApp.$.appearanceElement.dispatchEvent(
        new Event('edit-theme-click'));
    assertEquals(customizeChromeApp, document.activeElement);

    // Send event for chrome colors select.
    customizeChromeApp.$.categoriesPage.dispatchEvent(
        new Event('chrome-colors-select'));
    // Current page should now be chrome colors.
    assertTrue(customizeChromeApp.$.chromeColorsPage.classList.contains(
        'iron-selected'));
    assertEquals(customizeChromeApp, document.activeElement);

    // Send event for back click.
    customizeChromeApp.$.chromeColorsPage.dispatchEvent(
        new Event('back-click'));
    // Current page should now be categories.
    assertTrue(customizeChromeApp.$.categoriesPage.classList.contains(
        'iron-selected'));
    assertEquals(customizeChromeApp, document.activeElement);

    // Send event for back click.
    customizeChromeApp.$.categoriesPage.dispatchEvent(new Event('back-click'));
    // Current page should now be overview.
    assertTrue(
        customizeChromeApp.$.overviewPage.classList.contains('iron-selected'));
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
        customizeChromeApp.$.overviewPage.classList.contains('iron-selected'));
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
});
