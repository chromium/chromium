// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {NewTabFooterAppElement} from 'chrome://newtab-footer/app.js';
import {CustomizeDialogPage, FooterCustomizeChromeEntryPoint, FooterElement} from 'chrome://newtab-footer/app.js';
import {NewTabFooterDocumentProxy} from 'chrome://newtab-footer/browser_proxy.js';
import type {CustomizeButtonsDocumentRemote} from 'chrome://newtab-footer/customize_buttons.mojom-webui.js';
import {CustomizeButtonsDocumentCallbackRouter, CustomizeButtonsHandlerRemote, CustomizeChromeSection, SidePanelOpenTrigger} from 'chrome://newtab-footer/customize_buttons.mojom-webui.js';
import {CustomizeButtonsProxy} from 'chrome://newtab-footer/customize_buttons_proxy.js';
import type {ManagementNotice, NewTabFooterDocumentRemote} from 'chrome://newtab-footer/new_tab_footer.mojom-webui.js';
import {NewTabFooterDocumentCallbackRouter, NewTabFooterHandlerRemote} from 'chrome://newtab-footer/new_tab_footer.mojom-webui.js';
import type {CustomizeButtonsElement} from 'chrome://newtab-footer/shared/customize_buttons/customize_buttons.js';
import {WindowProxy} from 'chrome://newtab-footer/window_proxy.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, microtasksFinished} from 'chrome://webui-test/test_util.js';

type Constructor<T> = new (...args: any[]) => T;
type Installer<T> = (instance: T) => void;

function installMock<T extends object>(
    clazz: Constructor<T>, installer?: Installer<T>): TestMock<T> {
  installer = installer ||
      (clazz as unknown as {setInstance: Installer<T>}).setInstance;
  const mock = TestMock.fromClass(clazz);
  installer(mock);
  return mock;
}

