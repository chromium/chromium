// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/composebox_favicon_group.js';

import type {ComposeboxFaviconGroupElement} from 'chrome://resources/cr_components/composebox/composebox_favicon_group.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('ComposeboxFaviconGroupTest', () => {
  let element: ComposeboxFaviconGroupElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('composebox-favicon-group');
    document.body.appendChild(element);
    await element.updateComplete;
  });

  test(
    'Render favicon group with counter if more than three tabs added',
    async () => {
    const tabs = [
      {url: 'https://google.com'},
      {url: 'https://youtube.com'},
      {url: 'https://gmail.com'},
      {url: 'https://maps.google.com'},
    ];
        // Cast to any to avoid mocking full TabInfo.
        element.tabs = tabs as any;
    await element.updateComplete;

    const items = element.shadowRoot.querySelectorAll('.favicon-item');
        // Should show 3 favicon circles. (The +1 counter is checked separately below).
    assertEquals(3, items.length);

    const counter = element.shadowRoot.querySelector('#more-items');
    assertTrue(!!counter);
    assertEquals('+1', counter.textContent.trim());
  });

  test('Render tab favicons without counter if below the limit', async () => {
    const tabs = [
      {url: 'https://google.com'},
      {url: 'https://youtube.com'},
    ];
    element.tabs = tabs as any;
    await element.updateComplete;

    const items = element.shadowRoot.querySelectorAll('.favicon-item');
    assertEquals(2, items.length);

    const counter = element.shadowRoot.querySelector('#more-items');
    assertFalse(!!counter);
  });

  test(
      'Prevents internal z-indexes from overlapping external flyouts',
      async () => {
        // Populate with multiple tabs to trigger overlapping coin elements
        // along with the overflow counter badge.
        element.tabs = [
          {url: 'https://google.com'},
          {url: 'https://youtube.com'},
          {url: 'https://gmail.com'},
          {url: 'https://maps.google.com'},
        ] as any;
        await element.updateComplete;

        // Position container explicitly at top-left of the viewport with
        // fixed dimensions to establish deterministic coordinates.
        element.style.position = 'absolute';
        element.style.top = '0px';
        element.style.left = '0px';
        element.style.width = '120px';
        element.style.height = '40px';

        // Create an external floating menu overlay overlapping the favicon
        // coins area to simulate an expanded dropdown.
        const flyoutOverlay = document.createElement('div');
        flyoutOverlay.id = 'simulated-flyout';
        flyoutOverlay.style.position = 'fixed';
        flyoutOverlay.style.top = '0px';
        flyoutOverlay.style.left = '0px';
        flyoutOverlay.style.width = '200px';
        flyoutOverlay.style.height = '200px';
        flyoutOverlay.style.zIndex = '1';
        flyoutOverlay.style.backgroundColor = 'rgba(255, 255, 255, 0.95)';
        document.body.appendChild(flyoutOverlay);

        // Perform visual hit testing across representative grid points.
        // The favicon container creates an isolated stacking context, so child
        // z-indexes remain trapped inside and cannot pierce overlays.
        const hitTestCoordinates = [
          {x: 8, y: 10},
          {x: 20, y: 10},
          {x: 35, y: 10},
        ];
        for (const point of hitTestCoordinates) {
          const topElement = document.elementFromPoint(point.x, point.y);
          assertEquals(
              flyoutOverlay, topElement,
              `Overlay pierced at (${point.x}, ${point.y})`);
        }

        // Verify fallback behavior when the external overlay is hidden.
        // Hit testing at identical coordinates should reach the favicon group.
        flyoutOverlay.style.display = 'none';
        const fallbackHit = document.elementFromPoint(8, 10);
        assertTrue(
            fallbackHit !== null &&
                (fallbackHit === element || element.contains(fallbackHit) ||
                 (element.shadowRoot?.contains(fallbackHit) ?? false)),
            'Hit test failed to reach underlying favicon group');

        flyoutOverlay.remove();
      });

  test('Submitted tabs do not subscribe to live tab load updates', async () => {
    let fired = false;
    element.addEventListener('wait-for-tab-load', () => {
      fired = true;
    });

    element.submittedTabIds = new Set([1]);
    element.tabs = [
      {tabId: 1, url: 'https://google.com'},
    ] as any;
    await element.updateComplete;

    assertFalse(fired);
  });

  test('Retain original favicon for submitted tabs', async () => {
    element.submittedTabIds = new Set([1]);
    element.tabs = [
      {tabId: 1, url: 'https://google.com'},
    ] as any;
    await element.updateComplete;

    const internalElement = element as any;
    internalElement.loadedFavicons_ =
        new Map([[1, 'url("data:image/png;base64,wikipedia")']]);

    const faviconUrl = internalElement.getFaviconUrl_(element.tabs[0]);

    assertFalse(faviconUrl.includes('data:image/png;base64,wikipedia'));
    assertTrue(faviconUrl.includes('chrome://favicon2/'));
    assertTrue(faviconUrl.includes('pageUrl=https%3A%2F%2Fgoogle.com'));
  });
});
