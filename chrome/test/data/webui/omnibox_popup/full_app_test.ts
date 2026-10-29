// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';

import type {OmniboxFullAppElement} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('FullAppTest', function() {
  let app: OmniboxFullAppElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('omnibox-full-app');
    document.body.appendChild(app);
  });

  test('ContextMenuNotPrevented', async function() {
    const whenFired = eventToPromise('contextmenu', document.documentElement);
    document.documentElement.dispatchEvent(
        new Event('contextmenu', {cancelable: true}));
    const e = await whenFired;
    assertFalse(e.defaultPrevented);
  });

  test('MousedownPreventedOnlyWhenNothingFocusable', function() {
    function mousedown(target: EventTarget): boolean {
      const e = new MouseEvent(
          'mousedown', {bubbles: true, cancelable: true, composed: true});
      target.dispatchEvent(e);
      return e.defaultPrevented;
    }

    // Non-focusable targets (e.g. the transparent shadow margin or the
    // searchbox padding) would clear focus, so the default is prevented.
    assertTrue(mousedown(document.body));
    const plainDiv = document.createElement('div');
    document.body.appendChild(plainDiv);
    assertTrue(mousedown(plainDiv));

    // Natively focusable elements keep native handling.
    const input = document.createElement('input');
    const button = document.createElement('button');
    document.body.append(input, button);
    assertFalse(mousedown(input));
    assertFalse(mousedown(button));

    // Descendants of elements with a tabindex (including -1, e.g. dropdown
    // matches under virtual focus) keep native handling too.
    const focusableDiv = document.createElement('div');
    focusableDiv.tabIndex = -1;
    const child = document.createElement('span');
    focusableDiv.appendChild(child);
    document.body.appendChild(focusableDiv);
    assertFalse(mousedown(child));

    plainDiv.remove();
    input.remove();
    button.remove();
    focusableDiv.remove();
  });

  test('AdjustsSizeOnVoicePermissionChanged', async function() {
    await microtasksFinished();
    const searchbox = app.shadowRoot.querySelector('omnibox-popup-searchbox')!;
    assertTrue(!!searchbox);

    // Simulate permission prompt opening with non-zero dimensions.
    searchbox.fire('voice-permission-changed', {
      isOpened: true,
      height: 220,
      width: 250,
    });
    await microtasksFinished();

    assertTrue(searchbox.classList.contains('permission-prompt-showing'));
    assertEquals(
        '220px',
        searchbox.style.getPropertyValue('--cr_composebox_minimum_height'));
    assertEquals(
        '250px',
        searchbox.style.getPropertyValue('--cr_composebox_minimum_width'));
    assertEquals('220px', window.getComputedStyle(searchbox).minHeight);
    assertEquals('250px', window.getComputedStyle(searchbox).minWidth);
    assertEquals(220, Math.round(searchbox.getBoundingClientRect().height));

    // Simulate permission prompt closing.
    searchbox.fire('voice-permission-changed', {
      isOpened: false,
      height: 0,
      width: 0,
    });
    await microtasksFinished();

    assertFalse(searchbox.classList.contains('permission-prompt-showing'));
    assertEquals(
        '', searchbox.style.getPropertyValue('--cr_composebox_minimum_height'));
    assertEquals(
        '', searchbox.style.getPropertyValue('--cr_composebox_minimum_width'));
  });
});
