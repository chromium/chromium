// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://whats-new/whats_new_app.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {ModulePosition} from 'chrome://whats-new/whats_new.mojom-webui.js';
import {WhatsNewProxyImpl} from 'chrome://whats-new/whats_new_proxy.js';

import {TestWhatsNewBrowserProxy} from './test_whats_new_browser_proxy.js';

const whatsNewURL = 'chrome://webui-test/whats_new/test.html';

function getUrlForFixture(filename: string, query?: string): string {
  if (query) {
    return `chrome://webui-test/whats_new/${filename}.html?${query}`;
  }
  return `chrome://webui-test/whats_new/${filename}.html`;
}

suite('WhatsNewV2AppTest', function() {
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

  test('with module_impression metrics from embedded page', async () => {
    const proxy = new TestWhatsNewBrowserProxy(
        getUrlForFixture('test_with_metrics_module_impression_v2'));
    WhatsNewProxyImpl.setInstance(proxy);
    window.history.replaceState({}, '', '/');
    const whatsNewApp = document.createElement('whats-new-app');
    document.body.appendChild(whatsNewApp);

    const moduleImpression =
        await proxy.handler.whenCalled('recordModuleImpression');
    assertEquals('ChromeFeature', moduleImpression[0]);
    assertEquals(ModulePosition.kSpotlight1, moduleImpression[1]);
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
});
