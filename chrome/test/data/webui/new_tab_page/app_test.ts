// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ActionChipsHandlerRemote, IconType, PageCallbackRouter as ActionChipsPageCallbackRouter} from 'chrome://new-tab-page/action_chips.mojom-webui.js';
import type {PageRemote as ActionChipsPageRemote, TabInfo} from 'chrome://new-tab-page/action_chips.mojom-webui.js';
import type {CustomizeButtonsDocumentRemote} from 'chrome://new-tab-page/customize_buttons.mojom-webui.js';
import {CustomizeButtonsDocumentCallbackRouter, CustomizeButtonsHandlerRemote, SidePanelOpenTrigger} from 'chrome://new-tab-page/customize_buttons.mojom-webui.js';
import {CustomizeChromeSection} from 'chrome://new-tab-page/customize_chrome.mojom-webui.js';
import {ActionChipsApiProxyImpl, VoiceSearchAction} from 'chrome://new-tab-page/lazy_load.js';
import type {Module} from 'chrome://new-tab-page/lazy_load.js';
import {ActionChipsRetrievalState, ComposeboxProxyImpl, counterfactualLoad, ModuleDescriptor, ModuleRegistry} from 'chrome://new-tab-page/lazy_load.js';
import {$$, BackgroundManager, BrowserCommandProxy, CONTEXTUAL_ENTRYPOINT_ELEMENT_ID, CUSTOMIZE_CHROME_BUTTON_ELEMENT_ID, CustomizeButtonsProxy, CustomizeDialogPage, GlifAnimationState, NewTabPageProxy, NtpCustomizeChromeEntryPoint, NtpElement, SearchboxBrowserProxy, VoiceAction, WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import type {AppElement, CustomizeButtonsElement, NtpSearchboxElement} from 'chrome://new-tab-page/new_tab_page.js';
import type {PageRemote} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {NtpBackgroundImageSource, PageCallbackRouter, PageHandlerRemote} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {PageCallbackRouter as ComposeboxPageCallbackRouter, PageHandlerRemote as ComposeboxPageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ToolMode} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {Command, CommandHandlerRemote} from 'chrome://resources/js/browser_command.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertNotStyle, assertStyle, createBackgroundImage, createTheme, installMock} from './test_support.js';

const VOICE_ACTIONS_METRIC = 'NewTabPage.VoiceActions';

suite('NewTabPageAppTest', () => {
  let app: AppElement;
  let customizeButtons: CustomizeButtonsElement;
  let windowProxy: TestMock<WindowProxy>;
  let handler: TestMock<PageHandlerRemote>;
  let callbackRouterRemote: PageRemote;
  let customizeButtonsHandler: TestMock<CustomizeButtonsHandlerRemote>;
  let customizeButtonsCallbackRouterRemote: CustomizeButtonsDocumentRemote;
  let metrics: MetricsTracker;
  let moduleRegistry: TestMock<ModuleRegistry>;
  let backgroundManager: TestMock<BackgroundManager>;
  let moduleResolver: PromiseResolver<Module[]>;
  let searchboxHandler: TestMock<SearchboxPageHandlerRemote>;

  const url: URL = new URL(location.href);
  const backgroundImageLoadTime: number = 123;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    windowProxy = installMock(WindowProxy);
    handler = installMock(
        PageHandlerRemote,
        mock => NewTabPageProxy.setInstance(mock, new PageCallbackRouter()));
    customizeButtonsHandler = installMock(
        CustomizeButtonsHandlerRemote,
        mock => CustomizeButtonsProxy.setInstance(
            mock, new CustomizeButtonsDocumentCallbackRouter()));
    handler.setPromiseResolveFor('getMostVisitedSettings', {
      customLinksEnabled: false,
      shortcutsVisible: false,
    });
    handler.setPromiseResolveFor('getDoodle', {
      doodle: null,
    });
    handler.setPromiseResolveFor('getModulesIdNames', {data: []});
    handler.setPromiseResolveFor('getModulesOrder', {data: []});
    handler.setPromiseResolveFor(
        'canShowRealboxContextMenuAnimation', {canShow: false});
    windowProxy.setResultMapperFor(
        'matchMedia', (query: string) => ({
                        matches: false,
                        media: query,
                        addListener: () => {},
                        addEventListener: () => {},
                        removeListener: () => {},
                        removeEventListener: () => {},
                      }));
    windowProxy.setPromiseResolveFor('waitForLazyRender');
    windowProxy.setResultFor('createIframeSrc', '');
    windowProxy.setResultFor('url', url);
    callbackRouterRemote = NewTabPageProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
    customizeButtonsCallbackRouterRemote =
        CustomizeButtonsProxy.getInstance()
            .callbackRouter.$.bindNewPipeAndPassRemote();
    backgroundManager = installMock(BackgroundManager);
    backgroundManager.setPromiseResolveFor(
        'getBackgroundImageLoadTime', backgroundImageLoadTime);
    moduleRegistry = installMock(ModuleRegistry);
    moduleResolver = new PromiseResolver();
    moduleRegistry.setResultFor('initializeModules', moduleResolver.promise);
    metrics = fakeMetricsPrivate();

    installMock(
        ComposeboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new ComposeboxPageCallbackRouter(),
            new SearchboxPageHandlerRemote(),
            new SearchboxPageCallbackRouter())));
    searchboxHandler = installMock(SearchboxPageHandlerRemote, mock => {
      ComposeboxProxyImpl.getInstance().searchboxHandler = mock;
      SearchboxBrowserProxy.getInstance().handler = mock;
    });
    searchboxHandler.setPromiseResolveFor('getRecentTabs', {tabs: []});
    searchboxHandler.setPromiseResolveFor('getInputState', {
      state: {
        allowedModels: [],
        allowedTools: [],
        allowedInputTypes: [],
        activeModel: 0,
        activeTool: 0,
        disabledModels: [],
        disabledTools: [],
        disabledInputTypes: [],
      },
    });
    searchboxHandler.setResultFor(
        'getPageClassification',
        Promise.resolve({metricSource: 'NTP_REALBOX'}));
    app = document.createElement('ntp-app');
    document.body.appendChild(app);
    await microtasksFinished();

    customizeButtons = app.$.customizeButtons;
  });

  function getCustomizeButton(): CrButtonElement {
    return $$(customizeButtons, '#customizeButton')!;
  }

  function getWallpaperSearchButton(): CrButtonElement {
    return $$(customizeButtons, '#wallpaperSearchButton')!;
  }

  function getComposeButton(): HTMLElement|null {
    const searchboxContainer = app.shadowRoot.querySelector('ntp-searchbox');
    assertTrue(!!searchboxContainer);
    return searchboxContainer.shadowRoot.querySelector<HTMLElement>(
        '#composeButton');
  }

  function getScrim(): HTMLElement|null {
    return app.shadowRoot.querySelector<HTMLElement>('#scrim');
  }

  suite('Misc', () => {
    test('logs height', () => {
      // Assert.
      assertEquals(1, metrics.count('NewTabPage.Height'));
      assertEquals(
          1,
          metrics.count('NewTabPage.Height', Math.floor(window.innerHeight)));
    });

    test('logs width', () => {
      // Assert.
      assertEquals(1, metrics.count('NewTabPage.Width'));
      assertEquals(
          1, metrics.count('NewTabPage.Width', Math.floor(window.innerWidth)));
    });

    test('open voice search event opens voice search overlay', async () => {
      // Act.
      $$(app, '#searchbox')!.dispatchEvent(new Event('open-voice-search'));
      await microtasksFinished();

      // Assert.
      assertTrue(!!app.shadowRoot.querySelector('ntp-voice-search-overlay'));
      assertEquals(1, metrics.count(VOICE_ACTIONS_METRIC));
      assertEquals(
          1, metrics.count(VOICE_ACTIONS_METRIC, VoiceAction.ACTIVATE));
    });

    test('voice search keyboard shortcut', async () => {
      // Test correct shortcut opens voice search.
      // Act.
      window.dispatchEvent(new KeyboardEvent('keydown', {
        ctrlKey: true,
        shiftKey: true,
        code: 'Period',
      }));
      await microtasksFinished();

      // Assert.
      assertTrue(!!app.shadowRoot.querySelector('ntp-voice-search-overlay'));
      assertEquals(1, metrics.count(VOICE_ACTIONS_METRIC));
      assertEquals(
          1,
          metrics.count(VOICE_ACTIONS_METRIC, VoiceAction.ACTIVATE_KEYBOARD));

      // Test other shortcut doesn't close voice search.
      // Act
      window.dispatchEvent(new KeyboardEvent('keydown', {
        ctrlKey: true,
        shiftKey: true,
        code: 'Enter',
      }));
      await microtasksFinished();

      // Assert.
      assertTrue(!!app.shadowRoot.querySelector('ntp-voice-search-overlay'));
    });

    if (isMac) {
      test('keyboard shortcut opens voice search overlay on mac', async () => {
        // Act.
        window.dispatchEvent(new KeyboardEvent('keydown', {
          metaKey: true,
          shiftKey: true,
          code: 'Period',
        }));
        await microtasksFinished();

        // Assert.
        assertTrue(!!app.shadowRoot.querySelector('ntp-voice-search-overlay'));
      });
    }

    test('help bubble can correctly find anchor elements', () => {
      assertDeepEquals(
          app.getSortedAnchorStatusesForTesting(),
          [
            [CUSTOMIZE_CHROME_BUTTON_ELEMENT_ID, true],
          ],
      );
    });

    test('Webstore toast works correctly', async () => {
      const webstoreToast = $$<CrToastElement>(app, '#webstoreToast')!;
      assertTrue(webstoreToast.hidden);

      // Try to show webstore toast without opening side panel.
      callbackRouterRemote.showWebstoreToast();
      await callbackRouterRemote.$.flushForTesting();

      // The webstore toast should still be hidden.
      assertTrue(webstoreToast.hidden);
      assertFalse(webstoreToast.open);

      // Open the side panel.
      customizeButtonsCallbackRouterRemote
          .setCustomizeChromeSidePanelVisibility(true);
      await customizeButtonsCallbackRouterRemote.$.flushForTesting();

      // Try to show webstore toast again.
      callbackRouterRemote.showWebstoreToast();
      await callbackRouterRemote.$.flushForTesting();

      // The webstore toast should be open.
      assertFalse(webstoreToast.hidden);
      assertTrue(webstoreToast.open);
      assertTrue(!!webstoreToast.firstChild!.textContent);
    });
  });

  suite(`OgbThemingRemoveScrim`, () => {
    test('Ogb updates on ntp load', async () => {
      // Act.

      // Create a dark mode theme with a custom background.
      const theme = createTheme({isDark: true});
      theme.backgroundImage = createBackgroundImage('https://foo.com');
      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();

      // Notify the NTP that the ogb has loaded.
      window.dispatchEvent(new MessageEvent('message', {
        data: {
          frameType: 'one-google-bar',
          messageType: 'loaded',
        },
        source: window,
        origin: window.origin,
      }));
      await microtasksFinished();

      // Assert.

      // Dark mode themes with background images and removeScrim set should
      // apply background protection to the ogb.
      assertEquals(1, windowProxy.getCallCount('postMessage'));
      const [_, {type, applyLightTheme}] =
          windowProxy.getArgs('postMessage')[0];
      assertEquals('updateAppearance', type);
      assertEquals(true, applyLightTheme);
      assertNotStyle($$(app, '#oneGoogleBarScrim')!, 'display', 'none');
    });
  });

  suite('OgbScrim', () => {
    test('scroll bounce', async () => {
      // Arrange.

      // Set theme that triggers the scrim.
      const theme = createTheme({isDark: true});
      theme.backgroundImage = createBackgroundImage('https://foo.com');
      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();

      // Make sure page is scrollable.
      const spacer = document.createElement('div');
      spacer.style.width = '100%';
      spacer.style.height = '10000px';
      spacer.style.flexShrink = '0';
      $$(app, '#content')!.append(spacer);

      // Simulates a vertical scroll.
      const scrollY = async (y: number) => {
        window.scroll(0, y);
        // `window.scroll` doesn't automatically trigger scroll event.
        window.dispatchEvent(new Event('scroll'));
        // Wait for position update to propagate.
        await new Promise<void>(
            resolve => requestAnimationFrame(() => resolve()));
      };

      // Act (no bounce).
      await scrollY(0);

      // Assert (no bounce).
      assertStyle($$(app, '#oneGoogleBarScrim')!, 'position', 'fixed');

      // Act (scroll).
      await scrollY(10);

      // Assert (scroll).
      assertStyle($$(app, '#oneGoogleBarScrim')!, 'position', 'absolute');
    });
  });

  suite('Theming', () => {
    test('setting theme updates ntp', async () => {
      // Act.
      callbackRouterRemote.setTheme(createTheme());
      await callbackRouterRemote.$.flushForTesting();
      await microtasksFinished();

      // Assert.
      assertEquals(1, backgroundManager.getCallCount('setBackgroundColor'));
      assertEquals(
          0xffff0000 /* red */,
          (await backgroundManager.whenCalled('setBackgroundColor')).value);
      assertStyle(
          $$(app, '#content')!, '--color-new-tab-page-attribution-foreground',
          'rgba(0, 0, 255, 1.00)');
      assertEquals(1, backgroundManager.getCallCount('setShowBackgroundImage'));
      assertFalse(await backgroundManager.whenCalled('setShowBackgroundImage'));
      assertStyle($$(app, '#backgroundImageAttribution')!, 'display', 'none');
      assertStyle($$(app, '#backgroundImageAttribution2')!, 'display', 'none');
      assertFalse(app.$.logo.singleColored);
      assertTrue(!!app.$.logo.theme);
      assertFalse(app.$.logo.theme?.isDark);
      assertEquals(0xffff0000, app.$.logo.theme?.backgroundColor?.value);
    });

    test('setting 3p theme shows attribution', async () => {
      // Arrange.
      const theme = createTheme();
      theme.backgroundImage = createBackgroundImage('https://foo.com');
      theme.backgroundImage.attributionUrl = 'chrome://theme/foo';

      // Act.
      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();
      await microtasksFinished();

      assertNotStyle($$(app, '#themeAttribution')!, 'display', 'none');
      assertEquals(
          'chrome://theme/foo',
          $$<HTMLImageElement>(app, '#themeAttribution img')!.src);
    });

    test('setting background image shows image', async () => {
      // Arrange.
      const theme = createTheme();
      theme.backgroundImage = createBackgroundImage('https://img.png');

      // Act.
      backgroundManager.resetResolver('setShowBackgroundImage');
      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();

      // Assert.
      assertEquals(1, backgroundManager.getCallCount('setShowBackgroundImage'));
      assertTrue(await backgroundManager.whenCalled('setShowBackgroundImage'));

      // Scrim removal will remove text shadows as background protection is
      // applied to the background element instead.
      assertNotStyle(
          $$(app, '#backgroundImageAttribution')!, 'background-color',
          'rgba(0, 0, 0, 0)');
      assertStyle(
          $$(app, '#backgroundImageAttribution')!, 'text-shadow', 'none');

      assertEquals(1, backgroundManager.getCallCount('setBackgroundImage'));
      assertEquals(
          'https://img.png',
          (await backgroundManager.whenCalled('setBackgroundImage')).url);
      assertTrue(!!app.$.logo.theme?.backgroundColor);
    });

    test('setting attributions shows attributions', async function() {
      // Arrange.
      const theme = createTheme();
      theme.backgroundImageAttribution1 = 'foo';
      theme.backgroundImageAttribution2 = 'bar';
      theme.backgroundImageAttributionUrl = 'https://info.com';

      // Act.
      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();

      // Assert.
      assertNotStyle(
          $$(app, '#backgroundImageAttribution')!, 'display', 'none');
      assertNotStyle(
          $$(app, '#backgroundImageAttribution2')!, 'display', 'none');
      assertEquals(
          'https://info.com',
          $$(app, '#backgroundImageAttribution')!.getAttribute('href'));
      assertEquals(
          'foo', $$(app, '#backgroundImageAttribution1')!.textContent.trim());
      assertEquals(
          'bar', $$(app, '#backgroundImageAttribution2')!.textContent.trim());
    });

    test('setting logo color colors logo', async function() {
      // Arrange.
      const theme = createTheme();
      theme.logoColor = {value: 0xffff0000};

      // Act.
      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();

      // Assert.
      assertTrue(app.$.logo.singleColored);
      assertStyle(app.$.logo, '--ntp-logo-color', 'rgba(255, 0, 0, 1.00)');
    });

    test('theme updates add shortcut color', async () => {
      const theme = createTheme();
      theme.mostVisited.useWhiteTileIcon = true;
      callbackRouterRemote.setTheme(theme);
      const mostVisited = $$(app, '#mostVisited');
      assertTrue(!!mostVisited);
      assertFalse(mostVisited.hasAttribute('use-white-tile-icon_'));
      await callbackRouterRemote.$.flushForTesting();
      assertTrue(mostVisited.hasAttribute('use-white-tile-icon_'));
    });

    test('theme updates is dark', async () => {
      const theme = createTheme();
      theme.mostVisited.isDark = true;
      callbackRouterRemote.setTheme(theme);
      const mostVisited = $$(app, '#mostVisited');
      assertTrue(!!mostVisited);
      assertFalse(mostVisited.hasAttribute('is-dark_'));
      await callbackRouterRemote.$.flushForTesting();
      assertTrue(mostVisited.hasAttribute('is-dark_'));
    });

    [true, false].forEach((isDark) => {
      test(
          `OGB light mode whenever background image
          (ignoring dark mode) isDark: ${isDark}`,
          async () => {
            // Act.

            // Create a theme with a custom background.
            const theme = createTheme({isDark: isDark});
            theme.backgroundImage = createBackgroundImage('https://foo.com');
            callbackRouterRemote.setTheme(theme);
            await callbackRouterRemote.$.flushForTesting();

            // Notify the NTP that the ogb has loaded.
            window.dispatchEvent(new MessageEvent('message', {
              data: {
                frameType: 'one-google-bar',
                messageType: 'loaded',
              },
              source: window,
              origin: window.origin,
            }));
            await microtasksFinished();

            // Assert.
            assertEquals(1, windowProxy.getCallCount('postMessage'));
            const [_, {type, applyLightTheme}] =
                windowProxy.getArgs('postMessage')[0];
            assertEquals('updateAppearance', type);
            assertEquals(true, applyLightTheme);
          });
    });

    suite('theming metrics', () => {
      test('having no theme produces correct metrics', async () => {
        // Arrange.
        const theme = createTheme();
        theme.isCustomBackground = false;

        // Act.
        callbackRouterRemote.setTheme(theme);
        await callbackRouterRemote.$.flushForTesting();

        // Assert.
        assertEquals(1, metrics.count('NewTabPage.Collections.IdOnLoad'));
        assertEquals(1, metrics.count('NewTabPage.Collections.IdOnLoad', ''));
        assertEquals(1, metrics.count('NewTabPage.BackgroundImageSource'));
        assertEquals(
            1,
            metrics.count(
                'NewTabPage.BackgroundImageSource',
                NtpBackgroundImageSource.kNoImage));
      });

      test('having first party theme produces correct metrics', async () => {
        // Arrange.
        const theme = createTheme();
        theme.backgroundImage = createBackgroundImage('https://foo.com');
        theme.backgroundImage.imageSource =
            NtpBackgroundImageSource.kFirstPartyThemeWithoutDailyRefresh;
        theme.backgroundImageCollectionId = 'foo_collection';

        // Act.
        callbackRouterRemote.setTheme(theme);
        await callbackRouterRemote.$.flushForTesting();

        // Assert.
        assertEquals(1, metrics.count('NewTabPage.Collections.IdOnLoad'));
        assertEquals(
            1,
            metrics.count('NewTabPage.Collections.IdOnLoad', 'foo_collection'));
        assertEquals(1, metrics.count('NewTabPage.BackgroundImageSource'));
        assertEquals(
            1,
            metrics.count(
                'NewTabPage.BackgroundImageSource',
                NtpBackgroundImageSource.kFirstPartyThemeWithoutDailyRefresh));
      });

      test('having third party theme produces correct metrics', async () => {
        // Arrange.
        const theme = createTheme();
        theme.backgroundImage = createBackgroundImage('https://foo.com');
        theme.backgroundImage.imageSource =
            NtpBackgroundImageSource.kThirdPartyTheme;

        // Act.
        callbackRouterRemote.setTheme(theme);
        await callbackRouterRemote.$.flushForTesting();

        // Assert.
        assertEquals(1, metrics.count('NewTabPage.Collections.IdOnLoad'));
        assertEquals(1, metrics.count('NewTabPage.Collections.IdOnLoad', ''));
        assertEquals(1, metrics.count('NewTabPage.BackgroundImageSource'));
        assertEquals(
            1,
            metrics.count(
                'NewTabPage.BackgroundImageSource',
                NtpBackgroundImageSource.kThirdPartyTheme));
      });

      test(
          'having refresh daily enabled produces correct metrics', async () => {
            // Arrange.
            const theme = createTheme();
            theme.backgroundImage = createBackgroundImage('https://foo.com');
            theme.backgroundImage.imageSource =
                NtpBackgroundImageSource.kFirstPartyThemeWithDailyRefresh;
            theme.backgroundImageCollectionId = 'foo_collection';

            // Act.
            callbackRouterRemote.setTheme(theme);
            await callbackRouterRemote.$.flushForTesting();

            // Assert.
            assertEquals(1, metrics.count('NewTabPage.Collections.IdOnLoad'));
            assertEquals(
                1,
                metrics.count(
                    'NewTabPage.Collections.IdOnLoad', 'foo_collection'));
            assertEquals(1, metrics.count('NewTabPage.BackgroundImageSource'));
            assertEquals(
                1,
                metrics.count(
                    'NewTabPage.BackgroundImageSource',
                    NtpBackgroundImageSource.kFirstPartyThemeWithDailyRefresh));
          });

      test('setting uploaded background produces correct metrics', async () => {
        // Arrange.
        const theme = createTheme();
        theme.backgroundImage = createBackgroundImage('https://foo.com');
        theme.backgroundImage.imageSource =
            NtpBackgroundImageSource.kUploadedImage;

        // Act.
        callbackRouterRemote.setTheme(theme);
        await callbackRouterRemote.$.flushForTesting();

        // Assert.
        assertEquals(1, metrics.count('NewTabPage.Collections.IdOnLoad'));
        assertEquals(1, metrics.count('NewTabPage.Collections.IdOnLoad', ''));
        assertEquals(1, metrics.count('NewTabPage.BackgroundImageSource'));
        assertEquals(
            1,
            metrics.count(
                'NewTabPage.BackgroundImageSource',
                NtpBackgroundImageSource.kUploadedImage));
      });

      test(
          'setting wallpaper search background produces correct metrics',
          async () => {
            // Arrange.
            const theme = createTheme();
            theme.backgroundImage = createBackgroundImage('https://foo.com');
            theme.backgroundImage.imageSource =
                NtpBackgroundImageSource.kWallpaperSearch;

            // Act.
            callbackRouterRemote.setTheme(theme);
            await callbackRouterRemote.$.flushForTesting();

            // Assert.
            assertEquals(1, metrics.count('NewTabPage.Collections.IdOnLoad'));
            assertEquals(
                1, metrics.count('NewTabPage.Collections.IdOnLoad', ''));
            assertEquals(1, metrics.count('NewTabPage.BackgroundImageSource'));
            assertEquals(
                1,
                metrics.count(
                    'NewTabPage.BackgroundImageSource',
                    NtpBackgroundImageSource.kWallpaperSearch));
          });

      suite('background image load', () => {
        suiteSetup(() => {
          loadTimeData.overrideValues({backgroundImageUrl: 'https://foo.com'});
        });

        test('background image load time is logged', () => {
          // Assert.
          assertEquals(
              1, metrics.count('NewTabPage.Images.ShownTime.BackgroundImage'));
          assertEquals(
              1,
              metrics.count(
                  'NewTabPage.Images.ShownTime.BackgroundImage',
                  Math.floor(
                      backgroundImageLoadTime -
                      window.performance.timeOrigin)));
        });
      });
    });
  });

  suite('Promo', () => {
    test('can show promo with browser command', async () => {
      const promoBrowserCommandHandler = installMock(
          CommandHandlerRemote,
          mock => BrowserCommandProxy.getInstance().handler = mock);
      promoBrowserCommandHandler.setPromiseResolveFor(
          'canExecuteCommand', {canExecute: true});

      const commandId = 123;  // Unsupported command.
      window.dispatchEvent(new MessageEvent('message', {
        data: {
          frameType: 'one-google-bar',
          messageType: 'can-show-promo-with-browser-command',
          commandId,
        },
        source: window,
        origin: window.origin,
      }));

      // Make sure the command is sent to the browser.
      const expectedCommandId =
          await promoBrowserCommandHandler.whenCalled('canExecuteCommand');
      // Unsupported commands get resolved to the default command before being
      // sent to the browser.
      assertEquals(Command.kUnknownCommand, expectedCommandId);

      // Make sure the promo frame gets notified whether the promo can be shown.
      const {data} = await eventToPromise('message', window);
      assertEquals('can-show-promo-with-browser-command', data.messageType);
      assertTrue(data[commandId]);
    });

    test('executes promo browser command', async () => {
      const promoBrowserCommandHandler = installMock(
          CommandHandlerRemote,
          mock => BrowserCommandProxy.getInstance().handler = mock);
      promoBrowserCommandHandler.setPromiseResolveFor(
          'executeCommand', {commandExecuted: true});

      const commandId = 123;  // Unsupported command.
      const clickInfo = {middleButton: true};
      window.dispatchEvent(new MessageEvent('message', {
        data: {
          frameType: 'one-google-bar',
          messageType: 'execute-browser-command',
          data: {
            commandId,
            clickInfo,
          },
        },
        source: window,
        origin: window.origin,
      }));

      // Make sure the command and click information are sent to the browser.
      const [expectedCommandId, expectedClickInfo] =
          await promoBrowserCommandHandler.whenCalled('executeCommand');
      // Unsupported commands get resolved to the default command before being
      // sent to the browser.
      assertEquals(Command.kUnknownCommand, expectedCommandId);
      assertEquals(clickInfo, expectedClickInfo);

      // Make sure the promo frame gets notified whether the command was
      // executed.
      const {data: commandExecuted} = await eventToPromise('message', window);
      assertTrue(commandExecuted);
    });
  });

  suite('Clicks', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        modulesEnabled: true,
      });
    });

    ([
      ['#content', NtpElement.BACKGROUND],
      ['ntp-logo', NtpElement.LOGO],
      ['ntp-searchbox', NtpElement.REALBOX],
      ['cr-most-visited', NtpElement.MOST_VISITED],
      ['ntp-middle-slot-promo', NtpElement.MIDDLE_SLOT_PROMO],
      ['#modules', NtpElement.MODULE],
    ] as Array<[string, NtpElement]>)
        .forEach(([selector, element]) => {
          test(`clicking '${selector}' records click`, () => {
            // Act.
            $$<HTMLElement>(app, selector)!.click();

            // Assert.
            assertEquals(1, metrics.count('NewTabPage.Click'));
            assertEquals(1, metrics.count('NewTabPage.Click', element));
          });
        });

    test(`clicking #customizeButton records click`, () => {
      // Act.
      getCustomizeButton().click();

      // Assert.
      assertEquals(1, metrics.count('NewTabPage.Click'));
      assertEquals(
          1, metrics.count('NewTabPage.Click', NtpElement.CUSTOMIZE_BUTTON));
    });

    test('clicking OGB records click', () => {
      // Act.
      window.dispatchEvent(new MessageEvent('message', {
        data: {
          frameType: 'one-google-bar',
          messageType: 'click',
        },
      }));

      // Assert.
      assertEquals(1, metrics.count('NewTabPage.Click'));
      assertEquals(
          1, metrics.count('NewTabPage.Click', NtpElement.ONE_GOOGLE_BAR));
    });
  });

  function modulesCommonTests(modulesElementTag: string) {
    test('promo and modules coordinate', async () => {
      // Arrange.
      loadTimeData.overrideValues({navigationStartTime: 0.0});
      windowProxy.setResultFor('now', 123.0);
      const middleSlotPromo = $$(app, 'ntp-middle-slot-promo');
      assertTrue(!!middleSlotPromo);
      const modules = $$(app, modulesElementTag)!;
      assertTrue(!!modules);

      // Assert.
      assertStyle(middleSlotPromo, 'display', 'none');
      assertStyle(modules, 'display', 'none');

      // Act.
      middleSlotPromo.dispatchEvent(new Event('ntp-middle-slot-promo-loaded'));

      // Assert.
      assertStyle(middleSlotPromo, 'display', 'none');
      assertStyle(modules, 'display', 'none');

      // Act.
      modules.dispatchEvent(new CustomEvent('modules-loaded', {detail: 1}));
      await microtasksFinished();

      // Assert.
      assertNotStyle(middleSlotPromo, 'display', 'none');
      assertNotStyle(modules, 'display', 'none');
      assertEquals(1, metrics.count('NewTabPage.Modules.ShownTime'));
      assertEquals(1, metrics.count('NewTabPage.Modules.ShownTime', 123));
    });
  }

  suite('V2Modules', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        modulesEnabled: true,
      });
    });

    test('container is hidden', () => {
      const modules = $$(app, 'ntp-modules')!;
      assertTrue(!!modules);
      assertStyle(modules, 'display', 'none');
    });

    test(`clicking records click`, () => {
      // Act.
      $$<HTMLElement>(app, 'ntp-modules')!.click();

      // Assert.
      assertEquals(1, metrics.count('NewTabPage.Click'));
      assertEquals(1, metrics.count('NewTabPage.Click', NtpElement.MODULE));
    });

    modulesCommonTests('ntp-modules');
  });

  suite('CounterfactualModules', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        modulesEnabled: false,
        modulesLoadEnabled: true,
      });
    });

    test('modules loaded but not rendered if counterfactual', async () => {
      // Act.
      const fooElement = document.createElement('div');
      const barElement = document.createElement('div');
      moduleResolver.resolve([
        {
          descriptor:
              new ModuleDescriptor('foo', () => Promise.resolve(fooElement)),
          elements: [fooElement],
        },
        {
          descriptor:
              new ModuleDescriptor('bar', () => Promise.resolve(barElement)),
          elements: [barElement],
        },
      ]);
      await counterfactualLoad();
      await microtasksFinished();

      // Assert.
      assertTrue(moduleRegistry.getCallCount('initializeModules') > 0);
      assertEquals(1, handler.getCallCount('onModulesLoadedWithData'));
      assertEquals(
          0, app.shadowRoot.querySelectorAll('ntp-module-wrapper').length);
    });
  });

  suite('CustomizeChromeSidePanel', () => {
    test('clicking customize button opens side panel', () => {
      // Act.
      getCustomizeButton().click();

      // Assert.
      assertDeepEquals(
          [
            true,
            CustomizeChromeSection.kUnspecified,
            SidePanelOpenTrigger.kNewTabPage,
          ],
          customizeButtonsHandler.getArgs(
              'setCustomizeChromeSidePanelVisible')[0]);
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.CustomizeChromeOpened',
              NtpCustomizeChromeEntryPoint.CUSTOMIZE_BUTTON));
      assertEquals(
          1,
          customizeButtonsHandler.getCallCount(
              'incrementCustomizeChromeButtonOpenCount'));
    });

    test('clicking customize button hides side panel', async () => {
      // Act.
      customizeButtonsCallbackRouterRemote
          .setCustomizeChromeSidePanelVisibility(true);
      assertEquals(
          0,
          metrics.count(
              'NewTabPage.CustomizeChromeOpened',
              NtpCustomizeChromeEntryPoint.CUSTOMIZE_BUTTON));
      await customizeButtonsCallbackRouterRemote.$.flushForTesting();
      getCustomizeButton().click();

      // Assert.
      assertDeepEquals(
          [
            false,
            CustomizeChromeSection.kUnspecified,
            SidePanelOpenTrigger.kNewTabPage,
          ],
          customizeButtonsHandler.getArgs(
              'setCustomizeChromeSidePanelVisible')[0]);
      assertEquals(
          0,
          metrics.count(
              'NewTabPage.CustomizeChromeOpened',
              NtpCustomizeChromeEntryPoint.CUSTOMIZE_BUTTON));
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

    suite('modules', () => {
      suiteSetup(() => {
        loadTimeData.overrideValues({
          modulesEnabled: true,
        });
      });

      test('modules can open side panel', () => {
        // Act.
        $$(app, '#modules')!.dispatchEvent(new Event('customize-module'));

        // Assert.
        assertDeepEquals(
            [
              true,
              CustomizeChromeSection.kModules,
              SidePanelOpenTrigger.kNewTabPage,
            ],
            customizeButtonsHandler.getArgs(
                'setCustomizeChromeSidePanelVisible')[0]);
        assertEquals(
            1,
            metrics.count(
                'NewTabPage.CustomizeChromeOpened',
                NtpCustomizeChromeEntryPoint.MODULE));
      });
    });

    suite('customize URL', () => {
      suiteSetup(() => {
        // We inject the URL param in this suite setup so that the URL is
        // updated before the app element gets created.
        url.searchParams.append('customize', CustomizeDialogPage.THEMES);
      });

      test('URL opens side panel', () => {
        // Assert.
        assertDeepEquals(
            [
              true,
              CustomizeChromeSection.kAppearance,
              SidePanelOpenTrigger.kNewTabPage,
            ],
            customizeButtonsHandler.getArgs(
                'setCustomizeChromeSidePanelVisible')[0]);
        assertEquals(
            1,
            metrics.count(
                'NewTabPage.CustomizeChromeOpened',
                NtpCustomizeChromeEntryPoint.URL));
      });
    });
  });

  suite('LensUploadDialog', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        searchboxLensSearch: true,
        ntpRealboxNextEnabled: true,
      });
    });

    test('lens upload dialog closed on start', () => {
      // Assert.
      assertFalse(!!app.shadowRoot.querySelector('ntp-lens-upload-dialog'));
    });

    test('realbox is not visible when Lens upload dialog is open', async () => {
      // Arrange.
      callbackRouterRemote.setTheme(createTheme());
      await callbackRouterRemote.$.flushForTesting();

      // Act.
      $$(app, '#searchbox')!.dispatchEvent(new Event('open-lens-search'));
      await microtasksFinished();

      // Assert.
      const dialog = app.shadowRoot.querySelector('ntp-lens-upload-dialog');
      assertTrue(!!dialog);
      assertStyle($$(app, '#searchbox')!, 'visibility', 'hidden');

      // Act.
      dialog.closeDialog();
      await microtasksFinished();

      // Assert.
      assertStyle($$(app, '#searchbox')!, 'visibility', 'visible');
    });

    test('scrim is visible when Lens upload dialog is open', async () => {
      // Arrange.
      callbackRouterRemote.setTheme(createTheme());
      await callbackRouterRemote.$.flushForTesting();

      // Act.
      $$(app, '#searchbox')!.dispatchEvent(new Event('open-lens-search'));
      await microtasksFinished();

      // Assert.
      const dialog = app.shadowRoot.querySelector('ntp-lens-upload-dialog');
      assertTrue(!!dialog);
      const scrim = getScrim();
      assertTrue(!!scrim);
      assertFalse(scrim.hidden);

      // Act.
      scrim.click();
      await microtasksFinished();

      // Assert.
      assertTrue(scrim.hidden);
      assertFalse(!!app.shadowRoot.querySelector('ntp-lens-upload-dialog'));
    });
  });

  suite('ComposeEntryPoint', () => {
    const DEFAULT_COMPOSE_CLICK_EVENT_OPTIONS = {
      detail: {
        button: 0,
        ctrlKey: false,
        metaKey: false,
        shiftKey: false,
      },
      bubbles: true,
      composed: true,
    };
    suite('compose feature disabled', () => {
      suiteSetup(() => {
        loadTimeData.overrideValues({
          searchboxShowComposeEntrypoint: false,
          searchboxShowComposebox: false,
        });
      });

      test('compose entrypoint not shown', () => {
        // Assert entrypoint is not shown.
        assertFalse(!!getComposeButton());

        // Assert shown histogram not logged.
        assertEquals(0, metrics.count('NewTabPage.ComposeEntrypoint.Shown'));
        // Assert compose button shown count is not incremented.
        assertEquals(
            0, handler.getCallCount('incrementComposeButtonShownCount'));
      });
    });

    suite('compose features enabled', () => {
      suiteSetup(() => {
        loadTimeData.overrideValues({
          searchboxShowComposeEntrypoint: true,
          searchboxShowComposebox: true,
        });
        // Needed so `.click()` calls don't navigate.
        window.open = () => null;
      });

      test('compose entrypoint shows', () => {
        // Assert shown histogram logged.
        assertEquals(1, metrics.count('NewTabPage.ComposeEntrypoint.Shown'));
        // Assert entrypoint is shown.
        assertTrue(!!getComposeButton());
      });

      test('compose entrypoint emits histograms when clicked', () => {
        // Assert compose button is present.
        const composeButton = getComposeButton();
        assertTrue(!!composeButton);

        // Dispatch the 'compose-click' event directly, which ntp-searchbox
        // listens for. This simulates the `ntp-searchbox-compose-button`
        // child `cr-button` being clicked and its `onClick_` function being
        // called.
        composeButton.dispatchEvent(new CustomEvent(
            'compose-click', DEFAULT_COMPOSE_CLICK_EVENT_OPTIONS));

        // Metric should be recorded without user text present.
        assertEquals(
            1,
            metrics.count(
                'NewTabPage.ComposeEntrypoint.Click.UserTextPresent'));
        assertEquals(
            1,
            metrics.count(
                'NewTabPage.ComposeEntrypoint.Click.UserTextPresent', false));
      });
      test(
          'compose entrypoint emits histograms when clicked with text present',
          () => {
            // Assert compose button is present.
            const searchboxContainer =
                app.shadowRoot.querySelector('ntp-searchbox');
            const composeButton = getComposeButton();
            assertTrue(!!composeButton);

            searchboxContainer!.shadowRoot.querySelector('#input')!.shadowRoot!
                .querySelector<HTMLInputElement>('#input')!.value = 'hello';
            searchboxContainer!.shadowRoot.querySelector('#input')!.shadowRoot!
                .querySelector<HTMLInputElement>('#input')!.dispatchEvent(
                    new InputEvent('input'));

            // Dispatch the 'compose-click' event directly, which ntp-searchbox
            // listens for. This simulates the `ntp-searchbox-compose-button`
            // child `cr-button` being clicked and its `onClick_` function being
            // called.
            composeButton.dispatchEvent(new CustomEvent(
                'compose-click', DEFAULT_COMPOSE_CLICK_EVENT_OPTIONS));

            // Metric should be recorded with user text present.
            assertEquals(
                1,
                metrics.count(
                    'NewTabPage.ComposeEntrypoint.Click.UserTextPresent'));
            assertEquals(
                1,
                metrics.count(
                    'NewTabPage.ComposeEntrypoint.Click.UserTextPresent',
                    true));
          });

      test('compose entrypoint calls submitQuery', () => {
        const searchboxContainer =
            app.shadowRoot.querySelector('ntp-searchbox');
        const composeButton = getComposeButton();
        assertTrue(!!composeButton);

        searchboxContainer!.shadowRoot.querySelector('#input')!.shadowRoot!
            .querySelector<HTMLInputElement>('#input')!.value = 'hello';

        // Act.
        composeButton.dispatchEvent(new CustomEvent(
            'compose-click', DEFAULT_COMPOSE_CLICK_EVENT_OPTIONS));

        // Assert.
        assertEquals(1, searchboxHandler.getCallCount('notifySessionStarted'));
        assertEquals(1, searchboxHandler.getCallCount('submitQuery'));
        const args = searchboxHandler.getArgs('submitQuery')[0];
        assertEquals('hello', args[0]);  // query
      });
    });

    suite('compose entrypoint enabled - composebox disabled', () => {
      suiteSetup(() => {
        loadTimeData.overrideValues({
          searchboxShowComposeEntrypoint: true,
          searchboxShowComposebox: false,
        });
        // Needed so `.click()` calls don't navigate.
        window.open = () => null;
      });

      test('compose entrypoint emits histograms when shown', () => {
        // Assert shown histogram logged.
        assertEquals(1, metrics.count('NewTabPage.ComposeEntrypoint.Shown'));

        // Assert button is present.
        assertTrue(!!getComposeButton());

        // Assert increment compose button shown count is called on load.
        assertEquals(
            1, handler.getCallCount('incrementComposeButtonShownCount'));
      });
      test('compose entrypoint emits histograms when clicked', () => {
        // Assert compose button is present.
        const composeButton = getComposeButton();
        assertTrue(!!composeButton);

        // Dispatch the 'compose-click' event directly, which ntp-searchbox
        // listens for. This simulates the `cr-searchbox-compose-button`
        // child `cr-button` being clicked and its `onClick_` function being
        // called.
        composeButton.dispatchEvent(new CustomEvent(
            'compose-click', DEFAULT_COMPOSE_CLICK_EVENT_OPTIONS));

        // Metric should be recorded without user text present.
        assertEquals(
            1,
            metrics.count(
                'NewTabPage.ComposeEntrypoint.Click.UserTextPresent'));
        assertEquals(
            1,
            metrics.count(
                'NewTabPage.ComposeEntrypoint.Click.UserTextPresent', false));
      });
      test(
          'compose entrypoint emits histograms when clicked with text present',
          () => {
            // Assert compose button is present.
            const searchboxContainer =
                app.shadowRoot.querySelector('ntp-searchbox');
            const composeButton = getComposeButton();
            assertTrue(!!composeButton);

            searchboxContainer!.shadowRoot.querySelector('#input')!.shadowRoot!
                .querySelector<HTMLInputElement>('#input')!.value = 'hello';
            searchboxContainer!.shadowRoot.querySelector('#input')!.shadowRoot!
                .querySelector<HTMLInputElement>('#input')!.dispatchEvent(
                    new InputEvent('input'));

            // Dispatch the 'compose-click' event directly, which cr-searchbox
            // listens for. This simulates the `cr-searchbox-compose-button`
            // child `cr-button` being clicked and its `onClick_` function being
            // called.
            composeButton.dispatchEvent(new CustomEvent(
                'compose-click', DEFAULT_COMPOSE_CLICK_EVENT_OPTIONS));

            // Metric should be recorded with user text present.
            assertEquals(
                1,
                metrics.count(
                    'NewTabPage.ComposeEntrypoint.Click.UserTextPresent'));
            assertEquals(
                1,
                metrics.count(
                    'NewTabPage.ComposeEntrypoint.Click.UserTextPresent',
                    true));
          });
    });
  });

  suite('Composebox', () => {
    const DEFAULT_COMPOSE_CLICK_EVENT_OPTIONS = {
      detail: {
        button: 0,
        ctrlKey: false,
        metaKey: false,
        shiftKey: false,
      },
      bubbles: true,
      composed: true,
    };

    suiteSetup(() => {
      loadTimeData.overrideValues({
        searchboxShowComposeEntrypoint: true,
        searchboxShowComposebox: true,
      });
      // Needed so `.click()` calls don't navigate.
      window.open = () => null;
    });

    test('toggle composebox visibility', async () => {
      // Arrange.
      callbackRouterRemote.setTheme(createTheme());
      await callbackRouterRemote.$.flushForTesting();

      // Act.
      ($$(app, '#searchbox')!.dispatchEvent(new CustomEvent('open-composebox', {
        detail: {text: '', files: []},
      })));
      await microtasksFinished();

      // Assert.
      const composebox = app.shadowRoot.querySelector('cr-composebox');
      assertTrue(!!composebox);
      assertStyle($$(app, '#searchbox')!, 'visibility', 'hidden');
    });

    test('Sequential ESC clears input then closes composebox', async () => {
      // Arrange: Create and open the Composebox UI.
      const searchbox = $$(app, '#searchbox');
      assertTrue(!!searchbox);
      searchbox.dispatchEvent(new CustomEvent('open-composebox', {
        detail: {text: '', files: []},
      }));
      await microtasksFinished();

      const composebox = app.shadowRoot.querySelector('cr-composebox');
      assertTrue(!!composebox);
      // 1. Setup: Simulate input content.
      composebox.getInputElement().$.input.value = 'test input';
      composebox.getInputElement().$.input.dispatchEvent(new Event('input'));
      await microtasksFinished();

      assertEquals('test input', composebox.getInputElement().$.input.value);

      // First ESC: Clear Input (Content present)
      const closePromise1 = eventToPromise('close-composebox', composebox);
      let closedAfterFirstEsc = false;
      closePromise1.then(() => closedAfterFirstEsc = true);

      // Act: Press ESC 1.
      composebox.dispatchEvent(new KeyboardEvent(
          'keydown', {key: 'Escape', bubbles: true, composed: true}));
      await microtasksFinished();

      // Assert 1: Verify input cleared, component did NOT close.
      assertFalse(
          closedAfterFirstEsc,
          'First ESC should clear input, not close the box.');
      assertEquals(
          '', composebox.getInputElement().$.input.value,
          'Input must be cleared after first ESC.');

      // Second ESC: Close Box (Content empty)
      // Act: Press ESC 2.
      const whenCloseComposebox =
          eventToPromise('close-composebox', composebox);

      composebox.dispatchEvent(new KeyboardEvent(
          'keydown', {key: 'Escape', bubbles: true, composed: true}));
      await whenCloseComposebox;  // Wait for the close event

      // Assert 2: Verify close occurred.
      assertTrue(true, 'Second ESC must trigger the close event.');
    });

    test(
        'Clicking the searchbox composebox button notifies composebox handler',
        async () => {
          assertEquals(
              searchboxHandler.getCallCount('notifySessionStarted'), 0);
          assertEquals(
              0,
              metrics.count('NewTabPage.Composebox.FromNTPLoadToSessionStart'));

          const composeButton = getComposeButton();
          assertTrue(!!composeButton);

          // Simulate entrypoint click.
          composeButton.dispatchEvent(new CustomEvent(
              'compose-click', DEFAULT_COMPOSE_CLICK_EVENT_OPTIONS));
          await microtasksFinished();

          // Assert.
          const composebox = app.shadowRoot.querySelector('cr-composebox');
          assertTrue(!!composebox);
          assertEquals(
              searchboxHandler.getCallCount('notifySessionStarted'), 1);
          assertEquals(
              1,
              metrics.count('NewTabPage.Composebox.FromNTPLoadToSessionStart'));
        });
    test(
        'Clicking the searchbox composebox button displays the composebox',
        async () => {
          const composeButton = getComposeButton();
          assertTrue(!!composeButton);

          // Simulate entrypoint click.
          composeButton.dispatchEvent(new CustomEvent(
              'compose-click', DEFAULT_COMPOSE_CLICK_EVENT_OPTIONS));
          await microtasksFinished();

          // Assert.
          const composebox = app.shadowRoot.querySelector('cr-composebox');
          assertTrue(!!composebox);
          assertEquals(
              searchboxHandler.getCallCount('notifySessionStarted'), 1);
        });
    test(
        'Clicking the searchbox composebox button with text navigates',
        async () => {
          const searchboxContainer =
              app.shadowRoot.querySelector('ntp-searchbox');
          const composeButton = getComposeButton();
          assertTrue(!!composeButton);

          searchboxContainer!.shadowRoot.querySelector('#input')!.shadowRoot!
              .querySelector<HTMLInputElement>('#input')!.value = 'hello';
          searchboxContainer!.shadowRoot.querySelector('#input')!.shadowRoot!
              .querySelector<HTMLInputElement>('#input')!.dispatchEvent(
                  new InputEvent('input'));

          // Simulate entrypoint click with text present.
          composeButton.dispatchEvent(new CustomEvent(
              'compose-click', DEFAULT_COMPOSE_CLICK_EVENT_OPTIONS));

          await microtasksFinished();
          assertEquals(
              1,
              metrics.count(
                  'NewTabPage.ComposeEntrypoint.Click.UserTextPresent', true));
        });
    test('Voice search action records metric', async () => {
      // Act.
      const searchbox = $$(app, '#searchbox');
      assertTrue(!!searchbox);
      searchbox.dispatchEvent(new CustomEvent('open-composebox', {
        detail: {text: '', files: []},
      }));
      await microtasksFinished();
      const composebox = $$(app, '#composebox');
      assertTrue(!!composebox);
      composebox.dispatchEvent(new CustomEvent(
          'voice-search-action',
          {detail: {value: VoiceSearchAction.ACTIVATE}}));
      await microtasksFinished();

      // Assert.
      assertEquals(1, metrics.count(VOICE_ACTIONS_METRIC));
      assertEquals(
          1, metrics.count(VOICE_ACTIONS_METRIC, VoiceAction.ACTIVATE));
    });
    [false, true].forEach((ntpRealboxNextEnabled) => {
      test(
          `Propagate composebox text when closed when ntpRealboxNextEnabled is ${
              ntpRealboxNextEnabled}`,
          async () => {
            const searchbox = $$(app, '#searchbox');
            assertTrue(!!searchbox);
            searchbox.dispatchEvent(new CustomEvent('open-composebox', {
              detail: {text: '', files: []},
            }));
            await microtasksFinished();
            const composebox = app.shadowRoot.querySelector('cr-composebox');
            composebox!.input = 'hello';
            const composeboxScrim =
                app.shadowRoot.querySelector<HTMLElement>('#scrim');
            assertTrue(!!composeboxScrim);
            assertEquals(composebox!.input, 'hello');
            composeboxScrim.click();
            await microtasksFinished();

            const searchboxContainer =
                app.shadowRoot.querySelector('ntp-searchbox');

            assertEquals(
                'hello',
                searchboxContainer!.shadowRoot
                    .querySelector('cr-searchbox-input')!.shadowRoot
                    .querySelector<HTMLInputElement>('#input')!.value);
          });
    });

    test(
        'startup and on voice error updates showing error scrim' +
            'property properly',
        async () => {
          const realbox = $$(app, '#searchbox') as NtpSearchboxElement;
          assertTrue(!!realbox, 'realbox should exist');
          realbox.onVoiceSearchClick();
          await microtasksFinished();
          await app.updateComplete;

          assertTrue(
              realbox.inVoiceSearchMode,
              'Voice search should have started, regardless if voice' +
                  ' coherence is being utilized',
          );
          assertTrue(
              realbox.isListening,
              'Realbox voice search should be listening',
          );
          let overlay =
              app.shadowRoot.querySelector('ntp-voice-search-overlay');
          assertTrue(
              !!overlay,
              'Voice search overlay should have started, regardless if' +
                  ' voice coherence is being utilized',
          );
          assertFalse(
              realbox.hasVoiceSearchError,
              'Realbox voice error should hide when opening voice search',
          );

          app.onVoiceSearchError();
          await microtasksFinished();
          await app.updateComplete;

          assertTrue(
              realbox.inVoiceSearchMode,
              'Voice search mode should still be active after error',
          );
          assertTrue(
              realbox.hasVoiceSearchError,
              'Realbox voice error should be showing after error event fired',
          );
          assertFalse(
              realbox.isListening,
              'Realbox voice search should not be listening with error',
          );
          overlay = app.shadowRoot.querySelector('ntp-voice-search-overlay');
          assertTrue(
              !!overlay,
              'Voice search overlay should still show with error',
          );

          app.onVoiceSearchOverlayClose();
          await microtasksFinished();
          await app.updateComplete;

          assertFalse(
              realbox.inVoiceSearchMode,
              'Voice search mode should not be active' +
                  ' after closing after error',
          );
          assertFalse(
              realbox.hasVoiceSearchError,
              'Realbox voice error should not be showing after closing',
          );
          assertFalse(
              realbox.isListening,
              'Realbox voice search should still not be listneing',
          );
          overlay = app.shadowRoot.querySelector('ntp-voice-search-overlay');

          assertFalse(
              !!overlay,
              'Voice search overlay should not show after error closes overlay',
          );

          realbox.onVoiceSearchClick();
          await microtasksFinished();
          await app.updateComplete;

          assertTrue(
              realbox.inVoiceSearchMode,
              'Voice search start after voice button clicked again',
          );
          assertTrue(
              realbox.isListening,
              'Realbox voice search should be listening again',
          );
          overlay = app.shadowRoot.querySelector('ntp-voice-search-overlay');
          assertTrue(
              !!overlay,
              'Voice search overlay should show after voice mode starts again',
          );
          assertFalse(
              realbox.hasVoiceSearchError,
              'Realbox voice error should not show when starting' +
                  ' voice search again',
          );
        });
  });

  suite('WallpaperSearch', () => {
    setup(async () => {
      // Set a theme with no background image and a baseline color to avoid
      // potential conflicts with the ToT value for
      // `wallpaperSearchHideCondition`.
      callbackRouterRemote.setTheme(createTheme({isBaseline: true}));
      await callbackRouterRemote.$.flushForTesting();
      await microtasksFinished();
    });

    suite('ButtonDisabled', () => {
      suiteSetup(() => {
        loadTimeData.overrideValues({
          wallpaperSearchButtonEnabled: false,
        });
      });

      test('does not increment button shown count on startup'), () => {
        assertEquals(
            0,
            customizeButtonsHandler.getCallCount(
                'incrementWallpaperSearchButtonShownCount'));
      };

      test('wallpaper search button is not shown if it is disabled', () => {
        assertTrue(!!getCustomizeButton());
        assertFalse(!!getWallpaperSearchButton());
      });

      test(
          'setting background image styles customize chrome button',
          async () => {
            // Customize chrome button is expanded and its icon has a
            // non-white color.
            assertNotEquals(32, getCustomizeButton().offsetWidth);
            assertNotStyle(
                getCustomizeButton().querySelector('.customize-text')!,
                'display', 'none');
            assertNotStyle(
                getCustomizeButton().querySelector('.customize-icon')!, 'fill',
                'rgb(255, 255, 255)');

            const theme = createTheme({isDark: true});
            theme.backgroundImage = createBackgroundImage('https://foo.com');
            callbackRouterRemote.setTheme(theme);
            await callbackRouterRemote.$.flushForTesting();

            // Customize chrome button is collapsed and its icon is white.
            assertEquals(32, getCustomizeButton().offsetWidth);
            assertStyle(
                getCustomizeButton().querySelector('.customize-icon')!, 'fill',
                'rgb(255, 255, 255)');
            assertStyle(
                getCustomizeButton().querySelector('.customize-text')!,
                'display', 'none');
          });
    });

    function assertButtonAnimated() {
      assertNotStyle(getWallpaperSearchButton(), 'animation-name', 'none');
      assertNotStyle(
          getWallpaperSearchButton().querySelector('.customize-icon')!,
          'animation-name', 'none');
      assertStyle(
          getWallpaperSearchButton().querySelector('.customize-text')!,
          'animation-name', 'none');
    }

    function assertButtonNotAnimated() {
      assertStyle(getWallpaperSearchButton(), 'animation-name', 'none');
      assertStyle(
          getWallpaperSearchButton().querySelector('.customize-icon')!,
          'animation-name', 'none');
      assertStyle(
          getWallpaperSearchButton().querySelector('.customize-text')!,
          'animation-name', 'none');
    }

    suite('ButtonEnabled', () => {
      suiteSetup(() => {
        loadTimeData.overrideValues({
          wallpaperSearchButtonEnabled: true,
          wallpaperSearchButtonAnimationEnabled: true,
        });
      });

      test('increments button shown count on startup'), () => {
        assertEquals(
            1,
            customizeButtonsHandler.getCallCount(
                'incrementWallpaperSearchButtonShownCount'));
      };

      test('wallpaper search button shows if it is enabled', () => {
        assertTrue(!!getCustomizeButton());
        assertTrue(!!getWallpaperSearchButton());
      });

      test('button has animation', () => {
        assertButtonAnimated();
      });

      test(`clicking #customizeButton records click`, () => {
        getCustomizeButton().click();
        assertEquals(1, metrics.count('NewTabPage.Click'));
        assertEquals(
            1, metrics.count('NewTabPage.Click', NtpElement.CUSTOMIZE_BUTTON));
      });

      test(`clicking #wallpaperSearchButton records click`, () => {
        getWallpaperSearchButton().click();
        assertEquals(1, metrics.count('NewTabPage.Click'));
        assertEquals(
            1,
            metrics.count(
                'NewTabPage.Click', NtpElement.WALLPAPER_SEARCH_BUTTON));
      });

      test('clicking wallpaper search button opens side panel', () => {
        getWallpaperSearchButton().click();
        assertDeepEquals(
            [
              true,
              CustomizeChromeSection.kWallpaperSearch,
              SidePanelOpenTrigger.kNewTabPage,
            ],
            customizeButtonsHandler.getArgs(
                'setCustomizeChromeSidePanelVisible')[0]);
        assertEquals(
            1,
            metrics.count(
                'NewTabPage.CustomizeChromeOpened',
                NtpCustomizeChromeEntryPoint.WALLPAPER_SEARCH_BUTTON));
        assertEquals(
            1,
            customizeButtonsHandler.getCallCount(
                'incrementCustomizeChromeButtonOpenCount'));
      });

      test(
          'wallpaper search button can open wallpaper search ' +
              'and hide side panel',
          async () => {
            // Open side panel to non-wallpaper search page.
            customizeButtonsCallbackRouterRemote
                .setCustomizeChromeSidePanelVisibility(true);
            assertEquals(
                0,
                metrics.count(
                    'NewTabPage.CustomizeChromeOpened',
                    NtpCustomizeChromeEntryPoint.WALLPAPER_SEARCH_BUTTON));
            await customizeButtonsCallbackRouterRemote.$.flushForTesting();

            // Clicking the wallpaper search button should navigate the side
            // panel to the wallpaper search page.
            getWallpaperSearchButton().click();
            assertDeepEquals(
                [
                  true,
                  CustomizeChromeSection.kWallpaperSearch,
                  SidePanelOpenTrigger.kNewTabPage,
                ],
                customizeButtonsHandler.getArgs(
                    'setCustomizeChromeSidePanelVisible')[0]);

            // Clicking the wallpaper search button, when the wallpaper search
            // page is opened, should close the side panel.
            getWallpaperSearchButton().click();
            assertDeepEquals(
                [
                  false,
                  CustomizeChromeSection.kUnspecified,
                  SidePanelOpenTrigger.kNewTabPage,
                ],
                customizeButtonsHandler.getArgs(
                    'setCustomizeChromeSidePanelVisible')[1]);
          });

      test('wallpaper search button is accessible', async () => {
        // Open side panel to non-wallpaper search page.
        customizeButtonsCallbackRouterRemote
            .setCustomizeChromeSidePanelVisibility(true);
        await customizeButtonsCallbackRouterRemote.$.flushForTesting();

        // Only customize chrome button should be labeled as pressed.
        assertEquals(
            'false', getWallpaperSearchButton().getAttribute('aria-pressed'));
        assertEquals('true', getCustomizeButton().getAttribute('aria-pressed'));
        // Open wallpaper search page.
        getWallpaperSearchButton().click();
        await microtasksFinished();

        // Both buttons should be labeled as pressed.
        assertEquals(
            'true', getWallpaperSearchButton().getAttribute('aria-pressed'));
        assertEquals('true', getCustomizeButton().getAttribute('aria-pressed'));
        // Close the side panel.
        customizeButtonsCallbackRouterRemote
            .setCustomizeChromeSidePanelVisibility(false);
        await customizeButtonsCallbackRouterRemote.$.flushForTesting();

        // Both buttons should not be labeled as pressed.
        assertEquals(
            'false', getWallpaperSearchButton().getAttribute('aria-pressed'));
        assertEquals(
            'false', getCustomizeButton().getAttribute('aria-pressed'));
      });

      test(
          'clicking wallpaper search button collapses/expands it', async () => {
            assertNotEquals(32, getWallpaperSearchButton().offsetWidth);
            assertNotStyle(
                getWallpaperSearchButton().querySelector('.customize-text')!,
                'display', 'none');
            getWallpaperSearchButton().click();
            await microtasksFinished();

            assertEquals(32, getWallpaperSearchButton().offsetWidth);
            assertStyle(
                getWallpaperSearchButton().querySelector('.customize-text')!,
                'display', 'none');
          });

      test('button hides in accordance with callback router', async () => {
        // Both buttons shown.
        assertNotStyle(getCustomizeButton(), 'display', 'none');
        assertNotStyle(getWallpaperSearchButton(), 'display', 'none');
        callbackRouterRemote.setWallpaperSearchButtonVisibility(false);
        await callbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        // Wallpaper search button hides.
        assertNotStyle(getCustomizeButton(), 'display', 'none');
        assertEquals(null, getWallpaperSearchButton());
        callbackRouterRemote.setWallpaperSearchButtonVisibility(true);
        await callbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        // Wallpaper search button remains hidden.
        assertNotStyle(getCustomizeButton(), 'display', 'none');
        assertEquals(null, getWallpaperSearchButton());
      });
    });

    suite('AnimationDisabled', () => {
      suiteSetup(() => {
        loadTimeData.overrideValues({
          wallpaperSearchButtonEnabled: true,
          wallpaperSearchButtonAnimationEnabled: false,
        });
      });

      test('button has no animation if the flag is disabled', () => {
        assertButtonNotAnimated();
      });
    });

    suite('UnconditionalVisibility', () => {
      suiteSetup(() => {
        loadTimeData.overrideValues({
          wallpaperSearchButtonEnabled: true,
          wallpaperSearchButtonHideCondition: /*NONE*/ 0,
          wallpaperSearchButtonAnimationEnabled: true,
        });
      });

      test('hide condition 0 shows button unconditonally', async () => {
        assertTrue(!!getCustomizeButton());
        assertTrue(!!getWallpaperSearchButton());
        const theme = createTheme({isBaseline: false});
        theme.backgroundImage = createBackgroundImage('https://foo.com');
        await callbackRouterRemote.$.flushForTesting();
        assertTrue(!!getCustomizeButton());
        assertTrue(!!getWallpaperSearchButton());
      });

      test(
          'setting background styles both customize chrome buttons',
          async () => {
            // The fill color of wallpaperSearchButton's icon is explicitly
            // hardcoded to white (#fff) in the SVG <path>, which takes higher
            // percedence over any CSS fill color.
            assertStyle(
                getWallpaperSearchButton()
                    .querySelector(
                        '.customize-icon')!.shadowRoot!.querySelector('path')!,
                'fill', 'rgb(255, 255, 255)');
            // customizeButton's icon should have a non-white color.
            assertNotStyle(
                getCustomizeButton().querySelector('.customize-icon')!, 'fill',
                'rgb(255, 255, 255)');
            // Only customize chrome button should be collapsed.
            assertNotStyle(
                getWallpaperSearchButton().querySelector('.customize-text')!,
                'display', 'none');
            assertStyle(
                getCustomizeButton().querySelector('.customize-text')!,
                'display', 'none');
            assertNotEquals(32, getWallpaperSearchButton().offsetWidth);
            assertEquals(32, getCustomizeButton().offsetWidth);

            // Create and set theme.
            const theme = createTheme({isDark: true});
            theme.backgroundImage = createBackgroundImage('https://foo.com');
            callbackRouterRemote.setTheme(theme);
            await callbackRouterRemote.$.flushForTesting();

            // The fill color of wallpaperSearchButton's icon is explicitly
            // hardcoded to white (#fff) in the SVG <path>, which takes higher
            // percedence over any CSS fill color.
            assertStyle(
                getWallpaperSearchButton()
                    .querySelector(
                        '.customize-icon')!.shadowRoot!.querySelector('path')!,
                'fill', 'rgb(255, 255, 255)');
            // customizeButton's icon should have a non-white color.
            assertStyle(
                getCustomizeButton().querySelector('.customize-icon')!, 'fill',
                'rgb(255, 255, 255)');
            // Only customize chrome button should be collapsed.
            assertNotStyle(
                getWallpaperSearchButton().querySelector('.customize-text')!,
                'display', 'none');
            assertStyle(
                getCustomizeButton().querySelector('.customize-text')!,
                'display', 'none');
            assertNotEquals(32, getWallpaperSearchButton().offsetWidth);
            assertEquals(32, getCustomizeButton().offsetWidth);
          });

      [NtpBackgroundImageSource.kWallpaperSearch,
       NtpBackgroundImageSource.kWallpaperSearchInspiration]
          .forEach((imageSource) => {
            test(
                `having wallpaper search theme ${
                    imageSource} disables animation`,
                async () => {
                  // Arrange.
                  const theme = createTheme();
                  theme.backgroundImage =
                      createBackgroundImage('https://foo.com');
                  theme.backgroundImage.imageSource = imageSource;
                  assertButtonAnimated();

                  // Act.
                  callbackRouterRemote.setTheme(theme);
                  await callbackRouterRemote.$.flushForTesting();

                  // Assert.
                  assertButtonNotAnimated();
                });
          });
    });

    suite('ConditionalVisibility', () => {
      suiteSetup(() => {
        loadTimeData.overrideValues({
          wallpaperSearchButtonEnabled: true,
        });
      });

      test('hideCondition 1 hides button if background is set', async () => {
        loadTimeData.overrideValues({
          wallpaperSearchButtonHideCondition: /*BACKGROUND_IMAGE_SET*/ 1,
        });
        assertTrue(!!getCustomizeButton());
        assertTrue(!!getWallpaperSearchButton());
        // Set theme with a background image and baseline color.
        const theme = createTheme({isBaseline: true});
        theme.backgroundImage = createBackgroundImage('https://img.png');
        callbackRouterRemote.setTheme(theme);
        await backgroundManager.whenCalled('setShowBackgroundImage');
        await microtasksFinished();
        assertTrue(!!getCustomizeButton());
        assertFalse(!!getWallpaperSearchButton());
      });

      test(
          'hide condition 2 hides button if background or' +
              ' non-baseline color is set',
          async () => {
            loadTimeData.overrideValues({
              wallpaperSearchButtonHideCondition: /*THEME_SET*/ 2,
            });
            assertTrue(!!getCustomizeButton());
            assertTrue(!!getWallpaperSearchButton());
            // Set theme with a non-baseline color that has no background image.
            callbackRouterRemote.setTheme(createTheme({isBaseline: false}));
            await callbackRouterRemote.$.flushForTesting();
            await microtasksFinished();
            assertTrue(!!getCustomizeButton());
            assertFalse(!!getWallpaperSearchButton());
            // Resurface button by setting a theme with a baseline color (and no
            // background image).
            callbackRouterRemote.setTheme(createTheme({isBaseline: true}));
            await callbackRouterRemote.$.flushForTesting();
            await microtasksFinished();
            assertTrue(!!getCustomizeButton());
            assertTrue(!!getWallpaperSearchButton());
            // Set theme with a background image and baseline color.
            const theme = createTheme({isBaseline: true});
            theme.backgroundImage = createBackgroundImage('https://img.png');
            callbackRouterRemote.setTheme(theme);
            await backgroundManager.whenCalled('setShowBackgroundImage');
            await microtasksFinished();
            assertTrue(!!getCustomizeButton());
            assertFalse(!!getWallpaperSearchButton());
          });
    });
  });

  suite('MicrosoftAuth', () => {
    [true, false].forEach(
        (microsoftModuleEnabled) =>
            suite(`microsoftModuleEnabled ${microsoftModuleEnabled}`, () => {
              suiteSetup(() => {
                loadTimeData.overrideValues({microsoftModuleEnabled});
              });

              test('Show iframe when appropriate', () => {
                const iframe =
                    $$<HTMLIFrameElement>(app, 'iframe#microsoftAuth');

                if (!microsoftModuleEnabled) {
                  assertFalse(!!iframe);
                  return;
                }

                assertTrue(!!iframe);
                assertEquals(
                    'chrome-untrusted://ntp-microsoft-auth/', iframe.src);
              });
            }));
  });

  suite('AutoRemovalToast', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        modulesEnabled: true,
        shortcutsEnabled: true,
      });
    });

    test('displays single toast', async () => {
      // Arrange.
      const modules = app.shadowRoot.querySelector('ntp-modules');
      assertTrue(!!modules);

      // Act.
      modules.dispatchEvent(new CustomEvent('modules-auto-removed', {
        detail: {message: 'Module removed', undo: () => {}},
        bubbles: true,
        composed: true,
      }));
      await microtasksFinished();

      // Assert.
      assertTrue(app.$.undoToast.open);
      assertEquals('Module removed', app.$.undoToastMessage.textContent.trim());
    });

    test('queues multiple toasts', async () => {
      // Arrange.
      const modules = app.shadowRoot.querySelector('ntp-modules');
      assertTrue(!!modules);
      const mostVisited = app.shadowRoot.querySelector('cr-most-visited');
      assertTrue(!!mostVisited);

      // Act - dispatch the first toast.
      modules.dispatchEvent(new CustomEvent('modules-auto-removed', {
        detail: {message: 'Modules hidden', undo: () => {}},
        bubbles: true,
        composed: true,
      }));
      await microtasksFinished();

      // Act - dispatch the second toast.
      mostVisited.dispatchEvent(new CustomEvent('most-visited-auto-removed', {
        detail: {message: 'Shortcuts hidden', undo: () => {}},
        bubbles: true,
        composed: true,
      }));
      await microtasksFinished();

      // Assert.
      assertTrue(app.$.undoToast.open);
      assertEquals('Modules hidden', app.$.undoToastMessage.textContent.trim());

      // Act - clicking undo on the first toast.
      let undoButton = app.shadowRoot.querySelector<HTMLElement>('#undoButton');
      assertTrue(!!undoButton);
      undoButton.click();
      await microtasksFinished();

      // Assert.
      assertTrue(app.$.undoToast.open);
      assertEquals(
          'Shortcuts hidden', app.$.undoToastMessage.textContent.trim());

      // Act - clicking undo on the second toast.
      undoButton = app.shadowRoot.querySelector<HTMLElement>('#undoButton');
      assertTrue(!!undoButton);
      undoButton.click();
      await microtasksFinished();

      // Assert.
      assertFalse(app.$.undoToast.open);
    });

    test('toast with null undo callback', async () => {
      // Arrange.
      const modules = app.shadowRoot.querySelector('ntp-modules');
      assertTrue(!!modules);
      const mostVisited = app.shadowRoot.querySelector('cr-most-visited');
      assertTrue(!!mostVisited);

      // Act - dispatch the first toast with null callback.
      modules.dispatchEvent(new CustomEvent('modules-auto-removed', {
        detail: {message: 'Module removed', undo: null},
        bubbles: true,
        composed: true,
      }));
      await microtasksFinished();

      // Act - dispatch the second toast with non-null callback.
      mostVisited.dispatchEvent(new CustomEvent('most-visited-auto-removed', {
        detail: {message: 'Shortcuts hidden', undo: () => {}},
        bubbles: true,
        composed: true,
      }));
      await microtasksFinished();

      // Assert.
      assertTrue(app.$.undoToast.open);
      assertEquals('Module removed', app.$.undoToastMessage.textContent.trim());

      // Act - clicking undo on the first toast does not crash.
      let undoButton = app.shadowRoot.querySelector<HTMLElement>('#undoButton');
      assertTrue(!!undoButton);
      undoButton.click();
      await microtasksFinished();

      // Assert.
      assertTrue(app.$.undoToast.open);
      assertEquals(
          'Shortcuts hidden', app.$.undoToastMessage.textContent.trim());

      // Act - clicking undo on the second toast.
      undoButton = app.shadowRoot.querySelector<HTMLElement>('#undoButton');
      assertTrue(!!undoButton);
      undoButton.click();
      await microtasksFinished();

      // Assert - no crash and toast closed.
      assertFalse(app.$.undoToast.open);
    });
  });

  suite('NewTabFooter', () => {
    test('hide/show customize chrome and attribution buttons', async () => {
      // Arrange.
      const theme = createTheme();
      theme.backgroundImageAttribution1 = 'foo';
      theme.backgroundImageAttribution2 = 'bar';
      theme.backgroundImageAttributionUrl = 'https://info.com';
      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();

      // Assert default state of the buttons.
      assertTrue(!!$$(app, '#customizeButtons'));
      assertTrue(!!$$(app, '#backgroundImageAttribution'));

      // Act.
      callbackRouterRemote.footerVisibilityUpdated(true);
      await callbackRouterRemote.$.flushForTesting();
      await microtasksFinished();

      // Assert.
      assertFalse(!!$$(app, '#customizeButtons'));
      assertFalse(!!$$(app, '#backgroundImageAttribution'));

      // Act.
      callbackRouterRemote.footerVisibilityUpdated(false);
      await callbackRouterRemote.$.flushForTesting();
      await microtasksFinished();

      // Assert.
      assertTrue(!!$$(app, '#customizeButtons'));
      assertTrue(!!$$(app, '#backgroundImageAttribution'));
    });
  });

  suite('RealboxNext', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        ntpRealboxNextEnabled: true,
        composeboxCloseByClickOutside: true,
      });
    });

    test(
        'A scrim is applied when the focus is on the composebox input',
        async () => {
          const scrim = getScrim();
          assertTrue(!!scrim);
          assertTrue(scrim?.hidden);
          const realbox = $$(app, '#searchbox');
          assertTrue(!!realbox);
          realbox.dispatchEvent(new CustomEvent('open-composebox', {
            detail: {text: '', files: []},
          }));
          await microtasksFinished();
          const composebox = app.shadowRoot.querySelector('cr-composebox');
          assertTrue(!!composebox);
          composebox.getInputElement().$.input.dispatchEvent(
              new FocusEvent('focus'));
          await microtasksFinished();

          assertFalse(scrim.hidden);

          composebox.getInputElement().$.input.dispatchEvent(
              new FocusEvent('focusout', {relatedTarget: scrim}));
          await microtasksFinished();
          scrim.click();
          await microtasksFinished();
          assertTrue(scrim?.hidden);
          // Composebox should have been closed.
          assertFalse(!!app.shadowRoot.querySelector('cr-composebox'));
        });

    test('scrim remains shown when context menu is clicked', async () => {
      const scrim = getScrim();
      assertTrue(!!scrim);
      assertTrue(scrim.hidden);

      const searchboxContainer =
          app.shadowRoot.getElementById('searchboxContainer')!;
      const searchbox = $$(app, '#searchbox')!;

      // Click on the searchbox.
      searchboxContainer.dispatchEvent(new Event('focusin', {bubbles: true}));
      await microtasksFinished();

      // Assert scrim is shown.
      assertFalse(scrim.hidden);

      // Click on the context menu (the plus `+` button).
      // This fires open-composebox on the searchbox element.
      searchbox.dispatchEvent(new CustomEvent('open-composebox', {
        detail: {text: '', files: []},
      }));
      await microtasksFinished();
      assertFalse(scrim.hidden);
    });

    test('scrim is hidden after closing composebox', async () => {
      const scrim = getScrim()!;
      const searchbox = $$<NtpSearchboxElement>(app, '#searchbox')!;
      const searchboxContainer =
          app.shadowRoot.getElementById('searchboxContainer')!;

      // 1. Open NTP.
      // 2. Click on the searchbox.
      searchboxContainer.dispatchEvent(new Event('focusin', {bubbles: true}));
      await microtasksFinished();
      assertFalse(scrim.hidden);

      // 3 & 4. Open composebox (Deep Search tool).
      searchbox.dispatchEvent(new CustomEvent('open-composebox', {
        detail: {text: '', files: []},
      }));
      await microtasksFinished();
      let composeboxDialog = app.shadowRoot.querySelector('#composeboxDialog');
      assertTrue(!!composeboxDialog);
      assertFalse(scrim.hidden);

      // 5 & 6. Close composebox and clear modes (the 'x' button clicks).
      const composebox = app.shadowRoot.querySelector('cr-composebox')!;
      composebox.dispatchEvent(new CustomEvent('close-composebox', {
        detail: {composeboxText: ''},
        bubbles: true,
        composed: true,
      }));
      await microtasksFinished();
      composeboxDialog = app.shadowRoot.querySelector('#composeboxDialog');
      assertFalse(!!composeboxDialog);

      // The scrim should now be hidden because focus was lost
      // when the dialog closed.
      assertTrue(scrim.hidden);
    });

    test('searchbox text carries over to composebox', async () => {
      // Arrange.
      callbackRouterRemote.setTheme(createTheme());
      await callbackRouterRemote.$.flushForTesting();

      // Act.
      ($$(app, '#searchbox')!.dispatchEvent(new CustomEvent('open-composebox', {
        detail: {text: 'text', files: []},
      })));
      await microtasksFinished();

      // Assert.
      const composebox = app.shadowRoot.querySelector('cr-composebox');
      assertTrue(!!composebox);

      assertEquals('text', composebox.getInputElement().$.input.value);
      assertStyle($$(app, '#searchbox')!, 'visibility', 'hidden');
    });

    test('Contextual entrypoint IPH', () => {
      assertTrue(app.getSortedAnchorStatusesForTesting().some(
          ([anchorId, hasAnchor]: [string, boolean]) => {
            return anchorId === CONTEXTUAL_ENTRYPOINT_ELEMENT_ID && hasAnchor;
          }));
    });
  });

  suite('ActionChips', () => {
    let actionChipsPageRemote: ActionChipsPageRemote;
    suiteSetup(() => {
      loadTimeData.overrideValues({
        ntpNextFeaturesEnabled: true,
        ntpRealboxNextEnabled: true,
        actionChipsEnabled: true,
        addTabUploadDelayOnActionChipClick: true,
        ntpNextDisablementEnabled: true,
      });
      const actionChipsCallbackRouter = new ActionChipsPageCallbackRouter();
      const actionChipshandler = installMock(
          ActionChipsHandlerRemote,
          mock => ActionChipsApiProxyImpl.setInstance({
            getHandler: () => mock,
            getCallbackRouter: () => actionChipsCallbackRouter,
          }));
      const fakeTab: TabInfo = {
        tabId: 1,
        title: 'Test Title',
        url: 'https://example.com/test',
        lastActiveTime: {internalValue: BigInt(12345)},
      };
      actionChipsPageRemote =
          actionChipsCallbackRouter.$.bindNewPipeAndPassRemote();
      actionChipshandler.setResultMapperFor('startActionChipsRetrieval', () => {
        actionChipsPageRemote.onActionChipsChanged([
          {
            suggestion: 'tab-suggestion',
            suggestTemplateInfo: {
              typeIcon: IconType.kFavicon,
              primaryText: {text: 'TabContext', a11yText: null},
              secondaryText: {text: 'tab-subtitle', a11yText: null},
              preselectedTool: ToolMode.kUnspecified,
            },
            tab: fakeTab,
          },
          {
            suggestion: 'image-suggestion',
            suggestTemplateInfo: {
              typeIcon: IconType.kBanana,
              primaryText: {text: 'Nano Banana', a11yText: null},
              secondaryText: {text: 'image-subtitle', a11yText: null},
              preselectedTool: ToolMode.kImageGen,
            },
            tab: null,
          },
          {
            suggestion: 'ds-suggestion',
            suggestTemplateInfo: {
              typeIcon: IconType.kGlobeWithSearchLoop,
              primaryText: {text: 'DeepSearch', a11yText: null},
              secondaryText: {text: 'ds-subtitle', a11yText: null},
              preselectedTool: ToolMode.kDeepSearch,
            },
            tab: null,
          },
        ]);
      });
    });

    // Testing Action Chips visibility on initial flag load values.
    [true, false].forEach(
        (ntpNextDisablementEnabled) => [true, false].forEach(
            (actionChipsEnabled) => [true, false].forEach(
                (ntpNextFeaturesEnabled) => suite(
                    'Action Chips settings rendered with actionChipsEnabled: ' +
                        actionChipsEnabled + ' and ntpNextFeaturesEnabled: ' +
                        ntpNextFeaturesEnabled +
                        ' and ntpNextDisablementEnabled: ' +
                        ntpNextDisablementEnabled,
                    () => {
                      suiteSetup(() => {
                        loadTimeData.overrideValues({
                          ntpNextFeaturesEnabled,
                          actionChipsEnabled,
                          ntpNextDisablementEnabled,
                        });
                      });

                      // Assert.
                      test('Show action chips when appropriate', () => {
                        const expectedVisibility = ntpNextFeaturesEnabled &&
                            (!ntpNextDisablementEnabled || actionChipsEnabled);
                        const chips = $$<HTMLElement>(app, 'ntp-action-chips');
                        assertEquals(!!chips, expectedVisibility);
                      });
                    }))));

    // Testing Action Chips visibility on changing visibility prefs.
    [true, false].forEach(
        (isActionChipsVisible) => test(
            'Action chips are rendered/hidden on changing visibility to ' +
                isActionChipsVisible,
            async () => {
              // Act.
              callbackRouterRemote.setActionChipsVisibility(
                  isActionChipsVisible);
              await callbackRouterRemote.$.flushForTesting();
              await microtasksFinished();

              // Assert.
              const chips = $$(app, 'ntp-action-chips');
              assertEquals(!!chips, isActionChipsVisible);
            }));

    test('Show background when non-GM3 theme', async () => {
      // Arrange.
      const theme = createTheme({isGm3: false});
      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();

      // Assert.
      const chips = $$(app, 'ntp-action-chips')!;
      assertTrue(chips.showBackground);
    });

    test('Show background when background image', async () => {
      // Arrange.
      const theme = createTheme({isGm3: true});
      theme.backgroundImage = createBackgroundImage('https://img.png');
      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();

      // Assert.
      const chips = $$(app, 'ntp-action-chips')!;
      assertTrue(chips.showBackground);
    });

    test('Do not show background when GM2 w/ no background image', async () => {
      // Arrange.
      const theme = createTheme({isGm3: true});
      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();

      // Assert.
      const chips = $$(app, 'ntp-action-chips')!;
      assertFalse(chips.showBackground);
    });

    test(
        'Nano Banana chip click opens composebox create image mode',
        async () => {
          searchboxHandler.setPromiseResolveFor('getRecentTabs', {tabs: []});
          const actionChipsElement =
              app.shadowRoot.querySelector('ntp-action-chips');
          assertTrue(!!actionChipsElement);
          const nanoBananaChip =
              actionChipsElement.shadowRoot.querySelector<HTMLDivElement>(
                  '.icon-type-banana');
          assertTrue(!!nanoBananaChip);

          // Act.
          nanoBananaChip.click();
          await microtasksFinished();

          // Assert.
          const composebox = app.shadowRoot.getElementById('composebox');
          assertTrue(!!composebox);
          assertEquals(1, searchboxHandler.getCallCount('setActiveToolMode'));
          assertEquals(
              ToolMode.kImageGen,
              searchboxHandler.getArgs('setActiveToolMode')[0]);
        });
    test(
        'Deep search chip click opens composebox deep search mode',
        async () => {
          const actionChipsElement =
              app.shadowRoot.querySelector('ntp-action-chips');
          assertTrue(!!actionChipsElement);

          // Setup.
          const deepSearchChip =
              actionChipsElement.shadowRoot.querySelector<HTMLDivElement>(
                  '.icon-type-globe-with-search-loop');
          assertTrue(!!deepSearchChip);
          deepSearchChip.click();
          await microtasksFinished();

          // Assert.
          const composebox = app.shadowRoot.getElementById('composebox');
          assertTrue(!!composebox);
          assertEquals(1, searchboxHandler.getCallCount('setActiveToolMode'));
          assertEquals(
              ToolMode.kDeepSearch,
              searchboxHandler.getArgs('setActiveToolMode')[0]);
        });
    test('Recent tab chip click opens composebox with context', async () => {
      const actionChipsElement =
          app.shadowRoot.querySelector('ntp-action-chips');
      assertTrue(!!actionChipsElement);

      // Setup.
      const tabChip =
          actionChipsElement.shadowRoot.querySelector<HTMLDivElement>(
              '.icon-type-favicon');
      assertTrue(!!tabChip);
      tabChip.click();
      await microtasksFinished();

      // Assert.
      const composebox = app.shadowRoot.getElementById('composebox');
      assertTrue(!!composebox);
      assertEquals(1, searchboxHandler.getCallCount('addTabContext'));
      const [tabId, delayUpload] = searchboxHandler.getArgs('addTabContext')[0];
      assertEquals(1, tabId);
      assertEquals(true, delayUpload);
    });
    test(
        'Deep dive chip click opens composebox with context and suggestion',
        async () => {
          const subtitle = 'Help me with this page subtitle';
          const suggestion = 'Help me with this page suggestion';
          actionChipsPageRemote.onActionChipsChanged([{
            suggestion: suggestion,
            suggestTemplateInfo: {
              typeIcon: IconType.kSubArrowRight,
              primaryText: {text: 'Deep dive', a11yText: null},
              secondaryText: {text: subtitle, a11yText: null},
              preselectedTool: ToolMode.kUnspecified,
            },
            tab: {
              tabId: 1,
              title: 'Test Title',
              url: 'https://example.com/test',
              lastActiveTime: {internalValue: BigInt(0)},
            },
          }]);
          await microtasksFinished();
          const actionChipsElement =
              app.shadowRoot.querySelector('ntp-action-chips');
          assertTrue(!!actionChipsElement);

          // Setup.
          const deepDiveChip =
              actionChipsElement.shadowRoot.querySelector<HTMLButtonElement>(
                  'button:has(.icon-type-sub-arrow-right)');
          assertTrue(!!deepDiveChip);

          const chipBody = deepDiveChip.querySelector('.chip-body');
          assertTrue(!!chipBody);
          assertEquals(subtitle, chipBody.textContent.trim());

          // Act.
          deepDiveChip.click();
          await microtasksFinished();

          // Assert.
          const composebox = app.shadowRoot.querySelector('cr-composebox');
          assertTrue(!!composebox);
          assertEquals(1, searchboxHandler.getCallCount('addTabContext'));
          const [tabId, delayUpload] =
              searchboxHandler.getArgs('addTabContext')[0];
          assertEquals(1, tabId);
          assertEquals(true, delayUpload);
          assertTrue(!!composebox.getInputElement().$.input);
          assertEquals(suggestion, composebox.getInputElement().$.input.value);
        });
  });

  suite('ThreadsRail', () => {
    async function setThreadsRailEnabled(enabled: boolean) {
      loadTimeData.overrideValues({enableThreadsRail: enabled});
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      app = document.createElement('ntp-app');
      document.body.appendChild(app);
      await microtasksFinished();
    }

    test('threads rail is not visible when feature disabled', async () => {
      await setThreadsRailEnabled(false);
      const searchbox = $$(app, '#searchbox');
      assertTrue(!!searchbox);
      searchbox.dispatchEvent(new CustomEvent('open-composebox', {
        detail: {text: '', files: []},
      }));
      await microtasksFinished();

      const threadsRail = app.shadowRoot.querySelector('cr-threads-rail');
      assertFalse(!!threadsRail);
    });

    test('threads rail is visible when feature enabled', async () => {
      await setThreadsRailEnabled(true);
      const searchbox = $$(app, '#searchbox');
      assertTrue(!!searchbox);
      searchbox.dispatchEvent(new CustomEvent('open-composebox', {
        detail: {text: '', files: []},
      }));
      await microtasksFinished();

      const threadsRail = app.shadowRoot.querySelector('cr-threads-rail');
      assertTrue(!!threadsRail);
    });

    test('records impression metric when threads rail is shown', async () => {
      await setThreadsRailEnabled(true);
      // Act: Open composebox to show threads rail.
      ($$(app, '#searchbox')!.dispatchEvent(new CustomEvent('open-composebox', {
        detail: {text: '', files: []},
      })));
      await microtasksFinished();

      // Assert: Verify impression metric is recorded.
      assertEquals(1, metrics.count('NewTabPage.ThreadsRail.Shown', true));
    });

    test('clicking threads rail records click', async () => {
      await setThreadsRailEnabled(true);
      // Arrange: Open composebox.
      ($$(app, '#searchbox')!.dispatchEvent(new CustomEvent('open-composebox', {
        detail: {text: '', files: []},
      })));
      await microtasksFinished();

      const threadsRail = app.shadowRoot.querySelector('cr-threads-rail');
      assertTrue(!!threadsRail);

      // Act.
      threadsRail.click();

      // Assert.
      assertEquals(
          1, metrics.count('NewTabPage.Click', NtpElement.THREADS_RAIL));
    });
  });
});

