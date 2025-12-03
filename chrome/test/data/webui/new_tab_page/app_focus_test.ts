// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/new_tab_page.js';

import {CustomizeButtonsDocumentCallbackRouter, CustomizeButtonsHandlerRemote} from 'chrome://new-tab-page/customize_buttons.mojom-webui.js';
import type {Module} from 'chrome://new-tab-page/lazy_load.js';
import {ComposeboxProxyImpl, ModuleRegistry} from 'chrome://new-tab-page/lazy_load.js';
import type {AppElement, SearchboxElement} from 'chrome://new-tab-page/new_tab_page.js';
import {$$, BackgroundManager, CustomizeButtonsProxy, NewTabPageProxy, SearchboxBrowserProxy, WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import type {PageRemote} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertStyle, createTheme, installMock} from './test_support.js';

suite('NewTabPageAppFocusTest', () => {
  let app: AppElement;
  let windowProxy: TestMock<WindowProxy>;
  let handler: TestMock<PageHandlerRemote>;
  let moduleRegistry: TestMock<ModuleRegistry>;
  let moduleResolver: PromiseResolver<Module[]>;
  let backgroundManager: TestMock<BackgroundManager>;
  let callbackRouterRemote: PageRemote;
  let searchboxHandler: TestMock<SearchboxPageHandlerRemote>;

  const url: URL = new URL(location.href);
  const backgroundImageLoadTime: number = 123;

  suiteSetup(() => {
    loadTimeData.overrideValues({
      searchboxShowComposeEntrypoint: true,
      searchboxShowComposebox: true,
      ntpRealboxNextEnabled: true,
    });
  });

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    windowProxy = installMock(WindowProxy);
    windowProxy.setResultMapperFor('matchMedia', () => ({
                                                   addListener() {},
                                                   addEventListener() {},
                                                   removeListener() {},
                                                   removeEventListener() {},
                                                 }));
    windowProxy.setResultFor('waitForLazyRender', Promise.resolve());
    windowProxy.setResultFor('createIframeSrc', '');
    windowProxy.setResultFor('url', url);
    fakeMetricsPrivate();
    backgroundManager = installMock(BackgroundManager);
    backgroundManager.setResultFor(
        'getBackgroundImageLoadTime', Promise.resolve(backgroundImageLoadTime));
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
    installMock(
        CustomizeButtonsHandlerRemote,
        mock => CustomizeButtonsProxy.setInstance(
            mock, new CustomizeButtonsDocumentCallbackRouter()));
    CustomizeButtonsProxy.getInstance()
        .callbackRouter.$.bindNewPipeAndPassRemote();
    callbackRouterRemote = NewTabPageProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
    moduleRegistry = installMock(ModuleRegistry);
    moduleResolver = new PromiseResolver();
    moduleRegistry.setResultFor('initializeModules', moduleResolver.promise);
    searchboxHandler = installMock(SearchboxPageHandlerRemote, mock => {
      ComposeboxProxyImpl.getInstance().searchboxHandler = mock;
      SearchboxBrowserProxy.getInstance().handler = mock;
    });
    searchboxHandler.setResultFor('getRecentTabs', Promise.resolve({tabs: []}));

    app = document.createElement('ntp-app');
    document.body.appendChild(app);
    await microtasksFinished();
  });


  function getScrim(): HTMLElement|null {
    return app.shadowRoot.querySelector<HTMLElement>('#scrim');
  }

  test('A scrim is applied when the focus is on searchbox input', async () => {
    // Arrange.
    callbackRouterRemote.setTheme(createTheme());
    await callbackRouterRemote.$.flushForTesting();

    const scrim = getScrim();
    assertTrue(!!scrim);
    assertTrue(scrim.hidden);
    const searchbox = $$<SearchboxElement>(app, '#searchbox');
    assertTrue(!!searchbox);
    await microtasksFinished();
    assertStyle($$(app, '#searchbox')!, 'visibility', 'visible');

    const input = searchbox.shadowRoot.getElementById('input');
    assertTrue(!!input);
    input.focus();
    await microtasksFinished();
    assertFalse(scrim.hidden);

    input.blur();
    await microtasksFinished();
    assertTrue(scrim.hidden);
  });
});
