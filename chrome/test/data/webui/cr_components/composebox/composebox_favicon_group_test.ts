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
});
