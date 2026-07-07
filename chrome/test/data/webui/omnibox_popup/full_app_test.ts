// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';

import type {OmniboxFullAppElement} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {assertFalse} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('FullAppTest', function() {
  let app: OmniboxFullAppElement;

  setup(() => {
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
});