suite('NewTabPageAppReducedMotionTest', () => {
  suiteSetup(() => {
    loadTimeData.overrideValues({
      ntpRealboxNextEnabled: true,
      ntpNextFeaturesEnabled: true,
      searchboxShowComposebox: true,
      searchboxShowComposeEntrypoint: true,
      actionChipsEnabled: true,
    });
  });

  let app: AppElement;
  let windowProxy: TestMock<WindowProxy>;
  let handler: TestMock<PageHandlerRemote>;
  let backgroundManager: TestMock<BackgroundManager>;
  let searchboxHandler: TestMock<SearchboxPageHandlerRemote>;
  let moduleRegistry: TestMock<ModuleRegistry>;
  let moduleResolver: PromiseResolver<Module[]>;

  const url: URL = new URL(location.href);
  const backgroundImageLoadTime: number = 123;

  function createSetup() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    windowProxy =
        installMock(WindowProxy, (mock) => WindowProxy.setInstance(mock));
    windowProxy.setResultFor('waitForLazyRender', Promise.resolve());
    windowProxy.setResultFor('createIframeSrc', '');
    windowProxy.setResultFor('now', new Date());
    windowProxy.setResultFor('url', url);
    windowProxy.setResultFor('matchMedia', {
      matches: false,
      addEventListener: () => {},
      removeEventListener: () => {},
      addListener: () => {},
      removeListener: () => {},
    });
    handler = installMock(
        PageHandlerRemote,
        mock => NewTabPageProxy.setInstance(mock, new PageCallbackRouter()));
    handler.setResultFor('getMostVisitedSettings', Promise.resolve({
      customLinksEnabled: false,
      shortcutsVisible: false,
    }));
    handler.setResultFor('getDoodle', Promise.resolve({
      doodle: null,
    }));
    handler.setResultFor('getModulesIdNames', Promise.resolve({data: []}));
    handler.setResultFor('getModulesEligibleForRemoval', Promise.resolve({
      moduleIds: [],
    }));
    handler.setResultFor('getModulesOrder', Promise.resolve({moduleIds: []}));
    handler.setResultFor(
        'canShowRealboxContextMenuAnimation', Promise.resolve({canShow: true}));
    backgroundManager = installMock(
        BackgroundManager, (mock) => BackgroundManager.setInstance(mock));
    backgroundManager.setResultFor(
        'getBackgroundImageLoadTime', Promise.resolve(backgroundImageLoadTime));
    moduleRegistry =
        installMock(ModuleRegistry, (mock) => ModuleRegistry.setInstance(mock));
    moduleResolver = new PromiseResolver();
    moduleRegistry.setResultFor('initializeModules', moduleResolver.promise);
    installMock(
        ComposeboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new ComposeboxPageCallbackRouter(),
            new SearchboxPageHandlerRemote(),
            new SearchboxPageCallbackRouter())));
    searchboxHandler = installMock(SearchboxPageHandlerRemote, mock => {
      ComposeboxProxyImpl.getInstance().searchboxHandler = mock;
      SearchboxBrowserProxy.getInstance().handler = mock;
    });
    searchboxHandler.setResultFor('getRecentTabs', Promise.resolve({tabs: []}));
    searchboxHandler.setResultFor('getInputState', Promise.resolve({
      state: {
        allowedModels: [],
        allowedTools: [],
        allowedInputTypes: [],
        activeModel: 0,
        activeTool: 0,
        disabledModels: [],
        disabledTools: [],
        disabledInputTypes: [],
        toolConfigs: [],
        modelConfigs: [],
      },
    }));
    searchboxHandler.setResultFor(
        'getPageClassification',
        Promise.resolve({metricSource: 'NTP_REALBOX'}));
    installMock(
        ActionChipsHandlerRemote, mock => ActionChipsApiProxyImpl.setInstance({
          getHandler: () => mock,
          getCallbackRouter: () => new ActionChipsPageCallbackRouter(),
        }));
  }

  async function createAndAppendApp() {
    app = document.createElement('ntp-app');
    document.body.appendChild(app);
    await microtasksFinished();
  }

  function setReducedMotionPreference(reducedMotionPreferred: boolean) {
    if (reducedMotionPreferred) {
      document.documentElement.style.setProperty(
          '--cr-animations-disabled', '1');
    } else {
      document.documentElement.style.removeProperty('--cr-animations-disabled');
    }
  }

  suite('Initialization', () => {
    test(
        'initializes as SPINNER_ONLY when reduced motion is not preferred',
        async () => {
          createSetup();
          setReducedMotionPreference(false);
          await createAndAppendApp();
          app.dispatchEvent(new CustomEvent(
              'action-chips-retrieval-state-changed',
              {detail: {state: ActionChipsRetrievalState.REQUESTED}}));
          await microtasksFinished();

          assertEquals(
              GlifAnimationState.SPINNER_ONLY,
              app.$.searchbox.contextMenuGlifAnimationState);
        });
  });

  suite('ReducedMotionScrim', () => {
    setup(createSetup);

    test(
        'scrim transition is none when reduced motion is preferred',
        async () => {
          setReducedMotionPreference(true);
          await createAndAppendApp();
          app.$.searchbox.dispatchEvent(new CustomEvent('open-composebox', {
            detail: {text: '', files: []},
          }));
          await microtasksFinished();

          const scrim = app.shadowRoot.querySelector('#scrim')!;
          assertStyle(scrim, 'transition-property', 'none');
        });

    test(
        'scrim transition is not none when reduced motion is not preferred',
        async () => {
          setReducedMotionPreference(false);
          await createAndAppendApp();
          app.$.searchbox.dispatchEvent(new CustomEvent('open-composebox', {
            detail: {text: '', files: []},
          }));
          await microtasksFinished();

          const scrim = app.shadowRoot.querySelector('#scrim')!;
          assertNotStyle(scrim, 'transition-property', 'none');
        });
  });

  suite('ReducedMotionModules', () => {
    setup(() => {
      loadTimeData.overrideValues({
        modulesEnabled: true,
      });
      createSetup();
    });

    [false, true].forEach(preferred => {
      test(
          `modules animation is ${
              preferred ? '' : 'not '}none when reduced motion is ${
              preferred ? '' : 'not '}preferred`,
          async () => {
            setReducedMotionPreference(preferred);
            await createAndAppendApp();

            const modules = app.shadowRoot.querySelector('ntp-modules')!;
            modules.dispatchEvent(
                new CustomEvent('modules-loaded', {detail: 1}));
            await microtasksFinished();

            const assertion_fn = preferred ? assertStyle : assertNotStyle;
            assertion_fn(
                app.shadowRoot.querySelector('#modules')!, 'animation-name',
                'none');
          });
    });
  });
});

