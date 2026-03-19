// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//glic/shared/guest_view/slim_webview.js';
import '//glic/strings.m.js';

import {PermissionRequestEvent} from '//glic/shared/guest_view/slim_webview.js';
import type {SlimWebviewElement} from '//glic/shared/guest_view/slim_webview.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

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
  const baseUrl = loadTimeData.getString('glicGuestURL');
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
    webview: SlimWebviewElement, fn: Function, args: any[] = []): Promise<any> {
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
      assertEquals(getOrigin(webview.src), e.request.url);
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

    const loadStartPromise = eventToPromise('loadstart', webview);
    const loadCommitPromise = eventToPromise('loadcommit', webview);
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

    const loadStartPromise = eventToPromise('loadstart', webview);
    const loadAbortPromise = eventToPromise('loadabort', webview);
    // The remaining events are still fired for the error page.
    const loadCommitPromise = eventToPromise('loadcommit', webview);
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
    const newWindowPromise = eventToPromise('newwindow', webview);
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
});
