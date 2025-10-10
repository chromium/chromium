// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ActionChipsElement} from 'chrome://new-tab-page/lazy_load.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('NewTabPageActionChipsTest', () => {
  let chips: ActionChipsElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    chips = document.createElement('ntp-action-chips');
    document.body.append(chips);
    await microtasksFinished();
  });

  test('creates component', () => {
    assertTrue(!!chips);
  });
});
