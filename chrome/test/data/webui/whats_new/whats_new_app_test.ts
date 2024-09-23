// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CommandHandlerRemote} from 'chrome://resources/js/browser_command.mojom-webui.js';
import {BrowserCommandProxy} from 'chrome://resources/js/browser_command/browser_command_proxy.js';
import {isChromeOS} from 'chrome://resources/js/platform.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';
import {ModulePosition, ScrollDepth} from 'chrome://whats-new/whats_new.mojom-webui.js';
import {formatModuleName} from 'chrome://whats-new/whats_new_app.js';
import {WhatsNewProxyImpl} from 'chrome://whats-new/whats_new_proxy.js';

import {TestWhatsNewBrowserProxy} from './test_whats_new_browser_proxy.js';

const whatsNewURL = 'chrome://webui-test/whats_new/test.html';

function getUrlForFixture(filename: string, query?: string): string {
  if (query) {
    return `chrome://webui-test/whats_new/${filename}.html?${query}`;
  }
  return `chrome://webui-test/whats_new/${filename}.html`;
}

suite('WhatsNewAppTest', function() {
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('with query parameters', async () => {
    const proxy = new TestWhatsNewBrowserProxy(whatsNewURL);
    WhatsNewProxyImpl.setInstance(proxy);
    window.history.replaceState({}, '', '?auto=true');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);
    await proxy.handler.whenCalled('getServerUrl');
    await microtasksFinished();

    const iframe =
        whatsNewApp.shadowRoot!.querySelector<HTMLIFrameElement>('#content');
    assertTrue(!!iframe);
    // iframe has latest=true URL query parameter except on CrOS
    assertEquals(
        whatsNewURL + (isChromeOS ? '?latest=false' : '?latest=true'),
        iframe.src);
  });

  test('with version as query parameter', async () => {
    const proxy = new TestWhatsNewBrowserProxy(whatsNewURL + '?version=m98');
    WhatsNewProxyImpl.setInstance(proxy);
    window.history.replaceState({}, '', '?auto=true');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);
    await proxy.handler.whenCalled('getServerUrl');
    await microtasksFinished();

    const iframe =
        whatsNewApp.shadowRoot!.querySelector<HTMLIFrameElement>('#content');
    assertTrue(!!iframe);
    // iframe has latest=true URL query parameter except on CrOS
    assertEquals(
        whatsNewURL + '?version=m98' +
            (isChromeOS ? '&latest=false' : '&latest=true'),
        iframe.src);
  });

  test('no query parameters', async () => {
    const proxy = new TestWhatsNewBrowserProxy(whatsNewURL);
    WhatsNewProxyImpl.setInstance(proxy);
    window.history.replaceState({}, '', '/');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);
    await proxy.handler.whenCalled('getServerUrl');
    await microtasksFinished();

    const iframe =
        whatsNewApp.shadowRoot!.querySelector<HTMLIFrameElement>('#content');
    assertTrue(!!iframe);
    assertEquals(whatsNewURL + '?latest=false', iframe.src);
  });

  test('with legacy command format', async () => {
    const proxy = new TestWhatsNewBrowserProxy(
        getUrlForFixture('test_with_legacy_command_3'));
    WhatsNewProxyImpl.setInstance(proxy);
    const browserCommandHandler = TestMock.fromClass(CommandHandlerRemote);
    BrowserCommandProxy.getInstance().handler = browserCommandHandler;
    browserCommandHandler.setResultFor(
        'canExecuteCommand', Promise.resolve({canExecute: true}));
    window.history.replaceState({}, '', '/');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);

    const whenMessage = eventToPromise('message', window);
    const commandId =
        await browserCommandHandler.whenCalled('canExecuteCommand');
    assertEquals(3, commandId);

    const {data} = await whenMessage;
    assertEquals(3, data.data.commandId);

    await proxy.handler.whenCalled('recordBrowserCommandExecuted');
    assertEquals(1, proxy.handler.getCallCount('recordBrowserCommandExecuted'));
  });

  test('with browser command format', async () => {
    const proxy =
        new TestWhatsNewBrowserProxy(getUrlForFixture('test_with_command_4'));
    WhatsNewProxyImpl.setInstance(proxy);
    const browserCommandHandler = TestMock.fromClass(CommandHandlerRemote);
    BrowserCommandProxy.getInstance().handler = browserCommandHandler;
    browserCommandHandler.setResultFor(
        'canExecuteCommand', Promise.resolve({canExecute: true}));
    window.history.replaceState({}, '', '/');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);

    const whenMessage = eventToPromise('message', window);
    const commandId =
        await browserCommandHandler.whenCalled('canExecuteCommand');
    assertEquals(4, commandId);

    const {data} = await whenMessage;
    assertEquals('browser_command', data.data.event);
    assertEquals(4, data.data.commandId);

    await proxy.handler.whenCalled('recordBrowserCommandExecuted');
    assertEquals(1, proxy.handler.getCallCount('recordBrowserCommandExecuted'));
  });

  test('with page_load metrics from embedded page', async () => {
    const proxy = new TestWhatsNewBrowserProxy(
        getUrlForFixture('test_with_metrics_page_loaded'));
    WhatsNewProxyImpl.setInstance(proxy);
    window.history.replaceState({}, '', '/');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);

    const isAutoOpen =
        await proxy.handler.whenCalled('recordVersionPageLoaded');
    assertEquals(false, isAutoOpen);

    const contentLoadedCallCount =
        proxy.handler.getCallCount('recordTimeToLoadContent');
    assertEquals(1, contentLoadedCallCount);
  });

  test('with module_impression metrics from embedded page', async () => {
    const proxy = new TestWhatsNewBrowserProxy(
        getUrlForFixture('test_with_metrics_module_impression'));
    WhatsNewProxyImpl.setInstance(proxy);
    window.history.replaceState({}, '', '/');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);

    const moduleImpression =
        await proxy.handler.whenCalled('recordModuleImpression');
    assertEquals('ChromeFeature', moduleImpression[0]);
    assertEquals(ModulePosition.kUndefined, moduleImpression[1]);
  });

  test('with explore_more_toggled metrics from embedded page', async () => {
    const proxy = new TestWhatsNewBrowserProxy(
        getUrlForFixture('test_with_metrics_explore_more_toggled'));
    WhatsNewProxyImpl.setInstance(proxy);
    window.history.replaceState({}, '', '/');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);

    let expanded = await proxy.handler.whenCalled('recordExploreMoreToggled');
    assertEquals(true, expanded);
    await proxy.handler.resetResolver('recordExploreMoreToggled');
    expanded = await proxy.handler.whenCalled('recordExploreMoreToggled');
    assertEquals(false, expanded);
  });

  test('with scroll_depth metrics from embedded page', async () => {
    const proxy = new TestWhatsNewBrowserProxy(
        getUrlForFixture('test_with_metrics_scroll_depth'));
    WhatsNewProxyImpl.setInstance(proxy);
    window.history.replaceState({}, '', '/');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);

    const percentage = await proxy.handler.whenCalled('recordScrollDepth');
    assertEquals(ScrollDepth.k25, percentage);
  });

  test('with time_on_page metrics from embedded page', async () => {
    const proxy = new TestWhatsNewBrowserProxy(
        getUrlForFixture('test_with_metrics_time_on_page'));
    WhatsNewProxyImpl.setInstance(proxy);
    window.history.replaceState({}, '', '/');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);

    const timeOnPage = await proxy.handler.whenCalled('recordTimeOnPage');
    // 3 million microseconds = 3 thousand milliseconds
    assertEquals(3n * 1000n * 1000n, timeOnPage.microseconds);
  });

  test('with module_click metrics from embedded page', async () => {
    const proxy = new TestWhatsNewBrowserProxy(
        getUrlForFixture('test_with_metrics_module_click'));
    WhatsNewProxyImpl.setInstance(proxy);
    window.history.replaceState({}, '', '/');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);

    const clickedModule =
        await proxy.handler.whenCalled('recordModuleLinkClicked');
    assertEquals('FeatureWithLink', clickedModule[0]);
    assertEquals(ModulePosition.kSpotlight1, clickedModule[1]);
  });

  test('with different module name formats', async () => {
    // Formats legacy format correctly.
    assertEquals('ChromeFeature', formatModuleName('123-chrome-feature'));
    // Ignores modern format.
    assertEquals('ChromeFeature', formatModuleName('ChromeFeature'));

    // Edge-cases
    // Ignores when no hyphens present.
    assertEquals('chrome', formatModuleName('chrome'));
    // Ignores when starts with numbers, but does not contain hyphens.
    assertEquals('123feature', formatModuleName('123feature'));
    // Works when starts or ends with hyphen
    assertEquals('Feature', formatModuleName('feature-'));
    assertEquals('Feature', formatModuleName('-feature'));
    // Does not remove numbers within name.
    assertEquals('Chrome123Feature', formatModuleName('chrome-123-feature'));
  });
});
