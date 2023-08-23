// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {counterfactualLoad, LensUploadDialogElement, Module, ModuleDescriptor, ModuleRegistry} from 'chrome://new-tab-page/lazy_load.js';
import {$$, AppElement, BackgroundManager, BrowserCommandProxy, CUSTOMIZE_CHROME_BUTTON_ELEMENT_ID, CustomizeDialogPage, NewTabPageProxy, NtpCustomizeChromeEntryPoint, NtpElement, VoiceAction, WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {CustomizeChromeSection, NtpBackgroundImageSource, PageCallbackRouter, PageHandlerRemote, PageRemote} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {Command, CommandHandlerRemote} from 'chrome://resources/js/browser_command.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {assertNotStyle, assertStyle, createBackgroundImage, createTheme, installMock} from './test_support.js';

suite('NewTabPageAppTest', () => {
  let app: AppElement;
  let windowProxy: TestMock<WindowProxy>;
  let handler: TestMock<PageHandlerRemote>;
  let callbackRouterRemote: PageRemote;
  let metrics: MetricsTracker;
  let moduleRegistry: TestMock<ModuleRegistry>;
  let backgroundManager: TestMock<BackgroundManager>;
  let moduleResolver: PromiseResolver<Module[]>;

  const url: URL = new URL(location.href);
  const backgroundImageLoadTime: number = 123;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    windowProxy = installMock(WindowProxy);
    handler = installMock(
        PageHandlerRemote,
        mock => NewTabPageProxy.setInstance(mock, new PageCallbackRouter()));
    handler.setResultFor('getMostVisitedSettings', Promise.resolve({
      customLinksEnabled: false,
      shortcutsVisible: false,
    }));
    handler.setResultFor('getBackgroundCollections', Promise.resolve({
      collections: [],
    }));
    handler.setResultFor('getDoodle', Promise.resolve({
      doodle: null,
    }));
    handler.setResultFor('getModulesIdNames', Promise.resolve({data: []}));
    windowProxy.setResultMapperFor('matchMedia', () => ({
                                                   addListener() {},
                                                   addEventListener() {},
                                                   removeListener() {},
                                                   removeEventListener() {},
                                                 }));
    windowProxy.setResultFor('waitForLazyRender', Promise.resolve());
    windowProxy.setResultFor('createIframeSrc', '');
    windowProxy.setResultFor('url', url);
    callbackRouterRemote = NewTabPageProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
    backgroundManager = installMock(BackgroundManager);
    backgroundManager.setResultFor(
        'getBackgroundImageLoadTime', Promise.resolve(backgroundImageLoadTime));
    moduleRegistry = installMock(ModuleRegistry);
    moduleResolver = new PromiseResolver();
    moduleRegistry.setResultFor('initializeModules', moduleResolver.promise);
    metrics = fakeMetricsPrivate();

    app = document.createElement('ntp-app');
    document.body.appendChild(app);
    await flushTasks();
  });

  suite('Misc', () => {
    test('customize dialog closed on start', () => {
      // Assert.
      assertFalse(!!app.shadowRoot!.querySelector('ntp-customize-dialog'));
    });

    test('logs height', async () => {
      // Assert.
      assertEquals(1, metrics.count('NewTabPage.Height'));
      assertEquals(
          1,
          metrics.count('NewTabPage.Height', Math.floor(window.innerHeight)));
    });

    test('logs width', async () => {
      // Assert.
      assertEquals(1, metrics.count('NewTabPage.Width'));
      assertEquals(
          1, metrics.count('NewTabPage.Width', Math.floor(window.innerWidth)));
    });

    test('open voice search event opens voice search overlay', async () => {
      // Act.
      $$(app, '#realbox')!.dispatchEvent(new Event('open-voice-search'));
      await flushTasks();

      // Assert.
      assertTrue(!!app.shadowRoot!.querySelector('ntp-voice-search-overlay'));
      assertEquals(1, metrics.count('NewTabPage.VoiceActions'));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.VoiceActions', VoiceAction.ACTIVATE_SEARCH_BOX));
    });

    test('voice search keyboard shortcut', async () => {
      // Test correct shortcut opens voice search.
      // Act.
      window.dispatchEvent(new KeyboardEvent('keydown', {
        ctrlKey: true,
        shiftKey: true,
        code: 'Period',
      }));
      await flushTasks();

      // Assert.
      assertTrue(!!app.shadowRoot!.querySelector('ntp-voice-search-overlay'));
      assertEquals(1, metrics.count('NewTabPage.VoiceActions'));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.VoiceActions', VoiceAction.ACTIVATE_KEYBOARD));

      // Test other shortcut doesn't close voice search.
      // Act
      window.dispatchEvent(new KeyboardEvent('keydown', {
        ctrlKey: true,
        shiftKey: true,
        code: 'Enter',
      }));
      await flushTasks();

      // Assert.
      assertTrue(!!app.shadowRoot!.querySelector('ntp-voice-search-overlay'));
    });

    if (isMac) {
      test('keyboard shortcut opens voice search overlay on mac', async () => {
        // Act.
        window.dispatchEvent(new KeyboardEvent('keydown', {
          metaKey: true,
          shiftKey: true,
          code: 'Period',
        }));
        await flushTasks();

        // Assert.
        assertTrue(!!app.shadowRoot!.querySelector('ntp-voice-search-overlay'));
      });
    }

    test('help bubble can correctly find anchor elements', async () => {
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
      callbackRouterRemote.setCustomizeChromeSidePanelVisibility(true);
      await callbackRouterRemote.$.flushForTesting();

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
      const theme = createTheme(true);
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
      const theme = createTheme(true);
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
      assertFalse(app.$.logo.dark);
      assertEquals(0xffff0000, app.$.logo.backgroundColor.value);
    });

    test('setting 3p theme shows attribution', async () => {
      // Arrange.
      const theme = createTheme();
      theme.backgroundImage = createBackgroundImage('https://foo.com');
      theme.backgroundImage.attributionUrl = {url: 'chrome://theme/foo'};

      // Act.
      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();

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
          (await backgroundManager.whenCalled('setBackgroundImage')).url.url);
      assertEquals(null, app.$.logo.backgroundColor);
    });

    test('setting attributions shows attributions', async function() {
      // Arrange.
      const theme = createTheme();
      theme.backgroundImageAttribution1 = 'foo';
      theme.backgroundImageAttribution2 = 'bar';
      theme.backgroundImageAttributionUrl = {url: 'https://info.com'};

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
          'foo', $$(app, '#backgroundImageAttribution1')!.textContent!.trim());
      assertEquals(
          'bar', $$(app, '#backgroundImageAttribution2')!.textContent!.trim());
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
            const theme = createTheme(isDark);
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

      suite('background image load', () => {
        suiteSetup(() => {
          loadTimeData.overrideValues({backgroundImageUrl: 'https://foo.com'});
        });

        test('background image load time is logged', async () => {
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
      promoBrowserCommandHandler.setResultFor(
          'canExecuteCommand', Promise.resolve({canExecute: true}));

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
      promoBrowserCommandHandler.setResultFor(
          'executeCommand', Promise.resolve({commandExecuted: true}));

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
      ['ntp-realbox', NtpElement.REALBOX],
      ['cr-most-visited', NtpElement.MOST_VISITED],
      ['ntp-middle-slot-promo', NtpElement.MIDDLE_SLOT_PROMO],
      ['ntp-modules', NtpElement.MODULE],
      ['#customizeButton', NtpElement.CUSTOMIZE_BUTTON],
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
      modules.dispatchEvent(new Event('modules-loaded'));

      // Assert.
      assertNotStyle(middleSlotPromo, 'display', 'none');
      assertNotStyle(modules, 'display', 'none');
      assertEquals(1, metrics.count('NewTabPage.Modules.ShownTime'));
      assertEquals(1, metrics.count('NewTabPage.Modules.ShownTime', 123));
    });
  }

  suite('Modules', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        modulesEnabled: true,
        modulesRedesignedEnabled: false,
        wideModulesEnabled: false,
      });
    });

    [560, 672, 768].forEach(pageWidth => {
      test(
          `module width defaults to search box width rule applied for width: ${
              pageWidth}px`,
          () => {
            document.body.setAttribute('style', `width:${pageWidth}px`);
            const middleSlotPromo = $$(app, 'ntp-middle-slot-promo')!;
            middleSlotPromo.dispatchEvent(
                new Event('ntp-middle-slot-promo-loaded'));
            const modules = $$(app, 'ntp-modules')!;
            modules.dispatchEvent(new Event('modules-loaded'));
            const searchBoxWidth =
                window.getComputedStyle(app)
                    .getPropertyValue('--ntp-search-box-width')
                    .trim();

            assertStyle(modules, 'width', `${searchBoxWidth}`);
          });
    });

    test('modules max width media rule applied', async () => {
      const sampleMaxWidthPx = 768;
      loadTimeData.overrideValues({wideModulesEnabled: true});
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      document.body.setAttribute('style', `width:${sampleMaxWidthPx}px`);
      app = document.createElement('ntp-app');
      document.body.appendChild(app);
      await flushTasks();

      const middleSlotPromo = $$(app, 'ntp-middle-slot-promo')!;
      middleSlotPromo.dispatchEvent(new Event('ntp-middle-slot-promo-loaded'));
      const modules = $$(app, 'ntp-modules')!;
      modules.dispatchEvent(new Event('modules-loaded'));

      assertStyle(modules, 'width', `${sampleMaxWidthPx}px`);
    });

    modulesCommonTests('ntp-modules');
  });

  suite('V2Modules', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        modulesEnabled: true,
        modulesRedesignedEnabled: true,
      });
    });

    test('container is hidden', async () => {
      const modules = $$(app, 'ntp-modules-v2')!;
      assertTrue(!!modules);
      assertStyle(modules, 'display', 'none');
    });

    test('modules redesigned attribute applied', async () => {
      assertTrue(app.hasAttribute('modules-redesigned-enabled_'));
    });

    test(`clicking records click`, () => {
      // Act.
      $$<HTMLElement>(app, 'ntp-modules-v2')!.click();

      // Assert.
      assertEquals(1, metrics.count('NewTabPage.Click'));
      assertEquals(1, metrics.count('NewTabPage.Click', NtpElement.MODULE));
    });

    modulesCommonTests('ntp-modules-v2');
  });

  suite('v2 modules', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        modulesEnabled: true,
        modulesRedesignedEnabled: true,
      });
    });

    test('container is hidden', async () => {
      const modules = $$(app, 'ntp-modules-v2')!;
      assertTrue(!!modules);
      assertStyle(modules, 'display', 'none');
    });

    test(`clicking records click`, () => {
      // Act.
      $$<HTMLElement>(app, 'ntp-modules-v2')!.click();

      // Assert.
      assertEquals(1, metrics.count('NewTabPage.Click'));
      assertEquals(1, metrics.count('NewTabPage.Click', NtpElement.MODULE));
    });
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
      await flushTasks();

      // Assert.
      assertTrue(moduleRegistry.getCallCount('initializeModules') > 0);
      assertEquals(1, handler.getCallCount('onModulesLoadedWithData'));
      assertEquals(
          0, app.shadowRoot!.querySelectorAll('ntp-module-wrapper').length);
    });
  });

  suite('CustomizeDialog', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        customizeChromeEnabled: false,
      });
    });

    test('customize dialog closed on start', () => {
      // Assert.
      assertFalse(!!app.shadowRoot!.querySelector('ntp-customize-dialog'));
    });

    test('clicking customize button opens customize dialog', async () => {
      // Act.
      $$<HTMLElement>(app, '#customizeButton')!.click();
      await flushTasks();

      // Assert.
      assertTrue(!!app.shadowRoot!.querySelector('ntp-customize-dialog'));
      assertEquals(
          'true',
          $$<HTMLElement>(
              app, '#customizeButton')!.getAttribute('aria-pressed'));
      assertEquals(1, metrics.count('NewTabPage.Click'));
      assertEquals(
          1, metrics.count('NewTabPage.Click', NtpElement.CUSTOMIZE_BUTTON));

      // Act.
      $$<HTMLElement>(app, 'ntp-customize-dialog')!.click();

      // Assert.
      assertEquals(2, metrics.count('NewTabPage.Click'));
      assertEquals(
          1, metrics.count('NewTabPage.Click', NtpElement.CUSTOMIZE_DIALOG));
    });

    test('setting theme updates customize dialog', async () => {
      // Arrange.
      $$<HTMLElement>(app, '#customizeButton')!.click();
      const theme = createTheme();

      // TypeScript definitions for Mojo are not perfect, and the following
      // fields are incorrectly marked as non-optional and non-nullable, when
      // in reality they are optional and nullable.
      // TODO(crbug.com/1002798): Remove ignore statements if/when proper Mojo
      // TS support is added.
      // @ts-ignore:next-line
      theme.backgroundImage = null;
      // @ts-ignore:next-line
      theme.backgroundImageAttributionUrl = null;
      // @ts-ignore:next-line
      theme.logoColor = null;

      // Act.
      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();

      // Assert.
      assertDeepEquals(
          theme, app.shadowRoot!.querySelector('ntp-customize-dialog')!.theme);
      assertEquals(
          'true',
          $$<HTMLElement>(
              app, '#customizeButton')!.getAttribute('aria-pressed'));
    });

    suite('modules', () => {
      suiteSetup(() => {
        loadTimeData.overrideValues({
          modulesEnabled: true,
        });
      });
      test('modules can open customize dialog', async () => {
        // Act.
        $$(app, 'ntp-modules')!.dispatchEvent(new Event('customize-module'));
        app.$.customizeDialogIf.render();

        // Assert.
        assertTrue(!!$$(app, 'ntp-customize-dialog'));
        assertEquals(
            CustomizeDialogPage.MODULES,
            $$(app, 'ntp-customize-dialog')!.selectedPage);
      });
    });

    suite('customize URL', () => {
      suiteSetup(() => {
        // We inject the URL param in this suite setup so that the URL is
        // updated before the app element gets created.
        url.searchParams.append('customize', CustomizeDialogPage.THEMES);
      });

      test('URL opens customize dialog', () => {
        // Act.
        app.$.customizeDialogIf.render();

        // Assert.
        assertTrue(!!$$(app, 'ntp-customize-dialog'));
        assertEquals(
            CustomizeDialogPage.THEMES,
            $$(app, 'ntp-customize-dialog')!.selectedPage);
      });
    });
  });

  suite('CustomizeChromeSidePanel', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        customizeChromeEnabled: true,
      });
    });

    test('customize chrome in product help might show on startup'), () => {
      assertEquals(
          1, handler.getCallCount('maybeShowCustomizeChromeFeaturePromo'));
    };

    test('clicking customize button opens side panel', () => {
      // Act.
      $$<HTMLElement>(app, '#customizeButton')!.click();

      // Assert.
      assertDeepEquals(
          [true, CustomizeChromeSection.kUnspecified],
          handler.getArgs('setCustomizeChromeSidePanelVisible')[0]);
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.CustomizeChromeOpened',
              NtpCustomizeChromeEntryPoint.CUSTOMIZE_BUTTON));
      assertEquals(
          1, handler.getCallCount('incrementCustomizeChromeButtonOpenCount'));
    });

    test('clicking customize button hides side panel', async () => {
      // Act.
      callbackRouterRemote.setCustomizeChromeSidePanelVisibility(true);
      assertEquals(
          0,
          metrics.count(
              'NewTabPage.CustomizeChromeOpened',
              NtpCustomizeChromeEntryPoint.CUSTOMIZE_BUTTON));
      await callbackRouterRemote.$.flushForTesting();
      $$<HTMLElement>(app, '#customizeButton')!.click();

      // Assert.
      assertDeepEquals(
          [false, CustomizeChromeSection.kUnspecified],
          handler.getArgs('setCustomizeChromeSidePanelVisible')[0]);
      assertEquals(
          0,
          metrics.count(
              'NewTabPage.CustomizeChromeOpened',
              NtpCustomizeChromeEntryPoint.CUSTOMIZE_BUTTON));
      assertEquals(
          0, handler.getCallCount('incrementCustomizeChromeButtonOpenCount'));
    });

    test('clicking customize button is accessible', async () => {
      callbackRouterRemote.setCustomizeChromeSidePanelVisibility(true);
      await callbackRouterRemote.$.flushForTesting();
      assertEquals(
          'true',
          $$<HTMLElement>(
              app, '#customizeButton')!.getAttribute('aria-pressed'));
      callbackRouterRemote.setCustomizeChromeSidePanelVisibility(false);
      await callbackRouterRemote.$.flushForTesting();
      assertEquals(
          'false',
          $$<HTMLElement>(
              app, '#customizeButton')!.getAttribute('aria-pressed'));
    });

    suite('modules', () => {
      suiteSetup(() => {
        loadTimeData.overrideValues({
          modulesEnabled: true,
        });
      });

      test('modules can open side panel', async () => {
        // Act.
        $$(app, 'ntp-modules')!.dispatchEvent(new Event('customize-module'));

        // Assert.
        assertDeepEquals(
            [true, CustomizeChromeSection.kModules],
            handler.getArgs('setCustomizeChromeSidePanelVisible')[0]);
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
            [true, CustomizeChromeSection.kAppearance],
            handler.getArgs('setCustomizeChromeSidePanelVisible')[0]);
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
        realboxLensSearch: true,
      });
    });

    test('lens upload dialog closed on start', () => {
      // Assert.
      assertFalse(!!app.shadowRoot!.querySelector('ntp-lens-upload-dialog'));
    });

    test('realbox is not visible when Lens upload dialog is open', async () => {
      // Arrange.
      callbackRouterRemote.setTheme(createTheme());
      await callbackRouterRemote.$.flushForTesting();

      // Act.
      $$(app, '#realbox')!.dispatchEvent(new Event('open-lens-search'));
      await flushTasks();

      // Assert.
      assertTrue(!!app.shadowRoot!.querySelector('ntp-lens-upload-dialog'));
      assertStyle($$(app, '#realbox')!, 'visibility', 'hidden');

      // Act.
      (app.shadowRoot!.querySelector(LensUploadDialogElement.is) as
       LensUploadDialogElement)
          .closeDialog();
      await flushTasks();

      // Assert.
      assertStyle($$(app, '#realbox')!, 'visibility', 'visible');
    });
  });
});