suite('NewTabFooterAppTest', () => {
  let element: NewTabFooterAppElement;
  let customizeButtons: CustomizeButtonsElement;
  let handler: TestMock<NewTabFooterHandlerRemote>&NewTabFooterHandlerRemote;
  let callbackRouter: NewTabFooterDocumentRemote;
  let customizeButtonsCallbackRouterRemote: CustomizeButtonsDocumentRemote;
  let customizeButtonsHandler: TestMock<CustomizeButtonsHandlerRemote>;
  let metrics: MetricsTracker;
  let windowProxy: TestMock<WindowProxy>;

  const url: URL = new URL(location.href);

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = TestMock.fromClass(NewTabFooterHandlerRemote);
    NewTabFooterDocumentProxy.setInstance(
        handler, new NewTabFooterDocumentCallbackRouter());
    callbackRouter = NewTabFooterDocumentProxy.getInstance()
                         .callbackRouter.$.bindNewPipeAndPassRemote();

    customizeButtonsHandler = installMock(
        CustomizeButtonsHandlerRemote,
        mock => CustomizeButtonsProxy.setInstance(
            mock, new CustomizeButtonsDocumentCallbackRouter()));
    customizeButtonsCallbackRouterRemote =
        CustomizeButtonsProxy.getInstance()
            .callbackRouter.$.bindNewPipeAndPassRemote();
    metrics = fakeMetricsPrivate();
    windowProxy = installMock(WindowProxy);
    windowProxy.setResultFor('url', url);

    initializeElement();
    customizeButtons =
        element.shadowRoot.querySelector('ntp-customize-buttons')!;
  });

  async function initializeElement() {
    element = document.createElement('new-tab-footer-app');
    document.body.appendChild(element);
    await microtasksFinished();
    await handler.whenCalled('updateManagementNotice');
  }

  function getCustomizeButton(): CrButtonElement {
    return $$(customizeButtons, '#customizeButton')!;
  }

  suite('Extension', () => {
    test('Get extension name on initialization', async () => {
      // Arrange.
      await initializeElement();

      // Act.
      const fooName = 'foo';
      callbackRouter.setNtpExtensionName(fooName);
      await callbackRouter.$.flushForTesting();

      // Assert.
      let name = $$(element, '#extensionNameContainer');
      assertTrue(!!name);
      const link = name.querySelector<HTMLElement>('[role="link"]');
      assertTrue(!!link);
      assertEquals(link.innerText, fooName);

      // Act.
      callbackRouter.setNtpExtensionName('');
      await callbackRouter.$.flushForTesting();

      // Assert.
      name = $$(element, '#extensionNameContainer');
      assertFalse(!!name);
    });

    test('Click extension name link', async () => {
      // Arrange.
      callbackRouter.setNtpExtensionName('foo');
      await initializeElement();

      // Act.
      const link = $$(element, '#extensionNameContainer [role="link"]');
      assertTrue(!!link);
      link.click();

      // Assert.
      assertEquals(
          1, handler.getCallCount('openExtensionOptionsPageWithFallback'));
      assertEquals(1, metrics.count('NewTabPage.Footer.Click'));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.Footer.Click', FooterElement.EXTENSION_NAME));
    });
  });

  suite('Managed', () => {
    test('Get management notice', async () => {
      // Arrange.
      await initializeElement();
      const managementNotice: ManagementNotice = {
        text: 'Managed by your organization',
        bitmapDataUrl: {url: 'chrome://resources/images/chrome_logo_dark.svg'},
      };

      // Act.
      callbackRouter.setManagementNotice(managementNotice);
      await callbackRouter.$.flushForTesting();

      // Assert.
      const managementNoticeContainer =
          element.shadowRoot.querySelector('#managementNoticeContainer');
      assertTrue(!!managementNoticeContainer);
      let managementNoticeText = managementNoticeContainer.querySelector('p');
      assertTrue(!!managementNoticeText);
      assertEquals(
          managementNoticeText.innerText, 'Managed by your organization');
      let managementNoticeLogo =
          managementNoticeContainer.querySelector<HTMLImageElement>('img');
      assertTrue(!!managementNoticeLogo);
      assertEquals(
          managementNoticeLogo.src,
          'chrome://resources/images/chrome_logo_dark.svg');

      // Act.
      callbackRouter.setManagementNotice(null);
      await callbackRouter.$.flushForTesting();

      // Assert.
      managementNoticeText = $$(element, '#managementNoticeContainer p');
      managementNoticeLogo = $$(element, '#managementNoticeLogo');
      assertFalse(!!managementNoticeText);
      assertFalse(!!managementNoticeLogo);
    });
  });

  suite('CustomizeChromeButton', () => {
    test('clicking customize button opens side panel', () => {
      // Act.
      getCustomizeButton().click();

      // Assert.
      assertDeepEquals(
          [
            true,
            CustomizeChromeSection.kUnspecified,
            SidePanelOpenTrigger.kNewTabFooter,
          ],
          customizeButtonsHandler.getArgs(
              'setCustomizeChromeSidePanelVisible')[0]);
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.Footer.CustomizeChromeOpened',
              FooterCustomizeChromeEntryPoint.CUSTOMIZE_BUTTON));
      assertEquals(
          1,
          customizeButtonsHandler.getCallCount(
              'incrementCustomizeChromeButtonOpenCount'));
    });

    test(`clicking #customizeButton records click`, () => {
      getCustomizeButton().click();
      assertEquals(1, metrics.count('NewTabPage.Footer.Click'));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.Footer.Click', FooterElement.CUSTOMIZE_BUTTON));
    });

    test('clicking customize button hides side panel', async () => {
      // Act.
      customizeButtonsCallbackRouterRemote
          .setCustomizeChromeSidePanelVisibility(true);
      assertEquals(
          0,
          metrics.count(
              'NewTabPage.Footer.CustomizeChromeOpened',
              FooterCustomizeChromeEntryPoint.CUSTOMIZE_BUTTON));
      await customizeButtonsCallbackRouterRemote.$.flushForTesting();
      getCustomizeButton().click();

      // Assert.
      assertDeepEquals(
          [
            false,
            CustomizeChromeSection.kUnspecified,
            SidePanelOpenTrigger.kNewTabFooter,
          ],
          customizeButtonsHandler.getArgs(
              'setCustomizeChromeSidePanelVisible')[0]);
      assertEquals(
          0,
          metrics.count(
              'NewTabPage.Footer.CustomizeChromeOpened',
              FooterCustomizeChromeEntryPoint.CUSTOMIZE_BUTTON));
      assertEquals(
          0,
          customizeButtonsHandler.getCallCount(
              'incrementCustomizeChromeButtonOpenCount'));
    });

    test('clicking customize button is accessible', async () => {
      customizeButtonsCallbackRouterRemote
          .setCustomizeChromeSidePanelVisibility(true);
      await customizeButtonsCallbackRouterRemote.$.flushForTesting();
      assertEquals('true', getCustomizeButton().getAttribute('aria-pressed'));
      customizeButtonsCallbackRouterRemote
          .setCustomizeChromeSidePanelVisibility(false);
      await customizeButtonsCallbackRouterRemote.$.flushForTesting();
      assertEquals('false', getCustomizeButton().getAttribute('aria-pressed'));
    });

    ([
      [
        CustomizeDialogPage.BACKGROUNDS,
        CustomizeChromeSection.kAppearance,
        'BACKGROUNDS',
      ],
      [
        CustomizeDialogPage.THEMES,
        CustomizeChromeSection.kAppearance,
        'THEMES',
      ],
      [
        CustomizeDialogPage.SHORTCUTS,
        CustomizeChromeSection.kShortcuts,
        'SHORTCUTS',
      ],
      [
        CustomizeDialogPage.MODULES,
        CustomizeChromeSection.kModules,
        'MODULES',
      ],
    ] as Array<[CustomizeDialogPage, CustomizeChromeSection, string]>)
        .forEach(([page, section, label]) => {
          test(
              `page ${label} maps to section ${
                  CustomizeChromeSection[section]}`,
              () => {
                element.setSelectedCustomizeDialogPageForTesting(page);
                const returnedSection =
                    element.setCustomizeChromeSidePanelVisible(true);
                assertEquals(section, returnedSection);
              });
        });

    suite('customize URL', () => {
      suiteSetup(() => {
        // We inject the URL param in this suite setup so that the URL is
        // updated before the app element gets created.
        url.searchParams.append('customize', 'themes');
      });

      test('URL opens side panel', () => {
        // Assert.
        assertDeepEquals(
            [
              true,
              CustomizeChromeSection.kAppearance,
              SidePanelOpenTrigger.kNewTabFooter,
            ],
            customizeButtonsHandler.getArgs(
                'setCustomizeChromeSidePanelVisible')[0]);
        assertEquals(
            1,
            metrics.count(
                'NewTabPage.Footer.CustomizeChromeOpened',
                FooterCustomizeChromeEntryPoint.URL));
      });
    });
  });

  suite('Misc', () => {
    test(`right click opens context menu`, async () => {
      await initializeElement();
      const container = $$(element, '#container');
      assertTrue(!!container);

      container.dispatchEvent(new MouseEvent('contextmenu'));

      await handler.whenCalled('showContextMenu');
      assertEquals(
          1,
          metrics.count('NewTabPage.Footer.Click', FooterElement.CONTEXT_MENU));
    });
  });
});
