// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';

import type {OmniboxFullAppElement, OmniboxPopupSearchboxElement} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('FullAppTest', function() {
  let app: OmniboxFullAppElement;

  setup(() => {
    app = document.createElement('omnibox-full-app');
    document.body.appendChild(app);
  });

  test('ContextMenuPrevented', async function() {
    const whenFired = eventToPromise('contextmenu', document.documentElement);
    document.documentElement.dispatchEvent(
        new Event('contextmenu', {cancelable: true}));
    const e = await whenFired;
    assertTrue(e.defaultPrevented);
  });

  test('FocusesInputOnVisibilityChange', async function() {
    const searchbox =
        app.shadowRoot.querySelector<OmniboxPopupSearchboxElement>(
            'omnibox-popup-searchbox');
    assertTrue(!!searchbox);

    Object.defineProperty(document, 'visibilityState', {
      value: 'visible',
      writable: true,
      configurable: true,
    });
    document.dispatchEvent(new Event('visibilitychange'));
    await microtasksFinished();

    assertEquals(searchbox.shadowRoot.activeElement, searchbox.$.input);
  });
});
