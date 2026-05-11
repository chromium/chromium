// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//glic/shared/guest_view/slim_webview.js';

import {OnBeforeSendHeadersParams, OriginCheckParams} from '//glic/shared/guest_view/request_throttlers.js';
import {PermissionRequestEvent} from '//glic/shared/guest_view/slim_webview.js';
import type {LoadAbortEvent, LoadEvent, NewWindowEvent, SlimWebviewElement} from '//glic/shared/guest_view/slim_webview.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

declare global {
  interface Window {
    canGetLocation: boolean;
    testServerUrl: string;
    crossOriginUrl: string;
  }
}

async function isFulfilled(p: Promise<any>) {
  const PENDING = Symbol();
  try {
    // If 'p' is already settled, it wins.
    // If 'p' is pending, the immediate rejection wins.
    await Promise.race([p, Promise.reject(PENDING)]);
    return true;
  } catch (err) {
    if (err === PENDING) {
      return false;
    }
    throw err;  // The original promise actually rejected
  }
}

function getTestUrl(url: string): string {
  const baseUrl = window.testServerUrl;
  assertTrue(baseUrl.length > 0);
  const resolved = URL.parse(url, baseUrl);
  assertTrue(resolved !== null);
  return resolved.href;
}

function getCrossOriginSubDomain(subDomain: string): string {
  const url = URL.parse(window.crossOriginUrl);
  assertTrue(url !== null);
  return `${url.protocol}//${subDomain}.${url.host}`;
}

function getCrossOriginUrl(url: string, useSubDomain = false): string {
  const baseUrl =
      useSubDomain ? getCrossOriginSubDomain('sub') : window.crossOriginUrl;
  assertTrue(baseUrl.length > 0);
  const resolved = URL.parse(url, baseUrl);
  assertTrue(resolved !== null);
  return resolved.href;
}

function getOrigin(url: string): string {
  return URL.parse('/', url)!.href;
}

async function navigateAndWaitForContentLoad(
    webview: SlimWebviewElement, url: string) {
  const contentLoadPromise = eventToPromise('contentload', webview);
  webview.src = url;
  await contentLoadPromise;
}

function evalOnWebview(
    webview: SlimWebviewElement, fn: Function, ...args: any[]): Promise<any> {
  return new Promise(function(resolve, reject) {
    const messageHandler = (e: MessageEvent) => {
      if (e.data.eval === undefined) {
        return;
      }
      window.removeEventListener('message', messageHandler);
      const data = e.data.eval;
      if (data.success) {
        resolve(data.result);
      } else {
        reject(data.result);
      }
    };
    window.addEventListener('message', messageHandler);
    webview.contentWindow!.postMessage({fn: fn.toString(), args}, '*');
  });
}

function denyPermissionRequest(
    webview: SlimWebviewElement, permission: string): Promise<any> {
  return new Promise(function(resolve, _reject) {
    webview.addEventListener('permissionrequest', function f(e: Event) {
      assertTrue(e instanceof PermissionRequestEvent);
      assertEquals(permission, e.permission);
      assertEquals(getOrigin(webview.src), getOrigin(e.request.url));
      e.request.deny();
      console.info(`Denied ${permission} permission request`);
      resolve(e);
    });
  });
}

