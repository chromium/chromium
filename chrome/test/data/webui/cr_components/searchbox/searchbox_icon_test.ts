// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/new_tab_page.js';

import {createAutocompleteMatch, SearchboxBrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import type {SearchboxIconElement} from 'chrome://new-tab-page/new_tab_page.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSearchboxBrowserProxy} from './test_searchbox_browser_proxy.js';

suite('CrComponentsSearchboxIconTest', () => {
  let icon: SearchboxIconElement;
  let testProxy: TestSearchboxBrowserProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);

    icon = document.createElement('cr-searchbox-icon');
    icon.match = createAutocompleteMatch();
    document.body.appendChild(icon);
  });

  test('ImageShownOnLoad', async () => {
    const match = createAutocompleteMatch();
    match.imageUrl = '#';
    icon.match = match;

    await microtasksFinished();

    const image = icon.$.image;
    assertTrue(!!image);
    const loadPromise = eventToPromise('load', image);
    image.dispatchEvent(new Event('load'));

    await loadPromise;

    assertTrue(isVisible(image));
  });

  test('ImageHiddenOnError', async () => {
    const match = createAutocompleteMatch();
    match.imageUrl = '#';
    icon.match = match;

    await microtasksFinished();

    const image = icon.$.image;
    assertTrue(!!image);
    const errorPromise = eventToPromise('error', image);
    image.dispatchEvent(new Event('error'));

    await errorPromise;

    assertFalse(isVisible(image));
  });
});
