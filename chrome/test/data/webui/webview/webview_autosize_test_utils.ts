// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helpers for testing <webview> autosize from a WebUI embedder. Autosize
// constraints and sizes are in the embedder's CSS px. The guest's zoom matches
// the embedder's (see kGuestViewMatchEmbedderZoomAfterNavigation), so these are
// also the guest's CSS px.
//
// These don't depend on mocha, so C++ tests can also import them and run test
// steps with EvalJs, acting (e.g. zooming the embedder) in between.

// A [width, height] pair.
export type Size = [number, number];

// Fails with `label` if `promise` doesn't settle within `ms`, so a stall
// reports which step it's in rather than timing out the whole test.
function withTimeout<T>(
    promise: Promise<T>, label: string, ms = 10000): Promise<T> {
  let timeoutId = 0;
  const timeout = new Promise<never>((_resolve, reject) => {
    timeoutId = setTimeout(
        () => reject(new Error(`Timed out waiting for ${label}`)), ms);
  });
  return Promise.race([promise, timeout])
      .finally(() => clearTimeout(timeoutId));
}

// Evaluates `fn(...args)` in the guest, which must have loaded
// chrome/test/data/webui/guest_view_shared/eval_post_message.html, and
// resolves with the result.
function evalInGuest<T>(
    webview: chrome.webviewTag.WebView, label: string, fn: Function,
    ...args: any[]): Promise<T> {
  const result = new Promise<T>((resolve, reject) => {
    const onMessage = (e: MessageEvent) => {
      if (e.data.eval === undefined) {
        return;
      }
      window.removeEventListener('message', onMessage);
      if (e.data.eval.success) {
        resolve(e.data.eval.result);
      } else {
        reject(e.data.eval.result);
      }
    };
    window.addEventListener('message', onMessage);
    webview.contentWindow!.postMessage({fn: fn.toString(), args}, '*');
  });
  return withTimeout(result, label);
}

// Fills the guest with a block of exactly `width` x `height` guest CSS px.
export function setGuestContentSize(
    webview: chrome.webviewTag.WebView, width: number,
    height: number): Promise<void> {
  return evalInGuest<void>(
      webview,
      'the guest content to be set', (width: number, height: number) => {
        document.documentElement.style.overflow = 'hidden';
        document.body.style.margin = '0';
        document.body.innerHTML =
            `<div style="width:${width}px;height:${height}px"></div>`;
      }, width, height);
}

// Returns the guest's viewport size, in guest CSS px.
export function getGuestSize(webview: chrome.webviewTag.WebView):
    Promise<Size> {
  return evalInGuest(
      webview, 'the guest size', () => [window.innerWidth, window.innerHeight]);
}

// Returns the number of embedder CSS px per guest CSS px.
export async function getGuestToEmbedderScale(
    webview: chrome.webviewTag.WebView): Promise<number> {
  const guestDevicePixelRatio = await evalInGuest<number>(
      webview, 'the guest device pixel ratio', () => window.devicePixelRatio);
  return guestDevicePixelRatio / window.devicePixelRatio;
}

// Returns the <webview>'s size, in embedder CSS px.
export function getWebviewSize(webview: chrome.webviewTag.WebView): Size {
  const rect = webview.getBoundingClientRect();
  return [rect.width, rect.height];
}

// Allows for rounding to whole physical pixels.
export function isClose(expected: Size, actual: Size): boolean {
  return Math.abs(expected[0] - actual[0]) <= 1 &&
      Math.abs(expected[1] - actual[1]) <= 1;
}

// Waits (up to a limit) for `condition` to hold, so that a failing check
// afterwards reports the size things settled on.
export async function waitFor(condition: () => boolean | Promise<boolean>) {
  const deadline = performance.now() + 5000;
  while (!(await condition()) && performance.now() < deadline) {
    await new Promise(resolve => setTimeout(resolve, 50));
  }
}

// Creates an autosizing <webview> with the given constraints, in embedder CSS
// px, and loads `url` in it.
export async function createAutosizeWebview(
    url: string, minWidth: number, minHeight: number, maxWidth: number,
    maxHeight: number): Promise<chrome.webviewTag.WebView> {
  const webview =
      document.createElement('webview') as chrome.webviewTag.WebView;
  webview.style.display = 'block';
  webview.setAttribute('autosize', 'true');
  webview.setAttribute('minwidth', String(minWidth));
  webview.setAttribute('minheight', String(minHeight));
  webview.setAttribute('maxwidth', String(maxWidth));
  webview.setAttribute('maxheight', String(maxHeight));
  const loaded = new Promise<void>((resolve, reject) => {
    webview.addEventListener('loadstop', () => resolve(), {once: true});
    webview.addEventListener('loadabort', () => reject(), {once: true});
  });
  webview.src = url;
  document.body.appendChild(webview);
  await withTimeout(loaded, 'the guest to load');
  return webview;
}

// Waits for the guest's zoom to match the embedder's, so that one guest CSS px
// is exactly one embedder CSS px. Resolves with an empty string on success, or
// a description of the mismatch otherwise.
export async function waitForGuestToMatchEmbedderZoom(
    webview: chrome.webviewTag.WebView): Promise<string> {
  let scale = 0;
  await waitFor(async () => {
    scale = await getGuestToEmbedderScale(webview);
    return scale === 1;
  });
  return scale === 1 ? '' : `expected guest to embedder scale 1, got ${scale}`;
}

// Waits for the guest to be limited to `width` x `height` guest CSS px.
// Resolves with an empty string on success, or a description of the mismatch
// otherwise.
export async function waitForGuestSize(
    webview: chrome.webviewTag.WebView, width: number,
    height: number): Promise<string> {
  const expected: Size = [width, height];
  let actual: Size = [0, 0];
  await waitFor(async () => {
    actual = await getGuestSize(webview);
    return isClose(expected, actual);
  });
  return isClose(expected, actual) ?
      '' :
      `expected guest size ${JSON.stringify(expected)}, got ${
          JSON.stringify(actual)}`;
}
