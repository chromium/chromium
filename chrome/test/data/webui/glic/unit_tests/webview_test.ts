// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GuestPageType, WebviewController, WebviewPersistentState, ZoomAction} from 'chrome://glic/glic.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import type {CrA11yAnnouncerMessagesSentEvent} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {configureLoadTimeData, FakeBrowserProxy, FakeWebviewDelegate} from './test_helpers.js';

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

suite('WebviewLoadCommitTest', () => {
  let controller: WebviewController;
  let lastCommittedPageType: GuestPageType|undefined;
  let lastCommittedIsApiAllowed: boolean|undefined;

  setup(() => {
    configureLoadTimeData({
      glicGuestURL: 'https://cat.fun/party',
      glicApiAllowedOrigins: '',
      devMode: false,
    });

    const delegate = new FakeWebviewDelegate();
    delegate.webviewPageCommit = (pageType, isApiAllowed) => {
      lastCommittedPageType = pageType;
      lastCommittedIsApiAllowed = isApiAllowed;
    };

    const container = document.createElement('div');
    controller = new WebviewController(
        container,
        new FakeBrowserProxy(),
        delegate,
        new WebviewPersistentState(),
    );
  });

  test('login page committed reports login', () => {
    controller.onGuestNavigated(
        'https://accounts.google.com/signin', false, GuestPageType.kLogin,
        false);

    assertEquals(GuestPageType.kLogin, lastCommittedPageType);
    assertEquals(false, lastCommittedIsApiAllowed);
  });

  test('unauthorized page committed reports loadError', () => {
    controller.onGuestNavigated(
        'https://unknown.com/', false, GuestPageType.kRegular, false);

    assertEquals(GuestPageType.kLoadError, lastCommittedPageType);
    assertEquals(false, lastCommittedIsApiAllowed);
  });

  test('authorized regular page committed reports regular', () => {
    controller.onGuestNavigated(
        'https://cat.fun/party', true, GuestPageType.kRegular, false);

    assertEquals(GuestPageType.kRegular, lastCommittedPageType);
    assertEquals(true, lastCommittedIsApiAllowed);
  });
});
