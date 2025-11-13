// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/app.js';

import type {AppElement} from 'chrome://customize-chrome-side-panel.top-chrome/app.js';
import type {BackgroundCollection, CustomizeChromePageRemote, ManagementNoticeState} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote, CustomizeChromeSection, NewTabPageType} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {CustomizeToolbarClientCallbackRouter, CustomizeToolbarHandlerRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_toolbar.mojom-webui.js';
import type {CustomizeToolbarHandlerInterface} from 'chrome://customize-chrome-side-panel.top-chrome/customize_toolbar.mojom-webui.js';
import {CustomizeToolbarApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_toolbar/customize_toolbar_api_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertGE, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from './test_support.js';

suite('AppTest', () => {
  let customizeChromeApp: AppElement;
  let handler: TestMock<CustomizeChromePageHandlerRemote>;
  let callbackRouter: CustomizeChromePageRemote;

  setup(() => {
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
    return microtasksFinished();
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
    const sections = customizeChromeApp.shadowRoot.querySelectorAll('.section');
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
        customizeChromeApp.shadowRoot.querySelector('#shortcuts'),
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

    test('clicking "coupon" card opens Chrome Web Store category page', () => {
      const button = customizeChromeApp.shadowRoot.querySelector<HTMLElement>(
          '#couponsButton');
      assertTrue(!!button);
      button.click();
      assertEquals(1, handler.getCallCount('openChromeWebStoreCategoryPage'));
    });

    test(
        'clicking "writing" card opens Chrome Web Store collection page',
        () => {
          const button =
              customizeChromeApp.shadowRoot.querySelector<HTMLElement>(
                  '#writingButton');
          assertTrue(!!button);
          button.click();
          assertEquals(
              1, handler.getCallCount('openChromeWebStoreCollectionPage'));
        });

    test(
        'clicking "productivity" card opens Chrome Web Store category page',
        () => {
          const button =
              customizeChromeApp.shadowRoot.querySelector<HTMLElement>(
                  '#productivityButton');
          assertTrue(!!button);
          button.click();
          assertEquals(
              1, handler.getCallCount('openChromeWebStoreCategoryPage'));
        });

    test(
        'clicking Chrome Web Store link opens Chrome Web Store home page',
        () => {
          const button =
              customizeChromeApp.shadowRoot.querySelector<HTMLElement>(
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

      test(`extension card does ${flagEnabled ? '' : 'not '}show`, () => {
        assertEquals(
            !!customizeChromeApp.shadowRoot.querySelector('#extensions'),
            flagEnabled);
      });
    });
  });

  // Testing Tool Chips visibility on initial flag load values.
  [true, false].forEach(
      (aimPolicyEnabled) => [true, false].forEach(
          (ntpNextFeaturesEnabled) => suite(
              'Render Tool Chips with aimPolicyEnabled: ' + aimPolicyEnabled +
                  ' and ntpNextFeaturesEnabled: ' + ntpNextFeaturesEnabled,
              () => {
                // Arrange
                const expectedVisibility =
                    ntpNextFeaturesEnabled && aimPolicyEnabled;
                suiteSetup(() => {
                  loadTimeData.overrideValues({
                    'ntpNextFeaturesEnabled': ntpNextFeaturesEnabled,
                    'aimPolicyEnabled': aimPolicyEnabled,
                  });
                });

                // Assert
                test(
                    `Expected for tool chips settings to ${
                        expectedVisibility ? 'show' : 'not show'} in the ` +
                        'Customize Chrome side panel',
                    () => {
                      assertEquals(
                          !!customizeChromeApp.shadowRoot.querySelector(
                              '#tools'),
                          expectedVisibility);
                    });
              })));

  test('source tab type should update the cards', async () => {
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

    const newTabPageTypes = [
      NewTabPageType.kFirstPartyWebUI,
      NewTabPageType.kThirdPartyWebUI,
      NewTabPageType.kThirdPartyRemote,
      NewTabPageType.kExtension,
      NewTabPageType.kIncognito,
      NewTabPageType.kGuestMode,
      NewTabPageType.kNone,
    ];

    const checkIdsVisibility = (sourceTabType: NewTabPageType) => {
      idsControlledByIsSourceTabFirstPartyNtp.forEach(
          id => assertEquals(
              sourceTabType === NewTabPageType.kFirstPartyWebUI,
              !!customizeChromeApp.shadowRoot.querySelector(id)));
      idsNotControlledByIsSourceTabFirstPartyNtp.forEach(
          id => assertTrue(!!customizeChromeApp.shadowRoot.querySelector(id)));
    };

    await newTabPageTypes.forEach(async t => {
      callbackRouter.attachedTabStateUpdated(t);
      await microtasksFinished();
      checkIdsVisibility(t);
    });
  });

  suite('PageTransitions', () => {
    let toolbarCustomizationHandler: TestMock<CustomizeToolbarHandlerInterface>;

    suiteSetup(() => {
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

          callbackRouter.attachedTabStateUpdated(NewTabPageType.kExtension);
          callbackRouter.setThemeEditable(false);
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
              !!customizeChromeApp.shadowRoot.querySelector('#toolbarButton'));

          // Send event for toolbar button being clicked.
          customizeChromeApp.shadowRoot
              .querySelector<HTMLElement>('#toolbarButton')!.click();
          await microtasksFinished();
          // Current page should now be toolbar.
          assertTrue(
              customizeChromeApp.shadowRoot.querySelector('#toolbarPage')!
                  .classList.contains('selected'));

          callbackRouter.attachedTabStateUpdated(NewTabPageType.kExtension);
          await microtasksFinished();

          // Current page should now be toolbar.
          assertTrue(
              customizeChromeApp.shadowRoot.querySelector('#toolbarPage')!
                  .classList.contains('selected'));
        });
  });

  suite('Footer', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        'footerEnabled': true,
      });
    });

    ([
      [
        NewTabPageType.kFirstPartyWebUI,
        {canBeShown: false, enabledByPolicy: false},
        false,
        'hidden when no notice can be shown (unmanaged browser)',
      ],
      [
        NewTabPageType.kFirstPartyWebUI,
        {canBeShown: true, enabledByPolicy: false},
        true,
        'visible when the management notice can be shown',
      ],
      [
        NewTabPageType.kFirstPartyWebUI,
        {canBeShown: true, enabledByPolicy: true},
        true,
        'visible when enterprise badge is showing and enforced by policy',
      ],
      [
        NewTabPageType.kExtension,
        {canBeShown: false, enabledByPolicy: false},
        true,
        'visible when extension notice is showing',
      ],
      [
        NewTabPageType.kExtension,
        {canBeShown: true, enabledByPolicy: false},
        true,
        'visible when both notices are showing',
      ],
    ] as Array<[NewTabPageType, ManagementNoticeState, boolean, string]>)
        .forEach(([tabType, managementState, expected, description]) => {
          test(`toggle should be ${description}`, async () => {
            await Promise.all([
              handler.whenCalled('updateFooterSettings'),
              handler.whenCalled('updateAttachedTabState'),
            ]);
            callbackRouter.setFooterSettings(true, true, managementState);
            callbackRouter.attachedTabStateUpdated(tabType);
            await callbackRouter.$.flushForTesting();
            assertEquals(
                expected,
                !!customizeChromeApp.shadowRoot.querySelector('#footer'));
          });
        });

    ([
      [
        false,
        true,
        'visible with extension notice shown but extension policy disabled',
      ],
      [
        true,
        true,
        'visible with extension notice shown but extension policy disabled',
      ],
    ] as Array<[boolean, boolean, string]>)
        .forEach(([extensionPolicyEnabled, expected, description]) => {
          test(`toogle should be ${description}`, async () => {
            await Promise.all([
              handler.whenCalled('updateFooterSettings'),
              handler.whenCalled('updateAttachedTabState'),
            ]);
            callbackRouter.setFooterSettings(
                true, extensionPolicyEnabled,
                {canBeShown: true, enabledByPolicy: false});
            callbackRouter.attachedTabStateUpdated(NewTabPageType.kExtension);
            await callbackRouter.$.flushForTesting();
            assertEquals(
                expected,
                !!customizeChromeApp.shadowRoot.querySelector('#footer'));
          });
        });
  });
});
