// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {matcherForOrigin, urlMatchesAllowedOrigin, urlMatchesApiAllowedOrigin, WebviewController, WebviewPersistentState, ZoomAction} from 'chrome://glic/glic.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import type {CrA11yAnnouncerMessagesSentEvent} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {configureLoadTimeData, FakeApiHostEmbedder, FakeBrowserProxy, FakeWebviewDelegate} from './test_helpers.js';

suite('urlMatchesAllowedOriginTest', () => {
  setup(() => {
    configureLoadTimeData();
  });

  function assertUrlMatchesAllowedOrigin(expectMatches: boolean, url: string) {
    const urlObj = new URL(url);
    assertEquals(
        expectMatches, urlMatchesAllowedOrigin(urlObj),
        `urlMatchesAllowedOrigin("${url}")`);
  }

  test('matcherForOrigin works', () => {
    assertFalse(!!matcherForOrigin(''));
    assertFalse(!!matcherForOrigin('fun'));
    assertFalse(!!matcherForOrigin('cat.fun'));

    let result = matcherForOrigin('https://cat.fun');
    assertTrue(!!result);
    assertEquals('cat.fun', result?.hostname);
    assertEquals('https', result?.protocol);

    result = matcherForOrigin('http://cat.fun');
    assertTrue(!!result);
    assertEquals('cat.fun', result?.hostname);
    assertEquals('http', result?.protocol);

    result = matcherForOrigin('http://cat.fun:42');
    assertTrue(!!result);
    assertEquals('cat.fun', result?.hostname);
    assertEquals('http', result?.protocol);
    assertEquals('42', result?.port);

    result = matcherForOrigin('http://cat.fun:*');
    assertTrue(!!result);
    assertEquals('cat.fun', result?.hostname);
    assertEquals('http', result?.protocol);
    assertEquals('*', result?.port);

    result = matcherForOrigin('http://cat.fun/://foo://');
    assertTrue(!!result);
    assertEquals('cat.fun', result?.hostname);
    assertEquals('http', result?.protocol);
  });

  test('allows the primary url', () => {
    loadTimeData.overrideValues({
      glicAllowedOrigins: '',
      glicGuestURL: 'https://cat.fun/party',
    });
    assertUrlMatchesAllowedOrigin(true, 'https://cat.fun/party');
    assertUrlMatchesAllowedOrigin(true, 'https://cat.fun/disaster');
    assertUrlMatchesAllowedOrigin(true, 'https://cat.fun/');
    assertUrlMatchesAllowedOrigin(false, 'https://dog.fun/');
    assertUrlMatchesAllowedOrigin(false, 'http://cat.fun/');
  });

  test('allows allowed origins', () => {
    loadTimeData.overrideValues({
      glicAllowedOrigins: 'https://*.mouse.org https://dog.com',
      glicGuestURL: 'https://cat.fun/party',
    });

    assertUrlMatchesAllowedOrigin(true, 'https://sub.mouse.org/party');
    assertUrlMatchesAllowedOrigin(true, 'https://inner.sub.mouse.org/party');
    assertUrlMatchesAllowedOrigin(false, 'https://mouse.org');
    assertUrlMatchesAllowedOrigin(false, 'https://amouse.org');

    assertUrlMatchesAllowedOrigin(true, 'https://dog.com/party');
    assertUrlMatchesAllowedOrigin(true, 'https://dog.com:99/party');
    assertUrlMatchesAllowedOrigin(false, 'http://dog.com/party');
  });

  test('allows api allowed origins', () => {
    loadTimeData.overrideValues({
      glicAllowedOrigins: 'https://dog.com',
      glicApiAllowedOrigins: 'https://*.mouse.org',
      glicGuestURL: 'https://cat.fun/party',
    });

    assertUrlMatchesAllowedOrigin(true, 'https://sub.mouse.org/party');
  });

  test('allows http', () => {
    loadTimeData.overrideValues({
      glicAllowedOrigins: '',
      glicGuestURL: 'http://test.com',
    });

    assertUrlMatchesAllowedOrigin(true, 'http://test.com');
    assertUrlMatchesAllowedOrigin(false, 'https://test.com');
    assertUrlMatchesAllowedOrigin(false, 'http://other.com');
  });
});

