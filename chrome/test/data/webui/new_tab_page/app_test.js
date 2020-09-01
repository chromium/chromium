// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, BackgroundManager, BackgroundSelectionType, BrowserProxy, ModuleRegistry, PromoBrowserCommandProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {isMac} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {assertNotStyle, assertStyle, createTestProxy, createTheme} from 'chrome://test/new_tab_page/test_support.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
import {eventToPromise, flushTasks} from 'chrome://test/test_util.m.js';

suite('NewTabPageAppTest', () => {
  /** @type {!AppElement} */
  let app;

  /**
   * @implements {BrowserProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  /**
   * @implements {BackgroundManager}
   * @extends {TestBrowserProxy}
   */
  let backgroundManager;

  /** @type {PromiseResolver} */
  let moduleResolver;

  suiteSetup(() => {
    loadTimeData.overrideValues({
      realboxEnabled: false,
    });
  });

  setup(async () => {
    PolymerTest.clearBody();

    testProxy = createTestProxy();
    testProxy.handler.setResultFor('getBackgroundCollections', Promise.resolve({
      collections: [],
    }));
    testProxy.handler.setResultFor('getChromeThemes', Promise.resolve({
      chromeThemes: [],
    }));
    testProxy.handler.setResultFor('getDoodle', Promise.resolve({
      doodle: null,
    }));
    testProxy.handler.setResultFor('getOneGoogleBarParts', Promise.resolve({
      parts: null,
    }));
    testProxy.setResultMapperFor('matchMedia', () => ({
                                                 addListener() {},
                                                 removeListener() {},
                                               }));
    testProxy.setResultFor('waitForLazyRender', Promise.resolve());
    BrowserProxy.instance_ = testProxy;
    backgroundManager = TestBrowserProxy.fromClass(BackgroundManager);
    backgroundManager.setResultFor(
        'getBackgroundImageLoadTime', Promise.resolve(0));
    BackgroundManager.instance_ = backgroundManager;
    const moduleRegistry = TestBrowserProxy.fromClass(ModuleRegistry);
    moduleResolver = new PromiseResolver();
    moduleRegistry.setResultFor('initializeModules', moduleResolver.promise);
    ModuleRegistry.instance_ = moduleRegistry;

    app = document.createElement('ntp-app');
    document.body.appendChild(app);
    await flushTasks();
  });

  test('customize dialog closed on start', () => {
    // Assert.
    assertFalse(!!app.shadowRoot.querySelector('ntp-customize-dialog'));
  });

  test('clicking customize button opens customize dialog', async () => {
    // Act.
    $$(app, '#customizeButton').click();
    await flushTasks();

    // Assert.
    assertTrue(!!app.shadowRoot.querySelector('ntp-customize-dialog'));
  });

  test('setting theme updates customize dialog', async () => {
    // Arrange.
    $$(app, '#customizeButton').click();
    const theme = createTheme();

    // Act.
    testProxy.callbackRouterRemote.setTheme(theme);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertDeepEquals(
        theme, app.shadowRoot.querySelector('ntp-customize-dialog').theme);
  });

  test('setting theme updates ntp', async () => {
    // Act.
    testProxy.callbackRouterRemote.setTheme(createTheme());
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertEquals(1, backgroundManager.getCallCount('setBackgroundColor'));
    assertEquals(
        0xffff0000 /* red */,
        (await backgroundManager.whenCalled('setBackgroundColor')).value);
    assertStyle(
        $$(app, '#content'), '--ntp-theme-shortcut-background-color',
        'rgba(0, 255, 0, 1)');
    assertStyle(
        $$(app, '#content'), '--ntp-theme-text-color', 'rgba(0, 0, 255, 1)');
    assertEquals(1, backgroundManager.getCallCount('setShowBackgroundImage'));
    assertFalse(await backgroundManager.whenCalled('setShowBackgroundImage'));
    assertStyle($$(app, '#backgroundImageAttribution'), 'display', 'none');
    assertStyle($$(app, '#backgroundImageAttribution2'), 'display', 'none');
    assertTrue($$(app, '#logo').doodleAllowed);
    assertFalse($$(app, '#logo').singleColored);
    assertFalse($$(app, '#logo').dark);
    assertEquals(0xffff0000, $$(app, '#logo').backgroundColor.value);
  });

  test('setting 3p theme shows attribution', async () => {
    // Arrange.
    const theme = createTheme();
    theme.backgroundImage = {
      url: {url: 'https://foo.com'},
      attributionUrl: {url: 'chrome://theme/foo'},
    };

    // Act.
    testProxy.callbackRouterRemote.setTheme(theme);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    assertNotStyle($$(app, '#themeAttribution'), 'display', 'none');
    assertEquals('chrome://theme/foo', $$(app, '#themeAttribution img').src);
  });

  test('realbox is not visible by default', async () => {
    // Assert.
    assertNotStyle($$(app, '#fakebox'), 'display', 'none');
    assertStyle($$(app, '#realbox'), 'display', 'none');
    assertStyle($$(app, '#realbox'), 'visibility', 'hidden');

    // Act.
    testProxy.callbackRouterRemote.setTheme(createTheme());
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertStyle($$(app, '#realbox'), 'visibility', 'visible');
  });

  test('open voice search event opens voice search overlay', async () => {
    // Act.
    $$(app, '#fakebox').dispatchEvent(new Event('open-voice-search'));
    await flushTasks();

    // Assert.
    assertTrue(!!app.shadowRoot.querySelector('ntp-voice-search-overlay'));
    assertEquals(
        newTabPage.mojom.VoiceSearchAction.kActivateSearchBox,
        await testProxy.handler.whenCalled('onVoiceSearchAction'));
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
    assertTrue(!!app.shadowRoot.querySelector('ntp-voice-search-overlay'));
    assertEquals(
        newTabPage.mojom.VoiceSearchAction.kActivateKeyboard,
        await testProxy.handler.whenCalled('onVoiceSearchAction'));

    // Test other shortcut doesn't close voice search.
    // Act
    window.dispatchEvent(new KeyboardEvent('keydown', {
      ctrlKey: true,
      shiftKey: true,
      code: 'Enter',
    }));
    await flushTasks();

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
      await flushTasks();

      // Assert.
      assertTrue(!!app.shadowRoot.querySelector('ntp-voice-search-overlay'));
    });
  }

  [true, false].forEach(themeModeDoodlesEnabled => {
    const allows = themeModeDoodlesEnabled ? 'allows' : 'disallows';
    test(`setting background image shows image, ${allows} doodle`, async () => {
      // Arrange.
      loadTimeData.overrideValues({themeModeDoodlesEnabled});
      const theme = createTheme();
      theme.backgroundImage = {url: {url: 'https://img.png'}};

      // Act.
      backgroundManager.resetResolver('setShowBackgroundImage');
      testProxy.callbackRouterRemote.setTheme(theme);
      await testProxy.callbackRouterRemote.$.flushForTesting();

      // Assert.
      assertEquals(1, backgroundManager.getCallCount('setShowBackgroundImage'));
      assertTrue(await backgroundManager.whenCalled('setShowBackgroundImage'));
      assertNotStyle(
          $$(app, '#backgroundImageAttribution'), 'text-shadow', 'none');
      assertEquals(1, backgroundManager.getCallCount('setBackgroundImage'));
      assertEquals(
          'https://img.png',
          (await backgroundManager.whenCalled('setBackgroundImage')).url.url);
      assertEquals(null, $$(app, '#logo').backgroundColor);
      if (themeModeDoodlesEnabled) {
        assertTrue($$(app, '#logo').doodleAllowed);
      } else {
        assertFalse($$(app, '#logo').doodleAllowed);
      }
    });

    test(`setting non-default theme ${allows} doodle`, async function() {
      // Arrange.
      const theme = createTheme();
      theme.type = newTabPage.mojom.ThemeType.kChrome;

      // Act.
      testProxy.callbackRouterRemote.setTheme(theme);
      await testProxy.callbackRouterRemote.$.flushForTesting();

      // Assert.
      if (themeModeDoodlesEnabled) {
        assertTrue($$(app, '#logo').doodleAllowed);
      } else {
        assertFalse($$(app, '#logo').doodleAllowed);
      }
    });
  });

  test('setting attributions shows attributions', async function() {
    // Arrange.
    const theme = createTheme();
    theme.backgroundImageAttribution1 = 'foo';
    theme.backgroundImageAttribution2 = 'bar';
    theme.backgroundImageAttributionUrl = {url: 'https://info.com'};

    // Act.
    testProxy.callbackRouterRemote.setTheme(theme);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertNotStyle($$(app, '#backgroundImageAttribution'), 'display', 'none');
    assertNotStyle($$(app, '#backgroundImageAttribution2'), 'display', 'none');
    assertEquals(
        'https://info.com',
        $$(app, '#backgroundImageAttribution').getAttribute('href'));
    assertEquals(
        'foo', $$(app, '#backgroundImageAttribution1').textContent.trim());
    assertEquals(
        'bar', $$(app, '#backgroundImageAttribution2').textContent.trim());
  });

  test('setting logo color colors logo', async function() {
    // Arrange.
    const theme = createTheme();
    theme.logoColor = {value: 0xffff0000};

    // Act.
    testProxy.callbackRouterRemote.setTheme(theme);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertTrue($$(app, '#logo').singleColored);
    assertStyle($$(app, '#logo'), '--ntp-logo-color', 'rgba(255, 0, 0, 1)');
  });

  test('preview background image', async () => {
    const theme = createTheme();
    theme.backgroundImage = {url: {url: 'https://example.com/image.png'}};
    theme.backgroundImageAttribution1 = 'foo';
    theme.backgroundImageAttribution2 = 'bar';
    theme.backgroundImageAttributionUrl = {url: 'https://info.com'};
    testProxy.callbackRouterRemote.setTheme(theme);
    await testProxy.callbackRouterRemote.$.flushForTesting();
    assertEquals(backgroundManager.getCallCount('setBackgroundImage'), 1);
    assertEquals(
        'https://example.com/image.png',
        (await backgroundManager.whenCalled('setBackgroundImage')).url.url);
    assertEquals(
        'https://info.com/', $$(app, '#backgroundImageAttribution').href);
    assertEquals('foo', $$(app, '#backgroundImageAttribution1').innerText);
    assertEquals('bar', $$(app, '#backgroundImageAttribution2').innerText);
    $$(app, '#customizeButton').click();
    await flushTasks();
    const dialog = app.shadowRoot.querySelector('ntp-customize-dialog');
    backgroundManager.resetResolver('setBackgroundImage');
    dialog.backgroundSelection = {
      type: BackgroundSelectionType.IMAGE,
      image: {
        attribution1: '1',
        attribution2: '2',
        attributionUrl: {url: 'https://example.com'},
        imageUrl: {url: 'https://example.com/other.png'},
      },
    };
    assertEquals(1, backgroundManager.getCallCount('setBackgroundImage'));
    assertEquals(
        'https://example.com/other.png',
        (await backgroundManager.whenCalled('setBackgroundImage')).url.url);
    assertEquals(
        'https://example.com/', $$(app, '#backgroundImageAttribution').href);
    assertEquals('1', $$(app, '#backgroundImageAttribution1').innerText);
    assertEquals('2', $$(app, '#backgroundImageAttribution2').innerText);
    assertFalse($$(app, '#backgroundImageAttribution2').hidden);

    backgroundManager.resetResolver('setBackgroundImage');
    dialog.backgroundSelection = {type: BackgroundSelectionType.NO_SELECTION};
    assertEquals(1, backgroundManager.getCallCount('setBackgroundImage'));
    assertEquals(
        'https://example.com/image.png',
        (await backgroundManager.whenCalled('setBackgroundImage')).url.url);
  });

  test('theme update when dialog closed clears selection', async () => {
    const theme = createTheme();
    theme.backgroundImage = {url: {url: 'https://example.com/image.png'}};
    testProxy.callbackRouterRemote.setTheme(theme);
    await testProxy.callbackRouterRemote.$.flushForTesting();
    assertEquals(1, backgroundManager.getCallCount('setBackgroundImage'));
    assertEquals(
        'https://example.com/image.png',
        (await backgroundManager.whenCalled('setBackgroundImage')).url.url);
    $$(app, '#customizeButton').click();
    await flushTasks();
    const dialog = app.shadowRoot.querySelector('ntp-customize-dialog');
    backgroundManager.resetResolver('setBackgroundImage');
    dialog.backgroundSelection = {
      type: BackgroundSelectionType.IMAGE,
      image: {
        attribution1: '1',
        attribution2: '2',
        attributionUrl: {url: 'https://example.com'},
        imageUrl: {url: 'https://example.com/other.png'},
      },
    };
    assertEquals(1, backgroundManager.getCallCount('setBackgroundImage'));
    assertEquals(
        'https://example.com/other.png',
        (await backgroundManager.whenCalled('setBackgroundImage')).url.url);
    backgroundManager.resetResolver('setBackgroundImage');
    dialog.dispatchEvent(new Event('close'));
    assertEquals(0, backgroundManager.getCallCount('setBackgroundImage'));
    backgroundManager.resetResolver('setBackgroundImage');
    testProxy.callbackRouterRemote.setTheme(theme);
    await testProxy.callbackRouterRemote.$.flushForTesting();
    assertEquals(2, backgroundManager.getCallCount('setBackgroundImage'));
    assertEquals(
        'https://example.com/image.png',
        (await backgroundManager.whenCalled('setBackgroundImage')).url.url);
  });

  test('theme updates add shortcut color', async () => {
    const theme = createTheme();
    theme.shortcutUseWhiteAddIcon = true;
    testProxy.callbackRouterRemote.setTheme(theme);
    assertFalse(app.$.mostVisited.hasAttribute('use-white-add-icon'));
    await testProxy.callbackRouterRemote.$.flushForTesting();
    assertTrue(app.$.mostVisited.hasAttribute('use-white-add-icon'));
  });

  test('theme updates use title pill', async () => {
    const theme = createTheme();
    theme.shortcutUseTitlePill = true;
    testProxy.callbackRouterRemote.setTheme(theme);
    assertFalse(app.$.mostVisited.hasAttribute('use-title-pill'));
    await testProxy.callbackRouterRemote.$.flushForTesting();
    assertTrue(app.$.mostVisited.hasAttribute('use-title-pill'));
  });

  test('executes promo browser command', async () => {
    const testProxy = PromoBrowserCommandProxy.getInstance();
    testProxy.handler = TestBrowserProxy.fromClass(
        promoBrowserCommand.mojom.CommandHandlerRemote);
    testProxy.handler.setResultFor(
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
        await testProxy.handler.whenCalled('executeCommand');
    // Unsupported commands get resolved to the default command before being
    // sent to the browser.
    assertEquals(
        promoBrowserCommand.mojom.Command.kUnknownCommand, expectedCommandId);
    assertEquals(clickInfo, expectedClickInfo);

    // Make sure the promo frame gets notified whether the command was executed.
    const {data: commandExecuted} = await eventToPromise('message', window);
    assertTrue(commandExecuted);
  });

  suite('modules', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        modulesEnabled: true,
      });
    });

    test('modules appended to page', async () => {
      // Act.
      moduleResolver.resolve([
        {
          id: 'foo',
          name: 'Foo',
          element: document.createElement('div'),
          title: 'Foo Title',
        },
        {
          id: 'bar',
          name: 'Bar',
          element: document.createElement('div'),
          title: 'Bar Title',
        }
      ]);
      await flushTasks();  // Wait for module descriptor resolution.
      $$(app, '#modules').render();

      // Assert.
      const modules = app.shadowRoot.querySelectorAll('ntp-module-wrapper');
      assertEquals(2, modules.length);
    });
  });
});
