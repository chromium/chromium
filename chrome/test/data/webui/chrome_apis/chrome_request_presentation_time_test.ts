// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

// Lets any frame requested by earlier work go out, then waits a task so that
// no main frame is pending when the next test step runs.
async function waitForIdle(): Promise<void> {
  await new Promise(resolve => requestAnimationFrame(resolve));
  await new Promise(resolve => requestAnimationFrame(resolve));
  await new Promise(resolve => setTimeout(resolve, 0));
}

let markerCount = 0;

// Paints a visible change: a fixed-position box at a new position each time,
// so the resulting frame always has damage. A mutation with no visible effect
// is dropped by the compositor as a no-damage frame and yields null.
function paintMarker(): HTMLElement {
  const box = document.createElement('div');
  box.style.cssText = 'position:fixed;top:0;width:40px;height:40px;' +
      `background:red;z-index:2147483647;left:${++markerCount * 50}px`;
  document.body.appendChild(box);
  return box;
}

suite('ChromeRequestPresentationTimeTest', () => {
  test('ResolvesAfterDomChange', async () => {
    await waitForIdle();
    const before = chrome.timeTicks.nowInMicroseconds();

    // Mutate the DOM in the same task as the request so a frame is produced.
    const marker = paintMarker();
    const presented = await chrome.requestPresentationTime();
    marker.remove();

    // The page is visible and a frame carrying the mutation is pending, so a
    // real presentation timestamp (not null) is required here. It must be a
    // TimeTicks value not earlier than the request.
    assertNotEquals(null, presented);
    assertEquals(typeof presented, 'bigint');
    assertTrue(presented! >= before);
  });

  test('IsIdempotentWithinTask', async () => {
    await waitForIdle();

    // Multiple requests in the same task observe the same frame.
    paintMarker();
    const [a, b] = await Promise.all([
      chrome.requestPresentationTime(),
      chrome.requestPresentationTime(),
    ]);
    assertEquals(typeof a, 'bigint');
    assertEquals(a, b);
  });

  test('NullWhenFrameHasNoUpdate', async () => {
    await waitForIdle();

    // An empty requestAnimationFrame callback requests a main frame, so the
    // request is queued, but the frame produces no visual update and is never
    // swapped. The compositor reports that as a did-not-swap, which must
    // surface as null rather than as a presentation timestamp.
    requestAnimationFrame(() => {});
    assertEquals(null, await chrome.requestPresentationTime());
  });

  test('SettlesWhenIdle', async () => {
    await waitForIdle();

    // With no DOM change and therefore no frame pending, the API must not
    // wait for an unrelated future frame. It should resolve (normally with
    // null) well within this bound. A bigint is tolerated in case something
    // else on the page dirtied it concurrently.
    const timeout = new Promise<'timeout'>(
        resolve => setTimeout(() => resolve('timeout'), 5000));
    const result =
        await Promise.race([chrome.requestPresentationTime(), timeout]);
    assertTrue(
        result !== 'timeout',
        'requestPresentationTime() did not settle on an idle page');
  });
});