suite('urlMatchesApiAllowedOriginTest', () => {
  setup(() => {
    configureLoadTimeData();
  });

  function assertUrlMatchesApiAllowedOrigin(
      expectMatches: boolean, url: string) {
    assertEquals(
        expectMatches, urlMatchesApiAllowedOrigin(new URL(url)),
        `urlMatchesApiAllowedOrigin("${url}")`);
  }

  test('allows guest origin', () => {
    loadTimeData.overrideValues({
      glicGuestURL: 'https://cat.fun/party',
      glicApiAllowedOrigins: '',
      devMode: false,
    });
    assertUrlMatchesApiAllowedOrigin(true, 'https://cat.fun/party');
    assertUrlMatchesApiAllowedOrigin(true, 'https://cat.fun/disaster');
    assertUrlMatchesApiAllowedOrigin(true, 'https://cat.fun/');
    assertUrlMatchesApiAllowedOrigin(false, 'https://dog.fun/');
  });

  test('allows api allowed origins', () => {
    loadTimeData.overrideValues({
      glicGuestURL: 'https://cat.fun/party',
      glicApiAllowedOrigins: 'https://*.mouse.org https://dog.com',
      devMode: false,
    });
    assertUrlMatchesApiAllowedOrigin(true, 'https://sub.mouse.org/party');
    assertUrlMatchesApiAllowedOrigin(true, 'https://inner.sub.mouse.org/party');
    assertUrlMatchesApiAllowedOrigin(false, 'https://mouse.org');
    assertUrlMatchesApiAllowedOrigin(true, 'https://dog.com/party');
    assertUrlMatchesApiAllowedOrigin(false, 'http://dog.com/party');
  });

  test('devMode bypasses checks', () => {
    loadTimeData.overrideValues({
      glicGuestURL: 'https://cat.fun/party',
      glicApiAllowedOrigins: '',
      devMode: true,
    });
    assertUrlMatchesApiAllowedOrigin(true, 'https://cat.fun/party');
    assertUrlMatchesApiAllowedOrigin(true, 'https://dog.fun/');
  });

  test('handles null origin', () => {
    loadTimeData.overrideValues({
      glicGuestURL: 'https://cat.fun/party',
      glicApiAllowedOrigins: 'https://dog.com',
      devMode: true,
    });
    // A URL with 'null' origin should not be allowed even in devMode
    const nullOriginUrl = new URL('data:text/html,hello');
    assertFalse(urlMatchesApiAllowedOrigin(nullOriginUrl));
  });
});