suite('Loading', function() {
  test('SuccessfullLoadEvents', async function() {
    const webviewUrl = getTestUrl('/simple.html');

    const webview = document.createElement('webview');
    document.body.appendChild(webview);

    const loadStartPromise = eventToPromise<LoadEvent>('loadstart', webview);
    const loadCommitPromise = eventToPromise<LoadEvent>('loadcommit', webview);
    const contentLoadPromise = eventToPromise('contentload', webview);
    const loadStopPromise = eventToPromise('loadstop', webview);
    const loadAbortPromise = eventToPromise('loadabort', webview);

    webview.src = webviewUrl;

    const [loadStartEvent, loadCommitEvent] = await Promise.all([
      loadStartPromise,
      loadCommitPromise,
      contentLoadPromise,
      loadStopPromise,
    ]);

    assertEquals(webviewUrl, loadStartEvent.url);
    assertEquals(webviewUrl, loadCommitEvent.url);
    const loadAbortFulFilled = await isFulfilled(loadAbortPromise);
    assertFalse(loadAbortFulFilled);
  });

  test('FailedLoadEvents', async function() {
    const webviewUrl = 'http://does.not.exist/';

    const webview = document.createElement('webview');
    document.body.appendChild(webview);

    const loadStartPromise = eventToPromise<LoadEvent>('loadstart', webview);
    const loadAbortPromise =
        eventToPromise<LoadAbortEvent>('loadabort', webview);
    // The remaining events are still fired for the error page.
    const loadCommitPromise = eventToPromise<LoadEvent>('loadcommit', webview);
    const contentLoadPromise = eventToPromise('contentload', webview);
    const loadStopPromise = eventToPromise('loadstop', webview);

    webview.src = webviewUrl;

    const [loadStartEvent, loadAbortEvent, loadCommitEvent] =
        await Promise.all([
          loadStartPromise,
          loadAbortPromise,
          loadCommitPromise,
          contentLoadPromise,
          loadStopPromise,
        ]);

    assertEquals(webviewUrl, loadStartEvent.url);
    assertEquals(webviewUrl, loadAbortEvent.url);
    // The error network error varies depending on whether the device is online
    // or offline.
    assertTrue([-105, -106].includes(loadAbortEvent.code));
    assertTrue(['ERR_NAME_NOT_RESOLVED', 'ERR_INTERNET_DISCONNECTED'].includes(
        loadAbortEvent.reason));
    assertEquals(webviewUrl, loadCommitEvent.url);
  });

  // NOTE: We do not test navigation to a disallowed origin here.
  // In slim_web_view_page_handler.cc, a disallowed navigation triggers
  // mojo::ReportBadMessage, which terminates the renderer process.
  // In WebUI unit tests, this would cause the test itself to fail due to
  // process crash, making it difficult to test directly in this suite.

  test('AllowedOriginNavigation', async function() {
    const webviewUrl = getTestUrl('/simple.html');
    const origin = getOrigin(webviewUrl);

    const webview = document.createElement('webview');
    webview.allowedOriginsParams =
        new OriginCheckParams(['main_frame'], [origin]);
    document.body.appendChild(webview);

    const loadStartPromise = eventToPromise<LoadEvent>('loadstart', webview);
    const loadCommitPromise = eventToPromise<LoadEvent>('loadcommit', webview);
    const contentLoadPromise = eventToPromise('contentload', webview);
    const loadStopPromise = eventToPromise('loadstop', webview);

    webview.src = webviewUrl;

    const [loadStartEvent, loadCommitEvent] = await Promise.all([
      loadStartPromise,
      loadCommitPromise,
      contentLoadPromise,
      loadStopPromise,
    ]);

    assertEquals(webviewUrl, loadStartEvent.url);
    assertEquals(webviewUrl, loadCommitEvent.url);
  });

  test('InvalidAllowedOriginPatternFailsCreation', async function() {
    const webview = document.createElement('webview');
    // An invalid pattern that should fail parsing in SimpleUrlPatternMatcher.
    webview.allowedOriginsParams =
        new OriginCheckParams(['main_frame'], ['invalid pattern']);

    const failurePromise = new Promise<void>((resolve) => {
      window.addEventListener('unhandledrejection', function listener(e) {
        if (e.reason.message &&
            e.reason.message.includes('Failed to create guest')) {
          e.preventDefault();
          window.removeEventListener('unhandledrejection', listener);
          resolve();
        }
      });
    });

    document.body.appendChild(webview);
    webview.src = getTestUrl('/simple.html');

    await failurePromise;
  });
});

