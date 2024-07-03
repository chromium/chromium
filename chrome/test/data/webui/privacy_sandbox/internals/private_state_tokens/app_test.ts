// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-internals/private_state_tokens/private_state_tokens.js';

import type {PrivateStateTokensAppElement} from 'chrome://privacy-sandbox-internals/private_state_tokens/private_state_tokens.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('PrivateStateTokensAppTest', () => {
  let app: PrivateStateTokensAppElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('private-state-tokens-app');
    document.body.appendChild(app);
  });

  test('check layout', async () => {
    await microtasksFinished();
    assertTrue(isVisible(app));
    // TODO(crbug.com/348590926): Add more tests later...
  });
});
