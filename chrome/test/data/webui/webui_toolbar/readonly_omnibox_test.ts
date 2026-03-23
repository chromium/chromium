// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {type ReadonlyOmniboxElement} from 'chrome://webui-toolbar.top-chrome/readonly_omnibox.js';

suite('ReadonlyOmnibox', function() {
  let omnibox: ReadonlyOmniboxElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    omnibox = document.createElement('readonly-omnibox');
    document.body.appendChild(omnibox);
  });

  test('Setting text without selection', async () => {
    omnibox.omniboxViewState = {
      text: 'Hello',
      selection: null,
    };
    await microtasksFinished();
    assertEquals('Hello', omnibox.$.textContainer.textContent);
  });
});