suite('Operations', function() {
  let webview: SlimWebviewElement;

  setup(async function() {
    webview = document.createElement('webview');
    document.body.appendChild(webview);
    await navigateAndWaitForContentLoad(
        webview, getTestUrl('/webui/guest_view_shared/eval_post_message.html'));
  });

  test('NewWindowEvents', async function() {
    const newWindowPromise =
        eventToPromise<NewWindowEvent>('newwindow', webview);
    evalOnWebview(webview, () => {
      window.open('http://foo.bar/', '_blank', 'width=200,height=300');
    });
    const newWindowEvent = await newWindowPromise;
    assertEquals('http://foo.bar/', newWindowEvent.targetUrl);
    assertEquals('new_popup', newWindowEvent.windowOpenDisposition);
    assertEquals(200, newWindowEvent.initialWidth);
    assertEquals(300, newWindowEvent.initialHeight);
  });

  // The following tests are for permission events. We only test the permission
  // denied case for simplicity, as the focus of the test is that the embedder
  // can allow or deny the permission, without requiring a prompt.

  test('GeolocationPermissionEventDeniedByEmbedder', async function() {
    if (!window.canGetLocation) {
      console.warn(
          'Skipping geolocation test because system location is disabled');
      this.skip();
    }
    const requestDeniedPromise = denyPermissionRequest(webview, 'geolocation');

    const geolocationError = await evalOnWebview(webview, async () => {
      return await new Promise((resolve, reject) => {
        navigator.geolocation.getCurrentPosition(
            () => {
              reject(new Error(
                  'Geolocation permission request should have been denied'));
            },
            (error: GeolocationPositionError) => {
              // A GeolocationPositionError can't be cloned, so copy the
              // properties into a plain object.
              resolve({code: error.code, message: error.message});
            });
      });
    });

    assertEquals(1, geolocationError.code);
    assertTrue(await isFulfilled(requestDeniedPromise));
  });

  test('MediaPermissionEventDeniedByEmbedder', async function() {
    const requestDeniedPromise = denyPermissionRequest(webview, 'media');

    const mediaError = await evalOnWebview(webview, async () => {
      try {
        await navigator.mediaDevices.getUserMedia({audio: true});
        throw new Error('Media permission request should have been denied');
      } catch (error) {
        return error;
      }
    });

    assertEquals('NotAllowedError', mediaError.name);
    assertTrue(await isFulfilled(requestDeniedPromise));
  });

  test('DownloadPermissionEventDeniedByEmbedder', async function() {
    const requestDeniedPromise = denyPermissionRequest(webview, 'download');

    evalOnWebview(webview, () => {
      const a = document.createElement('a');
      a.href = '/download-file';
      a.download = 'file.txt';
      document.body.appendChild(a);
      a.click();
    });

    const event = await requestDeniedPromise;
    assertEquals(getTestUrl('/download-file'), event.request.url);
  });
});