suite('WebviewZoomTest', () => {
  let controller: WebviewController;

  setup(() => {
    configureLoadTimeData();

    // Set up mock interfaces to enable creating a WebviewController for test
    // use.
    const container = document.createElement('div');

    controller = new WebviewController(
        container,
        new FakeBrowserProxy(),
        new FakeWebviewDelegate(),
        new FakeApiHostEmbedder(),
        new WebviewPersistentState(),
    );
  });

  test('ZoomInReturnsNextZoomFactor', () => {
    let lastSetZoom = 1.0;
    const webview = controller.webview;
    webview.getZoom = (cb: (z: number) => void) => cb(lastSetZoom);
    webview.setZoom = (z: number) => {
      lastSetZoom = z;
    };

    controller.zoom(ZoomAction.kZoomIn);
    assertEquals(1.1, lastSetZoom);

    controller.zoom(ZoomAction.kZoomIn);
    assertEquals(1.25, lastSetZoom);
  });

  test('ZoomOutReturnsPreviousZoomFactor', () => {
    let lastSetZoom = 1.25;
    const webview = controller.webview;
    webview.getZoom = (cb: (z: number) => void) => cb(lastSetZoom);
    webview.setZoom = (currentZoom: number) => {
      lastSetZoom = currentZoom;
    };

    controller.zoom(ZoomAction.kZoomOut);
    assertEquals(1.1, lastSetZoom);

    controller.zoom(ZoomAction.kZoomOut);
    assertEquals(1.0, lastSetZoom);
  });

  test('ZoomResetReturnsOne', () => {
    let lastSetZoom = 1.5;
    const webview = controller.webview;
    webview.getZoom = (cb: (z: number) => void) => cb(lastSetZoom);
    webview.setZoom = (currentZoom: number) => {
      lastSetZoom = currentZoom;
    };

    controller.zoom(ZoomAction.kReset);
    assertEquals(1.0, lastSetZoom);
  });

  test('ZoomBoundaryConditions', () => {
    let lastSetZoom = 2.0;
    let setZoomCalled = false;
    const webview = controller.webview;
    webview.getZoom = (cb: (z: number) => void) => cb(lastSetZoom);
    webview.setZoom = (currentZoom: number) => {
      lastSetZoom = currentZoom;
      setZoomCalled = true;
    };

    // At 2.0, ZoomIn action should not result in a call to setZoom.
    controller.zoom(ZoomAction.kZoomIn);
    assertFalse(setZoomCalled);

    // At 1.0, ZoomOut action should not result in a call to setZoom.
    lastSetZoom = 1.0;
    setZoomCalled = false;
    controller.zoom(ZoomAction.kZoomOut);
    assertFalse(setZoomCalled);
  });

  test('ZoomImperfectFloatingPointFactor', () => {
    let lastSetZoom = 1.09;
    const webview = controller.webview;
    webview.getZoom = (cb: (z: number) => void) => cb(lastSetZoom);
    webview.setZoom = (z: number) => {
      lastSetZoom = z;
    };

    // From 1.09 (e.g. Android float rounding around 1.1), ZoomOut should step
    // down to 1.0.
    controller.zoom(ZoomAction.kZoomOut);
    assertEquals(1.0, lastSetZoom);

    // From 1.09, ZoomIn should step up to 1.1 (and not skip to 1.25).
    lastSetZoom = 1.09;
    controller.zoom(ZoomAction.kZoomIn);
    assertEquals(1.1, lastSetZoom);
  });

  test('ZoomAnnouncementMade', async () => {
    const announcementPromise =
        eventToPromise<CrA11yAnnouncerMessagesSentEvent>(
            'cr-a11y-announcer-messages-sent', document.body);

    // Simulate a zoom change to 125%
    const zoomEvent = Object.assign(
                          new Event('zoomchange'),
                          {oldZoomFactor: 1.0, newZoomFactor: 1.25}) as
        chrome.webviewTag.ZoomChangeEvent;
    controller.webview.dispatchEvent(zoomEvent);

    const event = await announcementPromise;
    assertDeepEquals(event.detail.messages, ['Zoom: 125%']);
  });

  test('ZoomChangeEventUpdatesPageHandler', () => {
    let notifiedZoomFactor: number|undefined;
    const fakeProxy = new FakeBrowserProxy();
    fakeProxy.pageHandler.onZoomLevelChange = (zoomFactor: number) => {
      notifiedZoomFactor = zoomFactor;
    };

    const container = document.createElement('div');
    const customController = new WebviewController(
        container,
        fakeProxy,
        new FakeWebviewDelegate(),
        new FakeApiHostEmbedder(),
        new WebviewPersistentState(),
    );

    const zoomEvent = Object.assign(
                          new Event('zoomchange'),
                          {oldZoomFactor: 1.0, newZoomFactor: 1.5}) as
        chrome.webviewTag.ZoomChangeEvent;
    customController.webview.dispatchEvent(zoomEvent);

    assertEquals(1.5, notifiedZoomFactor);
  });

  test('ZoomWithDeviceDisplayScaleMultiplier', () => {
    // Simulate a device with a 1.09x display scale factor (e.g. Android high
    // DPI).
    const DISPLAY_SCALE = 1.09;
    let configuredZoomFactor = 1.0;

    const webview = controller.webview;
    webview.getZoom = (cb: (z: number) => void) => {
      // webview.getZoom() returns configuredZoomFactor * DISPLAY_SCALE
      cb(configuredZoomFactor * DISPLAY_SCALE);
    };
    webview.setZoom = (z: number) => {
      const oldZoomFactor = configuredZoomFactor;
      configuredZoomFactor = z;
      // Dispatch zoomchange event with the exact configuredZoomFactor
      const zoomEvent =
          Object.assign(
              new Event('zoomchange'), {oldZoomFactor, newZoomFactor: z}) as
          chrome.webviewTag.ZoomChangeEvent;
      webview.dispatchEvent(zoomEvent);
    };

    // Initialize display scale multiplier via a zoomchange event
    const initEvent = Object.assign(
                          new Event('zoomchange'),
                          {oldZoomFactor: 1.0, newZoomFactor: 1.0}) as
        chrome.webviewTag.ZoomChangeEvent;
    webview.dispatchEvent(initEvent);

    // Zoom in from 1.0 -> 1.1
    controller.zoom(ZoomAction.kZoomIn);
    assertEquals(1.1, configuredZoomFactor);

    // Zoom in from 1.1 -> 1.25
    controller.zoom(ZoomAction.kZoomIn);
    assertEquals(1.25, configuredZoomFactor);

    // Zoom out from 1.25 -> 1.1 (Verify ZoomOut does not get trapped at 1.25!)
    controller.zoom(ZoomAction.kZoomOut);
    assertEquals(1.1, configuredZoomFactor);

    // Zoom out from 1.1 -> 1.0
    controller.zoom(ZoomAction.kZoomOut);
    assertEquals(1.0, configuredZoomFactor);
  });
});

suite('GlicThemeTest', () => {
  setup(() => {
    const link = document.createElement('link');
    link.rel = 'stylesheet';
    link.href = 'chrome://theme/colors.css?sets=chrome';
    document.body.appendChild(link);
  });

  test('ColorsStylesheetRefreshesOnColorProviderChanged', async () => {
    const updater = ColorChangeUpdater.forDocument();

    // Trigger color provider change callback
    await updater.onColorProviderChanged();

    // Verify the stylesheet refreshed itself with a version parameter
    const link = document.querySelector<HTMLLinkElement>(
        'link[href*="//theme/colors.css"]');
    assertTrue(!!link);
    const params =
        new URLSearchParams(new URL(link.href, location.href).search);
    assertTrue(params.has('version'));
    assertEquals(params.get('sets'), 'chrome');
  });

  teardown(() => {
    const link = document.querySelector('link[href*="//theme/colors.css"]');
    if (link) {
      link.remove();
    }
  });
});
