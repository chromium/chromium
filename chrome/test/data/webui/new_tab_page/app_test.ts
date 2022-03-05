// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {counterfactualLoad, Module, ModuleDescriptor, ModuleRegistry} from 'chrome://new-tab-page/lazy_load.js';
import {$$, AppElement, BackgroundManager, BrowserCommandProxy, CustomizeDialogPage, NewTabPageProxy, NtpElement, VoiceAction, WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {PageCallbackRouter, PageHandlerRemote, PageInterface} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {Command, CommandHandlerRemote} from 'chrome://resources/js/browser_command/browser_command.mojom-webui.js';
import {isMac} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise, flushTasks} from 'chrome://webui-test/test_util.js';

import {fakeMetricsPrivate, MetricsTracker} from './metrics_test_support.js';
import {assertNotStyle, assertStyle, createBackgroundImage, createTheme, installMock} from './test_support.js';

suite('NewTabPageAppTest', () => {
  let app: AppElement;
  let windowProxy: TestBrowserProxy;
  let handler: TestBrowserProxy;
  let callbackRouterRemote: PageHandlerRemote&PageInterface;
  let metrics: MetricsTracker;
  let moduleRegistry: TestBrowserProxy;
  let backgroundManager: TestBrowserProxy;
  let moduleResolver: PromiseResolver<Module[]>;

  let url: URL = new URL(location.href);

  setup(async () => {
    document.body.innerHTML = '';

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
    handler.setResultFor('getPromo', Promise.resolve({promo: null}));
    windowProxy.setResultMapperFor('matchMedia', () => ({
                                                   addListener() {},
                                                   removeListener() {},
                                                 }));
    windowProxy.setResultFor('waitForLazyRender', Promise.resolve());
    windowProxy.setResultFor('createIframeSrc', '');
    windowProxy.setResultFor('url', url);
    callbackRouterRemote = NewTabPageProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
    backgroundManager = installMock(BackgroundManager);
    backgroundManager.setResultFor(
        'getBackgroundImageLoadTime', Promise.resolve(0));
    moduleRegistry = installMock(ModuleRegistry);
    moduleResolver = new PromiseResolver();
    moduleRegistry.setResultFor('getDescriptors', []);
    moduleRegistry.setResultFor('initializeModules', moduleResolver.promise);
    metrics = fakeMetricsPrivate();

    app = document.createElement('ntp-app');
    document.body.appendChild(app);
    await flushTasks();
  });

  suite('misc', () => {
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
    });

    test('logs height', async () => {
      // Assert.
      assertEquals(1, metrics.count('NewTabPage.Height'));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.Height',
              Math.floor(document.documentElement.clientHeight)));
    });

    test('realbox is not visible by default', async () => {
      // Assert.
      assertStyle($$(app, '#realbox')!, 'visibility', 'hidden');

      // Act.
      callbackRouterRemote.setTheme(createTheme());
      await callbackRouterRemote.$.flushForTesting();

      // Assert.
      assertStyle($$(app, '#realbox')!, 'visibility', 'visible');
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
              'NewTabPage.VoiceActions', VoiceAction.kActivateSearchBox));
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
              'NewTabPage.VoiceActions', VoiceAction.kActivateKeyboard));

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
  });

  suite('theming', () => {
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
    });

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
          $$(app, '#content')!, '--ntp-theme-text-color', 'rgba(0, 0, 255, 1)');
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
      assertNotStyle(
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
      assertStyle(app.$.logo, '--ntp-logo-color', 'rgba(255, 0, 0, 1)');
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

    test('theme updates use title pill', async () => {
      const theme = createTheme();
      theme.mostVisited.useTitlePill = true;
      callbackRouterRemote.setTheme(theme);
      const mostVisited = $$(app, '#mostVisited');
      assertTrue(!!mostVisited);
      assertFalse(mostVisited.hasAttribute('use-title-pill_'));
      await callbackRouterRemote.$.flushForTesting();
      assertTrue(mostVisited.hasAttribute('use-title-pill_'));
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
  });

  suite('promo', () => {
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

  suite('clicks', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        modulesEnabled: true,
      });
    });

    ([
      ['#content', NtpElement.kBackground],
      ['ntp-logo', NtpElement.kLogo],
      ['ntp-realbox', NtpElement.kRealbox],
      ['cr-most-visited', NtpElement.kMostVisited],
      ['ntp-middle-slot-promo', NtpElement.kMiddleSlotPromo],
      ['ntp-modules', NtpElement.kModule],
    ] as [string, NtpElement][])
        .forEach(([selector, element]) => {
          test(`clicking '${selector}' records click`, () => {
            // Act.
            $$<HTMLElement>(app, selector)!.click();

            // Assert.
            assertEquals(1, metrics.count('NewTabPage.Click'));
            assertEquals(1, metrics.count('NewTabPage.Click', element));
          });
        });

    test('clicking customize records click', () => {
      // Act.
      $$<HTMLElement>(app, '#customizeButton')!.click();
      app.$.customizeDialogIf.render();
      $$<HTMLElement>(app, 'ntp-customize-dialog')!.click();

      // Assert.
      assertEquals(2, metrics.count('NewTabPage.Click'));
      assertEquals(2, metrics.count('NewTabPage.Click', NtpElement.kCustomize));
    });

    test('clicking OGB records click', () => {
      // Act.
      window.dispatchEvent(new MessageEvent('message', {
        data: {
          frameType: 'one-google-bar',
          messageType: 'click',
        }
      }));

      // Assert.
      assertEquals(1, metrics.count('NewTabPage.Click'));
      assertEquals(
          1, metrics.count('NewTabPage.Click', NtpElement.kOneGoogleBar));
    });
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

    test('promo and modules coordinate', async () => {
      // Arrange.
      loadTimeData.overrideValues({navigationStartTime: 0.0});
      windowProxy.setResultFor('now', 123.0);
      const middleSlotPromo = $$(app, 'ntp-middle-slot-promo');
      assertTrue(!!middleSlotPromo);
      const modules = $$(app, 'ntp-modules');
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
  });

  suite('counterfactual modules', () => {
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
          descriptor: new ModuleDescriptor(
              'foo', 'foo', () => Promise.resolve(fooElement)),
          element: fooElement,
        },
        {
          descriptor: new ModuleDescriptor(
              'bar', 'bar', () => Promise.resolve(barElement)),
          element: barElement,
        }
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

  suite('customize URL', () => {
    suiteSetup(() => {
      // We inject the URL param in this suite setup so that the URL is updated
      // before the app element gets created.
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