suite('Requests', function() {
  test('NavigateToDisallowedOriginFails', async function() {
    const webview = document.createElement('webview');
    const origin = getOrigin(getTestUrl('/'));
    // Only allow the test origin.
    webview.allowedOriginsParams =
        new OriginCheckParams(['main_frame'], [origin]);
    document.body.appendChild(webview);

    await navigateAndWaitForContentLoad(
        webview, getTestUrl('/webui/guest_view_shared/eval_post_message.html'));

    const loadAbortPromise =
        eventToPromise<LoadAbortEvent>('loadabort', webview);

    // Trigger a navigation to a DISALLOWED origin via window.location.
    evalOnWebview(webview, (url: string) => {
      window.location.href = url;
    }, getCrossOriginUrl('/simple.html'));

    const loadAbortEvent = await loadAbortPromise;

    assertEquals('ERR_BLOCKED_BY_CLIENT', loadAbortEvent.reason);
  });

  test('FetchAllowedCrossOriginSubDomainSucceeds', async function() {
    const webview = document.createElement('webview');
    const subDomainCrossOriginPattern = getCrossOriginSubDomain('*');
    console.info('subDomainCrossOriginPattern: ', subDomainCrossOriginPattern);
    webview.allowedOriginsParams = new OriginCheckParams(
        ['xmlhttprequest'],
        [window.testServerUrl, subDomainCrossOriginPattern]);
    document.body.appendChild(webview);

    await navigateAndWaitForContentLoad(
        webview, getTestUrl('webui/guest_view_shared/eval_post_message.html'));

    const fetchResult = await evalOnWebview(webview, async (url: string) => {
      try {
        await fetch(url);
        return {success: true};
      } catch (error: any) {
        return {success: false};
      }
    }, getCrossOriginUrl('/capture-headers', /*useSubDomain=*/ true));

    assertTrue(fetchResult.success);
  });

  test('FetchDisallowedOriginFails', async function() {
    const webview = document.createElement('webview');
    webview.allowedOriginsParams =
        new OriginCheckParams(['xmlhttprequest'], [window.testServerUrl]);
    document.body.appendChild(webview);

    await navigateAndWaitForContentLoad(
        webview, getTestUrl('webui/guest_view_shared/eval_post_message.html'));

    const fetchResult = await evalOnWebview(webview, async (url: string) => {
      try {
        await fetch(url);
        return {success: true};
      } catch (error: any) {
        return {success: false, error: error.name};
      }
    }, getCrossOriginUrl('/capture-headers'));

    assertFalse(fetchResult.success);
    // The fetch API wraps most errors in a generic `TypeError`, so there is no
    // more specific error to check for.
    // However, we don't need to worry about name resolution or network errors,
    // as the test is configured to resolve the above hostname to 127.0.0.1.
    assertEquals('TypeError', fetchResult.error);
  });

  test('HeadersConfigured', async function() {
    const webviewUrl =
        getTestUrl('/webui/guest_view_shared/eval_post_message.html');
    const webview = document.createElement('webview');

    webview.onBeforeSendHeadersParams = new OnBeforeSendHeadersParams(
        ['xmlhttprequest', 'main_frame'], false,
        [{name: 'X-Test-Header', value: 'test-value'}]);

    document.body.appendChild(webview);

    await navigateAndWaitForContentLoad(webview, webviewUrl);

    // Let the guest perform a fetch request and retrieve the captured headers.
    // We fetch from the guest context because the embedder WebUI has a strict
    // CSP that blocks connections to the local HTTP test server.
    const capturedHeaders = await evalOnWebview(webview, async () => {
      const response = await fetch('/capture-headers');
      return await response.json();
    });

    const mainFrameHeaders =
        capturedHeaders['/webui/guest_view_shared/eval_post_message.html'];
    assertTrue(!!mainFrameHeaders);
    assertEquals('test-value', mainFrameHeaders['X-Test-Header']);

    const fetchHeaders = capturedHeaders['/capture-headers'];
    assertTrue(!!fetchHeaders);
    assertEquals('test-value', fetchHeaders['X-Test-Header']);
  });

  test('InsecureHeadersFail', async function() {
    const webview = document.createElement('webview');

    webview.onBeforeSendHeadersParams = new OnBeforeSendHeadersParams(
        ['main_frame'], false, [{name: 'Cookie', value: 'test-cookie'}]);

    const failurePromise = new Promise<void>((resolve) => {
      window.addEventListener('unhandledrejection', function listener(e) {
        if (e.reason.message &&
            e.reason.message.includes('Failed to create guest')) {
          e.preventDefault();
          window.removeEventListener('unhandledrejection', listener);
          resolve();
        }
      });
    });

    document.body.appendChild(webview);
    webview.src = getTestUrl('/simple.html');

    await failurePromise;
  });
});
