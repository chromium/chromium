// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CommandHandlerRemote} from 'chrome://resources/js/browser_command.mojom-webui.js';
import {BrowserCommandProxy} from 'chrome://resources/js/browser_command/browser_command_proxy.js';
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
    assertEquals(whatsNewURL + '?updated=true', iframe.src);
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
    assertEquals(whatsNewURL + '?version=m98&updated=true', iframe.src);
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
    assertEquals(whatsNewURL + '?updated=false', iframe.src);
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
    assertEquals(ModulePosition.kSpotlight1, moduleImpression[1]);
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

  test('with different module name formats', () => {
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

  test('with video metrics from embedded page', async () => {
    const proxy = new TestWhatsNewBrowserProxy(
        getUrlForFixture('test_with_metrics_module_video_events'));
    WhatsNewProxyImpl.setInstance(proxy);
    window.history.replaceState({}, '', '/');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);

    const videoStarted =
        await proxy.handler.whenCalled('recordModuleVideoStarted');
    assertEquals('ChromeFeature', videoStarted[0]);
    assertEquals(ModulePosition.kSpotlight1, videoStarted[1]);

    const videoEnded = await proxy.handler.whenCalled('recordModuleVideoEnded');
    assertEquals('ChromeVideoEndFeature', videoEnded[0]);
    assertEquals(ModulePosition.kSpotlight3, videoEnded[1]);

    const playClicked =
        await proxy.handler.whenCalled('recordModulePlayClicked');
    assertEquals('ChromeVideoFeature', playClicked[0]);
    assertEquals(ModulePosition.kSpotlight1, playClicked[1]);

    const pauseClicked =
        await proxy.handler.whenCalled('recordModulePauseClicked');
    assertEquals('ChromeVideoFeature', pauseClicked[0]);
    assertEquals(ModulePosition.kSpotlight2, pauseClicked[1]);

    const restartClicked =
        await proxy.handler.whenCalled('recordModuleRestartClicked');
    assertEquals('ChromeVideoFeature', restartClicked[0]);
    assertEquals(ModulePosition.kSpotlight3, restartClicked[1]);
  });

  test('with qr_code_toggled metrics from embedded page', async () => {
    const proxy = new TestWhatsNewBrowserProxy(
        getUrlForFixture('test_with_metrics_qr_code_toggled'));
    WhatsNewProxyImpl.setInstance(proxy);
    window.history.replaceState({}, '', '/');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);

    let expanded = await proxy.handler.whenCalled('recordQrCodeToggled');
    assertEquals(true, expanded);
    await proxy.handler.resetResolver('recordQrCodeToggled');
    expanded = await proxy.handler.whenCalled('recordQrCodeToggled');
    assertEquals(false, expanded);
  });

  test('with expand_media_toggled metrics from embedded page', async () => {
    const proxy = new TestWhatsNewBrowserProxy(
        getUrlForFixture('test_with_metrics_expand_media_toggled'));
    WhatsNewProxyImpl.setInstance(proxy);
    window.history.replaceState({}, '', '/');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);

    let expandedMedia =
        await proxy.handler.whenCalled('recordExpandMediaToggled');
    assertEquals('ChromeFeature', expandedMedia[0]);
    assertEquals(true, expandedMedia[1]);
    await proxy.handler.resetResolver('recordExpandMediaToggled');
    expandedMedia = await proxy.handler.whenCalled('recordExpandMediaToggled');
    assertEquals('ChromeFeature', expandedMedia[0]);
    assertEquals(false, expandedMedia[1]);
  });

  test('with next button click metric from embedded page', async () => {
    const proxy = new TestWhatsNewBrowserProxy(
        getUrlForFixture('test_with_metrics_next_button_click'));
    WhatsNewProxyImpl.setInstance(proxy);
    window.history.replaceState({}, '', '/');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);

    await proxy.handler.whenCalled('recordNextButtonClick');
    assertEquals(1, proxy.handler.getCallCount('recordNextButtonClick'));
  });

  test('with nav_click metric from embedded page', async () => {
    const proxy = new TestWhatsNewBrowserProxy(
        getUrlForFixture('test_with_metrics_nav_click'));
    WhatsNewProxyImpl.setInstance(proxy);
    window.history.replaceState({}, '', '/');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);

    await proxy.handler.whenCalled('recordNavClick');
    assertEquals(1, proxy.handler.getCallCount('recordNavClick'));
  });

  test('with feature_tile_navigation metric from embedded page', async () => {
    const proxy = new TestWhatsNewBrowserProxy(
        getUrlForFixture('test_with_metrics_feature_tile_navigation'));
    WhatsNewProxyImpl.setInstance(proxy);
    window.history.replaceState({}, '', '/');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);

    await proxy.handler.whenCalled('recordFeatureTileNavigation');
    assertEquals(1, proxy.handler.getCallCount('recordFeatureTileNavigation'));
  });

  test(
      'with carousel_scroll_button_click metric from embedded page',
      async () => {
        const proxy = new TestWhatsNewBrowserProxy(
            getUrlForFixture('test_with_metrics_carousel_scroll_button_click'));
        WhatsNewProxyImpl.setInstance(proxy);
        window.history.replaceState({}, '', '/');
        const whatsNewApp = document.createElement('whats-new-app');
        document.body.appendChild(whatsNewApp);

        await proxy.handler.whenCalled('recordCarouselScrollButtonClick');
        assertEquals(
            1, proxy.handler.getCallCount('recordCarouselScrollButtonClick'));
      });

  test('with cta_click metric from embedded page', async () => {
    const proxy = new TestWhatsNewBrowserProxy(
        getUrlForFixture('test_with_metrics_cta_click'));
    WhatsNewProxyImpl.setInstance(proxy);
    window.history.replaceState({}, '', '/');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);

    await proxy.handler.whenCalled('recordCtaClick');
    assertEquals(1, proxy.handler.getCallCount('recordCtaClick'));
  });
});
