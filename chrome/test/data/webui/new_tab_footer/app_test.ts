// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {NewTabFooterAppElement} from 'chrome://newtab-footer/app.js';
import {CustomizeDialogPage, FooterCustomizeChromeEntryPoint, FooterElement} from 'chrome://newtab-footer/app.js';
import {NewTabFooterDocumentProxy} from 'chrome://newtab-footer/browser_proxy.js';
import type {CustomizeButtonsDocumentRemote} from 'chrome://newtab-footer/customize_buttons.mojom-webui.js';
import {CustomizeButtonsDocumentCallbackRouter, CustomizeButtonsHandlerRemote, SidePanelOpenTrigger} from 'chrome://newtab-footer/customize_buttons.mojom-webui.js';
import {CustomizeButtonsProxy} from 'chrome://newtab-footer/customize_buttons_proxy.js';
import {CustomizeChromeSection} from 'chrome://newtab-footer/customize_chrome.mojom-webui.js';
import type {BackgroundAttribution, ManagementNotice, NewTabFooterDocumentRemote} from 'chrome://newtab-footer/new_tab_footer.mojom-webui.js';
import {NewTabFooterDocumentCallbackRouter, NewTabFooterHandlerRemote, NewTabPageType} from 'chrome://newtab-footer/new_tab_footer.mojom-webui.js';
import {WindowProxy} from 'chrome://newtab-footer/window_proxy.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrIconElement} from 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

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
  let handler: TestMock<NewTabFooterHandlerRemote>&NewTabFooterHandlerRemote;
  let callbackRouter: NewTabFooterDocumentRemote;
  let customizeButtonsCallbackRouterRemote: CustomizeButtonsDocumentRemote;
  let customizeButtonsHandler: TestMock<CustomizeButtonsHandlerRemote>;
  let metrics: MetricsTracker;
  let windowProxy: TestMock<WindowProxy>;

  const url: URL = new URL(location.href);

  async function setupFooter(ntpType: NewTabPageType = NewTabPageType.kOther) {
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

    element = document.createElement('new-tab-footer-app');
    document.body.appendChild(element);
    callbackRouter.attachedTabStateUpdated(
        ntpType, /*canCustomizeChrome=*/ true);
    await callbackRouter.$.flushForTesting();
    await microtasksFinished();
  }

  suite('Extension', () => {
    setup(async () => {
      await setupFooter(NewTabPageType.kExtension);
    });

    test('Get extension name on initialization', async () => {
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
      await callbackRouter.$.flushForTesting();

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

    ([
      [NewTabPageType.kExtension, true],
      [NewTabPageType.kFirstPartyWebUI, false],
      [NewTabPageType.kOther, false],
    ] as Array<[NewTabPageType, boolean]>)
        .forEach(([pageType, expected]) => {
          test(`Change NTP type to ${pageType}`, async () => {
            // Act.
            const fooName = 'foo';
            callbackRouter.attachedTabStateUpdated(
                pageType, /*canCustomizeChrome=*/ true);
            callbackRouter.setNtpExtensionName(fooName);
            await callbackRouter.$.flushForTesting();

            // Assert.
            const name = $$(element, '#extensionNameContainer');
            assertEquals(expected, !!name);
          });
        });
  });

  suite('Managed', () => {
    setup(async () => {
      await setupFooter();
    });

    test('Get management notice', async () => {
      // Arrange.
      const managementNotice: ManagementNotice = {
        text: 'Managed by your organization',
        customBitmapDataUrl:
            {url: 'chrome://resources/images/chrome_logo_dark.svg'},
      };

      // Act.
      callbackRouter.setManagementNotice(managementNotice);
      await callbackRouter.$.flushForTesting();

      // Assert.
      const managementNoticeContainer =
          $$(element, '#managementNoticeContainer');
      assertTrue(!!managementNoticeContainer);
      let managementNoticeLink =
          $$(element, '#managementNoticeContainer [role="link"]');
      assertTrue(!!managementNoticeLink);
      assertEquals(
          managementNoticeLink.innerText, 'Managed by your organization');
      let managementNoticeLogo =
          $$<HTMLImageElement>(element, '#managementNoticeLogo');
      assertTrue(!!managementNoticeLogo);
      assertEquals(
          managementNoticeLogo.src,
          'chrome://resources/images/chrome_logo_dark.svg');

      // Act.
      callbackRouter.setManagementNotice(null);
      await callbackRouter.$.flushForTesting();

      // Assert.
      managementNoticeLink =
          $$(element, '#managementNoticeContainer [role="link"]');
      managementNoticeLogo = $$(element, '#managementNoticeLogo');
      assertFalse(!!managementNoticeLink);
      assertFalse(!!managementNoticeLogo);
    });

    test('Management notice logo style', async () => {
      // Arrange.
      const managementNoticeWithCustomLogo: ManagementNotice = {
        text: 'Managed by your organization',
        customBitmapDataUrl:
            {url: 'chrome://resources/images/chrome_logo_dark.svg'},
      };

      // Act.
      callbackRouter.setManagementNotice(managementNoticeWithCustomLogo);
      await callbackRouter.$.flushForTesting();

      // Assert.
      let logoContainter = $$(element, '#managementNoticeLogoContainer');
      assertTrue(!!logoContainter);
      assertTrue(logoContainter.classList.contains('custom_logo'));
      const customManagementNoticeLogo =
          $$<HTMLImageElement>(element, '#managementNoticeLogo');
      assertTrue(!!customManagementNoticeLogo);

      const managementNoticeWithDefaultLogo: ManagementNotice = {
        text: 'Managed by your organization',
        customBitmapDataUrl: null,
      };

      // Act.
      callbackRouter.setManagementNotice(managementNoticeWithDefaultLogo);
      await callbackRouter.$.flushForTesting();

      logoContainter = $$(element, '#managementNoticeLogoContainer');
      assertTrue(!!logoContainter);
      assertEquals(logoContainter.classList.length, 0);
      const defaultManagementNoticeLogo =
          $$<CrIconElement>(element, '#managementNoticeLogo');
      assertTrue(!!defaultManagementNoticeLogo);
      assertEquals('cr:domain', defaultManagementNoticeLogo.icon);
    });

    test('Click manageemnt notice link', async () => {
      // Arrange.
      const managementNotice: ManagementNotice = {
        text: 'Managed by your organization',
        customBitmapDataUrl: null,
      };
      callbackRouter.setManagementNotice(managementNotice);
      await callbackRouter.$.flushForTesting();

      // Act.
      const link = $$(element, '#managementNoticeContainer [role="link"]');
      assertTrue(!!link);
      link.click();

      // Assert.
      assertEquals(1, handler.getCallCount('openManagementPage'));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.Footer.Click', FooterElement.MANAGEMENT_NOTICE));
    });
  });

  suite('CustomizeChromeButton', () => {
    setup(async () => {
      await setupFooter(NewTabPageType.kFirstPartyWebUI);
    });

    function getCustomizeButton(): CrButtonElement {
      const buttons = $$(element, '#customizeButtons');
      assertTrue(!!buttons);
      const button = $$<CrButtonElement>(buttons, '#customizeButton');
      assertTrue(!!button);
      return button;
    }

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

    ([
      [NewTabPageType.kOther, false],
      [NewTabPageType.kFirstPartyWebUI, true],
      [NewTabPageType.kExtension, true],
    ] as Array<[NewTabPageType, boolean]>)
        .forEach(([pageType, expected]) => {
          test(
              `setting ntp type ${pageType} shows customize button ${expected}`,
              async () => {
                // Act.
                callbackRouter.attachedTabStateUpdated(
                    pageType, /*canCustomizeChrome=*/ true);
                await callbackRouter.$.flushForTesting();

                // Assert.
                const buttons = $$(element, '#customizeButtons');
                assertEquals(!!buttons, expected);
              });
        });

    test('button hidden when `canCustomizeChrome` is false', async () => {
      // Arrange.
      const button = getCustomizeButton();
      assertTrue(isVisible(button));

      // Act.
      callbackRouter.attachedTabStateUpdated(
          NewTabPageType.kExtension, /*canCustomizeChrome=*/ false);
      await callbackRouter.$.flushForTesting();

      // Assert.
      const buttons = $$(element, '#customizeButtons');
      assertFalse(!!buttons);
    });
  });

  suite('Misc', () => {
    setup(async () => {
      await setupFooter();
    });

    test(`right click opens context menu`, async () => {
      const container = $$(element, '#container');
      assertTrue(!!container);

      container.dispatchEvent(new MouseEvent('contextmenu'));

      await handler.whenCalled('showContextMenu');
      assertEquals(
          1,
          metrics.count('NewTabPage.Footer.Click', FooterElement.CONTEXT_MENU));
    });
  });

  suite('Background attribution', () => {
    setup(async () => {
      await setupFooter(NewTabPageType.kFirstPartyWebUI);
    });

    test('Get background attribution', async () => {
      // Arrange with empty attribution URL.
      let backgroundAttribution: BackgroundAttribution = {
        name: 'background image name',
        url: {url: ''},
      };

      // Act.
      callbackRouter.setBackgroundAttribution(backgroundAttribution);
      await callbackRouter.$.flushForTesting();

      // Assert that the button is disabled.
      let backgroundAttributionText =
          $$(element, '#backgroundAttributionContainer p');
      assertTrue(!!backgroundAttributionText);
      assertEquals(
          backgroundAttributionText.innerText, 'background image name');
      let backgroundAttributionLink =
          $$(element, '#backgroundAttributionContainer [role="link"]');
      assertFalse(!!backgroundAttributionLink);

      // Arrange with a non-empty URL.
      backgroundAttribution = {
        name: 'background image name',
        url: {url: 'https://info.com'},
      };

      // Act.
      callbackRouter.setBackgroundAttribution(backgroundAttribution);
      await callbackRouter.$.flushForTesting();

      // Assert that the button is enabled.
      backgroundAttributionText =
          $$(element, '#backgroundAttributionContainer p');
      assertFalse(!!backgroundAttributionText);
      backgroundAttributionLink =
          $$(element, '#backgroundAttributionContainer [role="link"]');
      assertTrue(!!backgroundAttributionLink);
      assertEquals(
          backgroundAttributionLink.innerText, 'background image name');
    });

    test('Click background attribution link', async () => {
      // Arrange.
      const attributionUrl = 'https://info.com';
      const backgroundAttribution: BackgroundAttribution = {
        name: 'background image name',
        url: {url: attributionUrl},
      };
      callbackRouter.setBackgroundAttribution(backgroundAttribution);
      await callbackRouter.$.flushForTesting();

      // Act.
      const link = $$(element, '#backgroundAttributionContainer [role="link"]');
      assertTrue(!!link);
      link.click();

      // Assert.
      assertEquals(1, handler.getCallCount('openUrlInCurrentTab'));
      assertEquals(
          attributionUrl, handler.getArgs('openUrlInCurrentTab')[0].url);
    });
  });
});