suite('NewTabPageAppContextMenuAnimationTest', () => {
  let app: AppElement;
  let windowProxy: TestMock<WindowProxy>;
  let handler: TestMock<PageHandlerRemote>;

  function createSetup() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    windowProxy =
        installMock(WindowProxy, (mock) => WindowProxy.setInstance(mock));
    windowProxy.setResultFor('waitForLazyRender', Promise.resolve());
    windowProxy.setResultFor('createIframeSrc', '');
    windowProxy.setResultFor('now', new Date());
    windowProxy.setResultFor('url', new URL(location.href));
    windowProxy.setResultFor('matchMedia', {
      matches: false,
      addEventListener: () => {},
      removeEventListener: () => {},
      addListener: () => {},
      removeListener: () => {},
    });
    handler = installMock(
        PageHandlerRemote,
        mock => NewTabPageProxy.setInstance(mock, new PageCallbackRouter()));
    handler.setResultFor('getMostVisitedSettings', Promise.resolve({
      customLinksEnabled: false,
      shortcutsVisible: false,
    }));
    handler.setResultFor('getDoodle', Promise.resolve({
      doodle: null,
    }));
    handler.setResultFor('getModulesIdNames', Promise.resolve({data: []}));
    handler.setResultFor('getModulesEligibleForRemoval', Promise.resolve({
      moduleIds: [],
    }));
    handler.setResultFor('getModulesOrder', Promise.resolve({moduleIds: []}));
    handler.setResultFor(
        'canShowRealboxContextMenuAnimation', Promise.resolve({canShow: true}));
    installMock(
        BackgroundManager, (mock) => BackgroundManager.setInstance(mock));
    const moduleRegistry =
        installMock(ModuleRegistry, (mock) => ModuleRegistry.setInstance(mock));
    moduleRegistry.setResultFor(
        'initializeModules', new PromiseResolver().promise);
    installMock(
        ComposeboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new ComposeboxPageCallbackRouter(),
            new SearchboxPageHandlerRemote(),
            new SearchboxPageCallbackRouter())));
    const searchboxHandler = installMock(SearchboxPageHandlerRemote, mock => {
      ComposeboxProxyImpl.getInstance().searchboxHandler = mock;
      SearchboxBrowserProxy.getInstance().handler = mock;
    });
    searchboxHandler.setResultFor('getRecentTabs', Promise.resolve({tabs: []}));
    searchboxHandler.setResultFor('getInputState', Promise.resolve({
      state: {
        allowedModels: [],
        allowedTools: [],
        allowedInputTypes: [],
        activeModel: 0,
        activeTool: 0,
        disabledModels: [],
        disabledTools: [],
        disabledInputTypes: [],
        toolConfigs: [],
        modelConfigs: [],
      },
    }));
    searchboxHandler.setResultFor(
        'getPageClassification',
        Promise.resolve({metricSource: 'NTP_REALBOX'}));
    installMock(
        ActionChipsHandlerRemote, mock => ActionChipsApiProxyImpl.setInstance({
          getHandler: () => mock,
          getCallbackRouter: () => new ActionChipsPageCallbackRouter(),
        }));
  }

  async function createAndAppendApp() {
    app = document.createElement('ntp-app');
    document.body.appendChild(app);
    await microtasksFinished();
  }

  suite('CappingEnabled', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        ntpRealboxNextEnabled: true,
        ntpNextFeaturesEnabled: true,
        actionChipsEnabled: true,
        realboxContextMenuAnimationCappingEnabled: true,
      });
    });

    test('canShow is true and energy effect enabled', async () => {
      loadTimeData.overrideValues({
        energyEffectAnimationEnabled: true,
      });
      createSetup();
      handler.setResultFor(
          'canShowRealboxContextMenuAnimation',
          Promise.resolve({canShow: true}));
      await createAndAppendApp();

      assertEquals(
          GlifAnimationState.STARTED,
          app.$.searchbox.contextMenuGlifAnimationState);
      assertEquals(
          1,
          handler.getCallCount('recordRealboxContextMenuAnimationImpression'));
    });

    test('canShow is false and energy effect enabled', async () => {
      loadTimeData.overrideValues({
        energyEffectAnimationEnabled: true,
      });
      createSetup();
      handler.setResultFor(
          'canShowRealboxContextMenuAnimation',
          Promise.resolve({canShow: false}));
      await createAndAppendApp();

      assertEquals(
          GlifAnimationState.INELIGIBLE,
          app.$.searchbox.contextMenuGlifAnimationState);
      assertEquals(
          0,
          handler.getCallCount('recordRealboxContextMenuAnimationImpression'));
    });

    test('canShow is true and energy effect disabled', async () => {
      loadTimeData.overrideValues({
        energyEffectAnimationEnabled: false,
      });
      createSetup();
      handler.setResultFor(
          'canShowRealboxContextMenuAnimation',
          Promise.resolve({canShow: true}));
      await createAndAppendApp();

      assertEquals(
          GlifAnimationState.SPINNER_ONLY,
          app.$.searchbox.contextMenuGlifAnimationState);
      assertEquals(
          0,
          handler.getCallCount('recordRealboxContextMenuAnimationImpression'));

      const actionChips = app.shadowRoot.querySelector('ntp-action-chips');
      assertTrue(!!actionChips);
      actionChips.dispatchEvent(new CustomEvent(
          'action-chips-retrieval-state-changed',
          {detail: {state: ActionChipsRetrievalState.UPDATED}}));
      await microtasksFinished();
      assertEquals(
          GlifAnimationState.STARTED,
          app.$.searchbox.contextMenuGlifAnimationState);
      assertEquals(
          1,
          handler.getCallCount('recordRealboxContextMenuAnimationImpression'));
    });

    test('canShow is false and energy effect disabled', async () => {
      loadTimeData.overrideValues({
        energyEffectAnimationEnabled: false,
      });
      createSetup();
      handler.setResultFor(
          'canShowRealboxContextMenuAnimation',
          Promise.resolve({canShow: false}));
      await createAndAppendApp();

      assertEquals(
          GlifAnimationState.INELIGIBLE,
          app.$.searchbox.contextMenuGlifAnimationState);
      assertEquals(
          0,
          handler.getCallCount('recordRealboxContextMenuAnimationImpression'));
    });
  });

  suite('CappingDisabled', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        ntpRealboxNextEnabled: true,
        ntpNextFeaturesEnabled: true,
        actionChipsEnabled: true,
        realboxContextMenuAnimationCappingEnabled: false,
      });
    });

    test('energy effect enabled', async () => {
      loadTimeData.overrideValues({
        energyEffectAnimationEnabled: true,
      });
      createSetup();
      await createAndAppendApp();

      assertEquals(
          GlifAnimationState.STARTED,
          app.$.searchbox.contextMenuGlifAnimationState);
      assertEquals(
          0, handler.getCallCount('canShowRealboxContextMenuAnimation'));
      assertEquals(
          0,
          handler.getCallCount('recordRealboxContextMenuAnimationImpression'));
    });

    test('energy effect disabled', async () => {
      loadTimeData.overrideValues({
        energyEffectAnimationEnabled: false,
      });
      createSetup();
      await createAndAppendApp();

      assertEquals(
          GlifAnimationState.SPINNER_ONLY,
          app.$.searchbox.contextMenuGlifAnimationState);
      assertEquals(
          0, handler.getCallCount('canShowRealboxContextMenuAnimation'));
      assertEquals(
          0,
          handler.getCallCount('recordRealboxContextMenuAnimationImpression'));

      const actionChips = app.shadowRoot.querySelector('ntp-action-chips');
      assertTrue(!!actionChips);
      actionChips.dispatchEvent(new CustomEvent(
          'action-chips-retrieval-state-changed',
          {detail: {state: ActionChipsRetrievalState.UPDATED}}));
      await microtasksFinished();
      assertEquals(
          GlifAnimationState.STARTED,
          app.$.searchbox.contextMenuGlifAnimationState);
      assertEquals(
          0,
          handler.getCallCount('recordRealboxContextMenuAnimationImpression'));
    });
  });
});
